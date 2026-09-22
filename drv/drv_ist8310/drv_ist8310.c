/**
 * @file drv_ist8310.c
 * @brief IST8310 三轴磁力计驱动实现
 *
 */

#include "drv_ist8310.h"
#include "app_cfg.h"

#ifdef DRV_IST8310_USED

#if defined(HAL_I2C_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED)

#include <string.h>
#include "bsp_dwt.h"
#include "bsp_log.h"

/* ist8310 日志实例（日志关闭时宏为空、不分配，BSPLOG 空宏不引用） */
#ifndef DRV_IST8310_LOG_LIMIT
#define DRV_IST8310_LOG_LIMIT 10
#endif // !DRV_IST8310_LOG_LIMIT
LOG_INSTANCE_DEF(g_ist8310_log, "drv_ist8310", DRV_IST8310_LOG_LIMIT);

/*============================ 时序与阈值 ============================*/

#define IST8310_DRDY_TIMEOUT_MS 20            // 轮询 STAT1.DRDY 的超时上限
#define IST8310_RECOVER_FAIL_TH 3             // 连续失败多少次触发恢复
#define IST8310_RECOVER_COOLDOWN_US 500000ull // 两次恢复之间的最小间隔 (500ms)
#define IST8310_PROBE_TRIALS 3                // I2CIsDeviceReady 探测重试次数
#define IST8310_WAI_RETRY_DELAY_S 0.001f      // WAI 重试间隔 (1ms)

/*============================ 转换常量 ============================*/

#define IST8310_I2C_ADDR I2C_DEV_ADDR(IST8310_I2C_ADDR_7BIT) // HAL 需要的 8 位形式地址

/*============================ 测量间隔查找表 ============================*/

/**
 * @brief 各平均档位下两次测量之间的最小等待时间 (ms)
 * @note 数据手册 §3.1.2：默认配置（4 次平均）最小间隔 5ms（200Hz）；
 *       §3.1.1：低噪声配置（16 次平均）最小间隔 6ms（166Hz）。
 *       中间档位手册未单独给出，取默认值 5ms —— 偏保守的那一侧（8 次平均
 *       实际快于 4 次不可能的，这里宁可按更慢的默认值等）。
 */
static const uint16_t s_meas_delay_ms[IST8310_AVG_NUM] = {
    [IST8310_AVG_NONE] = 5,
    [IST8310_AVG_2] = 5,
    [IST8310_AVG_4] = 5,
    [IST8310_AVG_8] = 5,
    [IST8310_AVG_16] = 6,
};

/*============================ 私有函数声明 ============================*/

/* BSP 子模块封装 */
static void IST8310_SetI2CMode(IST8310Instance *inst, I2C_Work_Mode_e mode);
static int8_t IST8310_WaitXfer(IST8310Instance *inst, uint32_t timeout_ms);
static int8_t IST8310_ReadReg(IST8310Instance *inst, uint8_t reg, uint8_t len);
static int8_t IST8310_WriteReg(IST8310Instance *inst, uint8_t reg, uint8_t data);
/* 器件操作 */
static void IST8310_ResetPulse(IST8310Instance *inst);
static int8_t IST8310_InitDeviceBlocking(IST8310Instance *inst);
static int8_t IST8310_InitDevice(IST8310Instance *inst);
static int8_t IST8310_ArmSingle(IST8310Instance *inst);
static int8_t IST8310_SampleBlocking(IST8310Instance *inst);
static int8_t IST8310_Recover(IST8310Instance *inst);
static void IST8310_PackFromRx(const uint8_t *raw, float *out);
/* 回调 */
static void IST8310_IntCallback(GPIOInstance *gpio_inst);
static void IST8310_I2CCpltCallback(I2CInstance *i2c_inst);
static void IST8310_I2CErrCallback(I2CInstance *i2c_inst);

/*============================ BSP 子模块封装 ============================*/

/**
 * @brief 切换 I2C 子实例的工作模式（只改 BSP 实例字段，不碰硬件）
 * @note I2CConfig 可重复调用，仅做字段赋值与句柄查找，切换代价可以忽略；
 *       回调只在异步模式下挂，阻塞模式挂回调没有意义且会引入
 *       "一次写完成被当成一次读完成"的误判。
 */
static void IST8310_SetI2CMode(IST8310Instance *inst, I2C_Work_Mode_e mode)
{
    I2C_Config_s cfg = {
        .i2c_e = inst->i2c_inst->i2c_e,
        .work_mode = mode,
        .rx_callback = (mode == I2C_BLOCK_MODE) ? NULL : IST8310_I2CCpltCallback,
        .err_callback = (mode == I2C_BLOCK_MODE) ? NULL : IST8310_I2CErrCallback,
    };
    (void)I2CConfig(inst->i2c_inst, &cfg);
}

/**
 * @brief 等一笔异步（IT/DMA）传输落地
 * @retval 0 成功；-1 失败或超时
 *
 * @note 本函数是忙等，**只能在任务上下文调用**：中断里 tick 与 DWT 都在走，
 *       但等的是另一个中断里的回调，跑在中断里就是死循环。
 * @note 超时只记账返回，不在这里做收尾：此时 HAL 的传输可能还在飞，
 *       驱动无法安全地中止它。留下的 xfer_done/xfer_error 脏值不会污染下一笔传输
 *       （ReadReg/WriteReg 每次发起前都会清零），真卡死的句柄由 fail_count 攒够后的
 *       IST8310_Recover → I2CBusRecover 重建。
 */
static int8_t IST8310_WaitXfer(IST8310Instance *inst, uint32_t timeout_ms)
{
    uint64_t start_us = DWT_GetTimeUs();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000ull;

    while (inst->xfer_done == 0)
    {
        if (inst->xfer_error != 0)
        {
            return -1;
        }
        if ((DWT_GetTimeUs() - start_us) > timeout_us)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Async xfer timeout (%dms), frame dropped", (int)timeout_ms);
            return -1;
        }
    }

    return (inst->xfer_error != 0) ? -1 : 0;
}

/**
 * @brief 读从机寄存器，结果在 inst->i2c_inst->rx_buff[0..len-1]
 * @retval 0 成功；-1 失败
 *
 * @note 对调用方而言阻塞/IT/DMA 三种模式**语义完全一致**：返回 0 时缓冲里一定是新数据。
 *       异步模式下本函数自己等回调（IST8310_WaitXfer），所以上层代码一行都不用改，
 *       差别只在传输期间 CPU 是空闲的。
 */
static int8_t IST8310_ReadReg(IST8310Instance *inst, uint8_t reg, uint8_t len)
{
    if (inst->i2c_inst->work_mode == I2C_BLOCK_MODE)
    {
        return I2CMemRead(inst->i2c_inst, IST8310_I2C_ADDR, reg,
                          I2C_MEM_ADDR_SIZE_8BIT, len, inst->i2c_timeout_ms);
    }

    /* 发起前清标志：否则上一笔残留的完成标记会让本笔立刻"成功"返回，
     * 调用方拿到的其实是上一次读剩下的旧缓冲 */
    inst->xfer_done = 0;
    inst->xfer_error = 0;

    if (I2CMemRead(inst->i2c_inst, IST8310_I2C_ADDR, reg,
                   I2C_MEM_ADDR_SIZE_8BIT, len, inst->i2c_timeout_ms) != 0)
    {
        return -1; /* 启动失败不会有完成回调，bsp_i2c 已复位句柄并回调了 err_callback */
    }

    return IST8310_WaitXfer(inst, inst->i2c_timeout_ms);
}

/**
 * @brief 写从机寄存器
 * @retval 0 成功；-1 失败
 *
 * @note data 是调用方的栈变量。阻塞模式下 HAL 同步用完即返回；
 *       异步模式下本函数会等到传输真正结束才返回，那时 HAL 早已不再持有该指针 ——
 *       所以传栈变量是安全的，「不能传栈变量」的约束不适用于本函数。
 */
static int8_t IST8310_WriteReg(IST8310Instance *inst, uint8_t reg, uint8_t data)
{
    if (inst->i2c_inst->work_mode == I2C_BLOCK_MODE)
    {
        return I2CMemWrite(inst->i2c_inst, IST8310_I2C_ADDR, reg,
                           I2C_MEM_ADDR_SIZE_8BIT, &data, 1, inst->i2c_timeout_ms);
    }

    inst->xfer_done = 0;
    inst->xfer_error = 0;

    if (I2CMemWrite(inst->i2c_inst, IST8310_I2C_ADDR, reg,
                    I2C_MEM_ADDR_SIZE_8BIT, &data, 1, inst->i2c_timeout_ms) != 0)
    {
        return -1;
    }

    return IST8310_WaitXfer(inst, inst->i2c_timeout_ms);
}

/*============================ 器件操作 ============================*/

/**
 * @brief RSTN 硬复位脉冲（低有效）
 * @note 假定板卡按常规接法（RSTN 低电平复位）。未接 RSTN 时本函数直接返回。
 * @note 复位后必须等满 POR（最大 50ms）才能访问寄存器。
 */
static void IST8310_ResetPulse(IST8310Instance *inst)
{
    if (!inst->has_rstn)
    {
        return;
    }

    GPIOReset(inst->rstn);
    DWT_Delay((float)IST8310_RSTN_PULSE_MS / 1000.0f);
    GPIOSet(inst->rstn);
    DWT_Delay((float)IST8310_POR_DELAY_MS / 1000.0f);
}

/**
 * @brief 器件初始化主体（调用方须已切到阻塞模式）
 * @retval 0 成功；-1 失败
 *
 * @note 序列依据数据手册 §3.1：
 *       RSTN 脉冲（可选）→ 等 POR → WAI 校验 → 软复位并等 SRST 自清 → 等 POR
 *       → PDCNTL（脉冲宽度，§3.1.1 要求初始写 Normal）→ AVGCNTL（平均次数）
 *       → CNTL2（DRDY 使能 + 极性）→ CNTL1（待机）→ 回读校验
 * @note **恢复流程必须整段重跑本函数**：软复位会把 PDCNTL/AVGCNTL/CNTL2
 *       全部恢复成复位默认值，只补写 CNTL1 是不够的。
 */
static int8_t IST8310_InitDeviceBlocking(IST8310Instance *inst)
{
    /* 1) 硬复位（可选）——有 RSTN 就用，保证从确定的初态开始；
     *    没有 RSTN 时只能等电源 POR 走完，靠下面的软复位兜底 */
    IST8310_ResetPulse(inst);
    if (!inst->has_rstn)
    {
        DWT_Delay((float)IST8310_POR_DELAY_MS / 1000.0f);
    }

    /* 2) WAI 校验（带重试）：确认总线上确实是 IST8310，而不是地址冲突的其他器件 */
    uint8_t wai = 0;
    for (uint8_t i = 0; i < IST8310_WAI_RETRY; i++)
    {
        wai = 0;
        if (IST8310_ReadReg(inst, IST8310_WAI_REG, 1) == 0)
        {
            wai = inst->i2c_inst->rx_buff[0];
            if (wai == IST8310_WAI_VALUE)
            {
                break;
            }
        }
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "WAI mismatch (got 0x%02X, want 0x%02X), retry %d/%d",
               wai, IST8310_WAI_VALUE, i + 1, IST8310_WAI_RETRY);
        DWT_Delay(IST8310_WAI_RETRY_DELAY_S);
    }
    if (wai != IST8310_WAI_VALUE)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "WAI check failed, got 0x%02X", wai);
        return -1;
    }

    /* 3) 软复位，并轮询等 SRST 自清 —— 自清即 POR 流程结束 */
    if (IST8310_WriteReg(inst, IST8310_CNTL2_REG, IST8310_CNTL2_SRST) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "SRST write failed");
        return -1;
    }

    uint64_t start_us = DWT_GetTimeUs();
    while (1)
    {
        if (IST8310_ReadReg(inst, IST8310_CNTL2_REG, 1) != 0)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "SRST poll read failed");
            return -1;
        }
        if ((inst->i2c_inst->rx_buff[0] & IST8310_CNTL2_SRST) == 0)
        {
            break;
        }
        if ((DWT_GetTimeUs() - start_us) > ((uint64_t)IST8310_SRST_TIMEOUT_MS * 1000ull))
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "SRST self-clear timeout (%dms)", IST8310_SRST_TIMEOUT_MS);
            return -1;
        }
    }
    DWT_Delay((float)IST8310_POR_DELAY_MS / 1000.0f);

    /* 4) 脉冲宽度：数据手册 §3.1.1 要求初始配置写 Normal(0xC0) */
    if (IST8310_WriteReg(inst, IST8310_PDCNTL_REG, (uint8_t)inst->pd_pulse) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "PDCNTL write failed");
        return -1;
    }

    /* 5) 平均次数：y 与 x/z 用同一档位（寄存器里是两份独立的 3bit 域） */
    {
        uint8_t avg_code = (uint8_t)inst->avg;
        if (avg_code >= IST8310_AVG_NUM)
        {
            avg_code = (uint8_t)IST8310_AVG_4;
        }
        uint8_t avg_reg = (uint8_t)((avg_code << 3) | avg_code);
        if (IST8310_WriteReg(inst, IST8310_AVGCNTL_REG, avg_reg) != 0)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "AVGCNTL write failed");
            return -1;
        }
    }

    /* 6) CNTL2：使能 DRDY（总开关）+ 按 Config 设极性 */
    uint8_t cntl2 = (uint8_t)IST8310_CNTL2_DREN;
    if (inst->drdy_polarity == IST8310_DRDY_ACTIVE_HIGH)
    {
        cntl2 |= (uint8_t)IST8310_CNTL2_DRP;
    }
    if (IST8310_WriteReg(inst, IST8310_CNTL2_REG, cntl2) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "CNTL2 write failed");
        return -1;
    }

    /* 7) 回到待机（软复位后的默认态，显式写一次让语义明确） */
    if (IST8310_WriteReg(inst, IST8310_CNTL1_REG, IST8310_CNTL1_MODE_STANDBY) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "CNTL1 standby write failed");
        return -1;
    }

    /* 8) 回读校验：DREN/DRP 可读，能证明前面的写确实落到了器件里 */
    if (IST8310_ReadReg(inst, IST8310_CNTL2_REG, 1) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "CNTL2 verify read failed");
        return -1;
    }
    if ((inst->i2c_inst->rx_buff[0] & (uint8_t)(IST8310_CNTL2_DREN | IST8310_CNTL2_DRP)) != cntl2)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "CNTL2 verify failed (wrote 0x%02X, read 0x%02X)",
               cntl2, inst->i2c_inst->rx_buff[0]);
        return -1;
    }

    return 0;
}

/**
 * @brief 器件初始化（自动在阻塞模式下进行，结束时恢复配置的传输模式）
 * @note 初始化序列本身必须阻塞：里面全是"写完马上读回校验"和轮询 SRST 自清，
 *       用异步还得逐笔等，纯属把简单流程复杂化。结束时统一恢复到 inst->i2c_mode。
 */
static int8_t IST8310_InitDevice(IST8310Instance *inst)
{
    IST8310_SetI2CMode(inst, I2C_BLOCK_MODE);
    int8_t ret = IST8310_InitDeviceBlocking(inst);
    IST8310_SetI2CMode(inst, inst->i2c_mode);
    return ret;
}

/**
 * @brief 触发一次单次测量（仅中断模式使用）
 * @retval 0 已触发（armed=1，等 DRDY）；-1 触发失败
 *
 * @note 为什么异步模式下要"临时切阻塞写完再切回"：
 *       IT/DMA 模式下的写完成会进 IST8310_I2CCpltCallback，而那里在中断模式下
 *       被设计成"收到一帧数据就发布"。若不加区分，一次 CNTL1 写完成会被误当成
 *       数据读完成，把上一次读残留的缓冲当新帧发布（时间戳还是错的）。
 *       临时切阻塞后写是同步的、不产生任何回调，语义最干净。
 *       本函数只在 !armed && !transfer_busy 时被调用，不存在在途传输被打断。
 * @note 触发写本身耗时不到 1ms，临时切回阻塞不影响 CPU 占用；
 *       真正占时间的"等测量 + 读六字节"仍走配置的 i2c_mode。
 */
static int8_t IST8310_ArmSingle(IST8310Instance *inst)
{
    /* 复用 transfer_busy 兼作"正在触发"锁：从写完 CNTL1 到器件真正出 DRDY
     * 之间有时序窗口，期间若来一个残留的 DRDY 边沿，EXTI 回调会因
     * transfer_busy=1 而把它丢掉，不会起一笔错位的读 */
    inst->transfer_busy = 1;
    IST8310_SetI2CMode(inst, I2C_BLOCK_MODE);

    int8_t ret = IST8310_WriteReg(inst, IST8310_CNTL1_REG, IST8310_CNTL1_MODE_SINGLE);

    IST8310_SetI2CMode(inst, inst->i2c_mode);

    inst->transfer_busy = 0;
    inst->armed = (ret == 0) ? 1 : 0;
    /* 记下"从哪一刻开始等 DRDY"，armed 超时看门狗（IST8310ReadInt）据此判断边沿是否丢了。
     * 只在触发成功时刷新：失败时 armed=0，看门狗本来就不看这个值 */
    if (ret == 0)
    {
        inst->arm_us = DWT_GetTimeUs();
    }

    if (ret != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Arm single measurement failed");
    }
    return ret;
}

/**
 * @brief 触发 → 等待 → 轮询 DRDY → 连读六字节（阻塞模式专用）
 * @retval 0 成功，原始数据在 inst->i2c_inst->rx_buff；-1 失败
 */
static int8_t IST8310_SampleBlocking(IST8310Instance *inst)
{
    if (IST8310_WriteReg(inst, IST8310_CNTL1_REG, IST8310_CNTL1_MODE_SINGLE) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Trigger single measurement failed");
        return -1;
    }

    DWT_Delay((float)inst->meas_delay_ms / 1000.0f);

    /* 轮询 STAT1.DRDY。注意：读 STAT1 **不会**清 DRDY，
     * 只有读输出数据寄存器或 STAT2 才会清，所以这里可以反复读 */
    uint64_t start_us = DWT_GetTimeUs();
    while (1)
    {
        if (IST8310_ReadReg(inst, IST8310_STAT1_REG, 1) != 0)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "STAT1 read failed");
            return -1;
        }
        if ((inst->i2c_inst->rx_buff[0] & IST8310_STAT1_DRDY) != 0)
        {
            break;
        }
        if ((DWT_GetTimeUs() - start_us) > ((uint64_t)IST8310_DRDY_TIMEOUT_MS * 1000ull))
        {
            /* 超时：DRDY 一直没置起。初始化已写入并回读校验过 CNTL2.DREN，正常情况下
             * 走不到这里；真走到就说明这一帧并没有准备好。
             * 仍然把输出寄存器读走一遍 —— 那是手册里唯一能拉低 DRDY 的途径，读掉能让
             * 器件回到干净状态；但这帧**不作为有效数据返回**：
             * 否则调用方拿到的是上一帧的旧磁场值，却配着刚生成的新时间戳，从外部完全
             * 看不出是旧数据（这比直接报一次读失败危险得多）。
             * 返回 -1 让调用方按失败处理，持续失败会走恢复链。 */
            BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "DRDY poll timeout (%dms), frame dropped",
                   IST8310_DRDY_TIMEOUT_MS);
            (void)IST8310_ReadReg(inst, IST8310_DATAXL_REG, IST8310_DATA_LEN);
            return -1;
        }
    }

    /* 0x03~0x08 地址自增，一次 START 取全三轴 */
    if (IST8310_ReadReg(inst, IST8310_DATAXL_REG, IST8310_DATA_LEN) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Data read failed");
        return -1;
    }

    return 0;
}

/**
 * @brief 失败恢复：外设级总线恢复 → 探测从机（必要时 RSTN 脉冲）→ 整段重跑初始化
 * @retval 0 成功；-1 失败或仍在冷却期
 * @note 含阻塞 I2C 与 DeInit/Init，**只能在任务上下文调用**。
 */
static int8_t IST8310_Recover(IST8310Instance *inst)
{
    uint64_t now = DWT_GetTimeUs();
    if (inst->last_recover_us != 0 && (now - inst->last_recover_us) < IST8310_RECOVER_COOLDOWN_US)
    {
        /* 冷却中：调用方每次失败都会来问一次，这里静默返回，避免刷屏 */
        return -1;
    }
    inst->last_recover_us = now;
    if (inst->recover_count < UINT8_MAX)
    {
        inst->recover_count++;
    }

    BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Recover start (count=%d, fail=%d)",
           inst->recover_count, inst->fail_count);

    /* 1) 外设级总线恢复（DeInit + Init + 复位句柄状态） */
    (void)I2CBusRecover(inst->i2c_inst);

    /* 2) 探测从机是否应答；不应答就先硬复位再试一次 */
    if (I2CIsDeviceReady(inst->i2c_inst, IST8310_I2C_ADDR, IST8310_PROBE_TRIALS, inst->i2c_timeout_ms) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Device not ready after bus recover");
        if (!inst->has_rstn)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "No RSTN wired, cannot reset device");
            return -1;
        }
        IST8310_ResetPulse(inst);
        if (I2CIsDeviceReady(inst->i2c_inst, IST8310_I2C_ADDR, IST8310_PROBE_TRIALS, inst->i2c_timeout_ms) != 0)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Device still not ready after RSTN pulse");
            return -1;
        }
    }

    /* 3) 整段重跑初始化（软复位会把 PDCNTL/AVGCNTL/CNTL2 全恢复默认） */
    if (IST8310_InitDevice(inst) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Re-init failed");
        return -1;
    }

    inst->fail_count = 0;
    inst->transfer_busy = 0;
    inst->armed = 0;
    /* 异步传输标志一并清干净：重建外设后旧标志已无意义，
     * 留着会让恢复后的第一笔读"立刻完成"并拿回旧缓冲 */
    inst->xfer_done = 0;
    inst->xfer_error = 0;

    /* 4) 中断模式补一次触发，把采集链接回去 */
    if (inst->work_mode == IST8310_MODE_INT && IST8310_ArmSingle(inst) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Re-arm after recover failed");
        return -1;
    }

    BSPLOG(&g_ist8310_log, LOG_LEVEL_INFO, "Recover done");
    return 0;
}

/*============================ 数据换算 ============================*/

/**
 * @brief 原始六字节 → 三轴磁感应强度 (µT)
 * @note 逐字节拼小端二进制补码，不对可能非对齐的缓冲做 int16 指针访问
 * @note 只按数据手册灵敏度 3.3 LSB/µT 换算，**不做零偏/硬铁软铁补偿**（归 drvlib）
 */
static void IST8310_PackFromRx(const uint8_t *raw, float *out)
{
    for (uint8_t i = 0; i < IST8310_AXIS_NUM; i++)
    {
        int16_t v = (int16_t)((uint16_t)raw[2 * i] | ((uint16_t)raw[2 * i + 1] << 8));
        out[i] = (float)v / IST8310_SENSITIVITY_LSB_PER_UT;
    }
}

/*============================ 中断模式回调 ============================*/

/**
 * @brief DRDY EXTI 回调（运行在中断上下文）
 * @note **只发起异步 I2C 读，绝不做阻塞 I2C**：HAL 的阻塞式 I2C 用
 *       HAL_GetTick 计时，而 tick 中断优先级低于本 EXTI，在中断里 tick 不前进，
 *       任何阻塞调用都会立刻超时（或永久卡住）。
 * @note 与 drv_bmi088 同一约定：新传输只从 EXTI 发起，不从完成回调里发起。
 */
static void IST8310_IntCallback(GPIOInstance *gpio_inst)
{
    IST8310Instance *inst = (IST8310Instance *)gpio_inst->parent;
    if (inst == NULL || inst->work_mode != IST8310_MODE_INT)
    {
        return;
    }

    /* 未触发就来 DRDY（复位残留、或上一次已处理完的边沿）：直接丢弃 */
    if (!inst->armed || inst->transfer_busy)
    {
        return;
    }

    inst->int_timestamp = DWT_GetTimeUs();
    inst->transfer_busy = 1;

    if (I2CMemRead(inst->i2c_inst, IST8310_I2C_ADDR, IST8310_DATAXL_REG,
                   I2C_MEM_ADDR_SIZE_8BIT, IST8310_DATA_LEN, inst->i2c_timeout_ms) != 0)
    {
        /* 启动失败不会有完成回调。I2CMemRead 内部已复位 BSP 句柄并回调了
         * 本驱动的 err_callback，这里再兜一次传输锁，双保险 */
        inst->transfer_busy = 0;
        inst->armed = 0;
    }
}

/**
 * @brief I2C 传输完成回调
 * @note 本回调**只做记账并发布数据，绝不发起新传输**（新传输由
 *       IST8310ReadInt 在任务上下文触发）。原因同 drv_bmi088：
 *       完成回调运行在 I2C 事件/DMA 中断里，此时 HAL 句柄状态尚未完全稳定，
 *       在里面发起下一笔传输容易撞上 HAL_BUSY 并把驱动永久卡在 BUSY。
 */
static void IST8310_I2CCpltCallback(I2CInstance *i2c_inst)
{
    IST8310Instance *inst = (IST8310Instance *)i2c_inst->parent;
    if (inst == NULL)
    {
        return;
    }

    /* 先记账：轮询模式下的 IT/DMA 传输靠它让 IST8310_WaitXfer 收尾 */
    inst->xfer_done = 1;

    /* 轮询模式：这笔传输是 IST8310_ReadReg/WriteReg 自己发起的，其中既有 STAT1
     * 这种中间读、也有 CNTL1 触发写，调用方在 WaitXfer 返回后自己去取 rx_buff。
     * 这里绝不能发布成磁力帧 —— 那会把状态寄存器的值当成磁场；也不能动 fail_count，
     * 否则一次 STAT1 读成功就能把前面真失败攒下的计数清零。 */
    if (inst->work_mode != IST8310_MODE_INT)
    {
        return;
    }

    /* 双缓冲发布：填另一个槽，最后一步切 latest_idx（单字节写入，原子）。
     * 任务侧读 frame[latest_idx] 时，本中断只会往另一个槽写，不会读到半帧 */
    uint8_t slot = (uint8_t)(inst->frame_wr ^ 1u);
    IST8310_PackFromRx(i2c_inst->rx_buff, inst->frame[slot].mag);
    inst->frame[slot].time_stamp = inst->int_timestamp;
    inst->frame_wr = slot;
    inst->latest_idx = slot;
    inst->frame_valid = 1;

    inst->transfer_busy = 0;
    inst->armed = 0; /* 本帧已收完，等任务重新触发下一次测量 */
    inst->fail_count = 0;

    /* 喂狗 */
    DaemonReload(inst->daemon);
}

/**
 * @brief I2C 传输错误回调（bsp_i2c 在传输未能正常发起或 HAL 报错时调用）
 * @note 这类失败不会再有完成回调，必须在这里复位传输状态，
 *       否则 transfer_busy/armed 恒为 1，驱动永久失联。
 * @note 真正的恢复（DeInit/Init + 阻塞 I2C）留给任务上下文，
 *       这里**只置请求位**：本回调可能运行在中断里。
 */
static void IST8310_I2CErrCallback(I2CInstance *i2c_inst)
{
    IST8310Instance *inst = (IST8310Instance *)i2c_inst->parent;
    if (inst == NULL)
    {
        return;
    }

    /* 先记账：轮询模式下调用方靠它知道这笔传输没成功（I2CMemRead 那头也会返回 -1，
     * 两条路都指向同一个结论） */
    inst->xfer_error = 1;

    if (inst->work_mode != IST8310_MODE_INT)
    {
        /* 轮询模式的失败计数与恢复统一由 IST8310ReadBlocking 管，
         * 这里再计一次会变成"一次失败记两笔" */
        return;
    }

    inst->transfer_busy = 0;
    inst->armed = 0; /* 丢弃本帧，等任务重新触发 */

    if (inst->fail_count < UINT8_MAX)
    {
        inst->fail_count++;
    }
    if (inst->fail_count >= IST8310_RECOVER_FAIL_TH)
    {
        inst->recover_request = 1;
    }
}

/*============================ 公开接口实现 ============================*/

/**
 * @brief 注册 IST8310 实例（仅调用一次）
 * @note 注册 I2C/GPIO/Daemon 子模块，硬件配置由 IST8310Config 完成。
 */
int8_t IST8310Register(IST8310Instance *inst)
{
    BSP_RETURN_IF_TRUE_LOG(inst == NULL, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance is NULL!"));

    /* 防重复注册检查（通过检查 I2C parent 是否已设置） */
    if (inst->i2c_inst->parent == inst)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance already registered!");
        return -1;
    }

    /* 设置 parent 指针（回调据此找回父实例） */
    inst->i2c_inst->parent = inst;
    inst->drdy->parent = inst;
    inst->rstn->parent = inst;

    if (I2CRegister(inst->i2c_inst) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "I2C register failed");
        return -1;
    }
    if (GPIORegister(inst->drdy) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "drdy register failed");
        return -1;
    }
    if (GPIORegister(inst->rstn) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "rstn register failed");
        return -1;
    }

    if (inst->daemon != NULL)
    {
        DaemonRegister(inst->daemon);
    }

    BSPLOG(&g_ist8310_log, LOG_LEVEL_INFO, "IST8310 registered, waiting for Config...");
    return 0;
}

/**
 * @brief 配置 IST8310 实例（可重复调用）
 * @note 填充子模块硬件映射、配置 daemon、执行器件初始化。
 *       要求在 IST8310Register 之后调用。
 */
int8_t IST8310Config(IST8310Instance *inst, const IST8310_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(inst == NULL, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->i2c_e >= I2C_NUM_MAX, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "i2c_e out of range!"));
    BSP_RETURN_IF_TRUE_LOG(config->work_mode != IST8310_MODE_POLLING && config->work_mode != IST8310_MODE_INT, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Invalid work_mode=%d!", (int)config->work_mode));
    BSP_RETURN_IF_TRUE_LOG(config->i2c_mode > I2C_DMA_MODE, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Invalid i2c_mode=%d!", (int)config->i2c_mode));
    /* 中断模式配阻塞传输 = 在 DRDY EXTI 里做阻塞 I2C：HAL 用 HAL_GetTick 计时，
     * tick 中断优先级低于该 EXTI，中断里 tick 不前进，从机不应答就是死循环。
     * 这个组合没有合法用途，直接拒绝而不是留个上电就卡的实例 */
    BSP_RETURN_IF_TRUE_LOG(config->work_mode == IST8310_MODE_INT && config->i2c_mode == I2C_BLOCK_MODE, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "INT mode cannot use I2C_BLOCK_MODE (blocking I2C in EXTI)!"));
    BSP_RETURN_IF_TRUE_LOG(config->avg >= IST8310_AVG_NUM, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Invalid avg=%d!", (int)config->avg));
    BSP_RETURN_IF_TRUE_LOG(config->pd_pulse != IST8310_PD_PULSE_LONG && config->pd_pulse != IST8310_PD_PULSE_NORMAL, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Invalid pd_pulse=0x%02X!", (unsigned)config->pd_pulse));
    BSP_RETURN_IF_TRUE_LOG(config->drdy_polarity != IST8310_DRDY_ACTIVE_LOW && config->drdy_polarity != IST8310_DRDY_ACTIVE_HIGH, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Invalid drdy_polarity=%d!", (int)config->drdy_polarity));
    BSP_RETURN_IF_TRUE_LOG(config->i2c_timeout_ms == 0, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "i2c_timeout_ms must be > 0!"));
    /* 中断模式没接 DRDY 就等于没有数据来源，直接拒绝配置而不是留个哑巴实例 */
    BSP_RETURN_IF_TRUE_LOG(config->work_mode == IST8310_MODE_INT && config->drdy_e >= GPIO_NUM_MAX, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "INT mode requires a valid drdy_e!"));

    /* --- 保存器件参数 --- */
    inst->work_mode = config->work_mode;
    inst->i2c_mode = config->i2c_mode; /* 必须赶在 IST8310_InitDevice 之前：它结束后按这个值切回运行时模式 */
    inst->avg = config->avg;
    inst->pd_pulse = config->pd_pulse;
    inst->drdy_polarity = config->drdy_polarity;
    inst->i2c_timeout_ms = config->i2c_timeout_ms;
    inst->meas_delay_ms = s_meas_delay_ms[config->avg];
    /* RSTN 用 GPIO_NUM_MAX 作为"未接"哨兵，未接时不做复位脉冲 */
    inst->has_rstn = (config->rstn_e < GPIO_NUM_MAX) ? 1 : 0;
    /* armed 看门狗上限：DRDY 至少要等满测量延时，再宽限一个和轮询模式同量的余量 */
    inst->arm_timeout_us = ((uint64_t)inst->meas_delay_ms + (uint64_t)IST8310_DRDY_TIMEOUT_MS) * 1000ull;

    /* --- 清零运行态字段（**必须赶在挂 DRDY 回调之前**） ---
     * Config 是可重复调用的。若先挂回调再清零，则重复配置时上一轮的 armed=1 还在，
     * 而此刻 I2C 子实例已被切到 BLOCK 模式、work_mode 也已是 INT，三者同时满足
     * IST8310_IntCallback 的全部放行条件 → DRDY 边沿会在 EXTI 里走到 I2CMemRead，
     * 而阻塞模式在 I2C_WaitReady 里是直接放行的 → HAL 的阻塞式 I2C 在中断上下文里
     * 执行，HAL_GetTick 不前进，从机不应答就是死循环（即使应答也会和任务的阻塞写撞 HAL_BUSY）。 */
    inst->transfer_busy = 0;
    inst->armed = 0;
    inst->recover_request = 0;
    inst->xfer_done = 0;
    inst->xfer_error = 0;
    inst->int_timestamp = 0;
    inst->arm_us = 0;
    inst->frame_wr = 0;
    inst->latest_idx = 0;
    inst->frame_valid = 0;
    memset(inst->frame, 0, sizeof(inst->frame));
    inst->fail_count = 0;
    inst->recover_count = 0;
    inst->last_recover_us = 0;

    /* --- 预存硬件枚举到子实例（Config 时填充映射） --- */
    inst->i2c_inst->i2c_e = config->i2c_e;
    inst->drdy->gpio_e = config->drdy_e;
    if (inst->has_rstn)
    {
        inst->rstn->gpio_e = config->rstn_e;
    }

    /* --- 配置 I2C 子实例 ---
     * 先给一个确定的工作模式：初始化序列是阻塞的，InitDevice 结束时
     * 再按 work_mode 切到运行时模式（轮询=阻塞 / 中断=IT） */
    {
        I2C_Config_s i2c_cfg = {
            .i2c_e = config->i2c_e,
            .work_mode = I2C_BLOCK_MODE,
            .rx_callback = NULL,
            .err_callback = NULL,
        };
        if (I2CConfig(inst->i2c_inst, &i2c_cfg) != 0)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "I2C config failed");
            return -1;
        }
    }

    /* --- 配置 RSTN（普通输出，无回调） ---
     * 未接时整段跳过：拿哨兵值去查 gpio_map 会越界 */
    if (inst->has_rstn)
    {
        GPIO_Config_s rstn_cfg = {
            .gpio_e = config->rstn_e,
            .callback = NULL,
        };
        if (GPIOConfig(inst->rstn, &rstn_cfg) != 0)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "rstn config failed");
            return -1;
        }
        GPIOSet(inst->rstn); /* 释放复位（低有效） */
    }

    /* --- 配置 DRDY ---
     * 回调只在中断模式挂：轮询模式挂上只会白占一个 EXTI 槽位。
     * 注意 EXTI 的触发边沿必须与 drdy_polarity 一致（CubeMX 侧配置） */
    if (config->drdy_e < GPIO_NUM_MAX)
    {
        GPIO_Config_s drdy_cfg = {
            .gpio_e = config->drdy_e,
            .callback = (config->work_mode == IST8310_MODE_INT) ? IST8310_IntCallback : NULL,
        };
        if (GPIOConfig(inst->drdy, &drdy_cfg) != 0)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "drdy config failed");
            return -1;
        }
    }

    /* --- 更新 daemon 运行参数（可重入） --- */
    if (inst->daemon != NULL)
    {
        Daemon_Config_s daemon_cfg = {
            .reload_count = config->daemon_reload,
            .fault_action = config->daemon_fault,
            .callback = NULL,
            .owner_id = inst,
        };
        DaemonConfig(inst->daemon, &daemon_cfg);
    }

    /* --- 器件初始化（会按 work_mode 自动切到运行时模式） --- */
    if (IST8310_InitDevice(inst) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Device init failed");
        return -1;
    }

    BSPLOG(&g_ist8310_log, LOG_LEVEL_INFO, "IST8310 config success (mode=%d, i2c=%d, avg=%d, meas_delay=%dms)",
           (int)inst->work_mode, (int)inst->i2c_mode, (int)inst->avg, inst->meas_delay_ms);
    return 0;
}

IST8310_Data_t IST8310ReadBlocking(IST8310Instance *inst)
{
    if (inst == NULL)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance is NULL");
        return (IST8310_Data_t){0};
    }
    if (inst->work_mode == IST8310_MODE_INT)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "IST8310ReadBlocking called in INT mode, use IST8310ReadInt instead");
        return (IST8310_Data_t){0};
    }

    /* 错误回调攒够失败次数后会置恢复请求，就地处理 */
    if (inst->recover_request)
    {
        inst->recover_request = 0;
        (void)IST8310_Recover(inst);
    }

    if (IST8310_SampleBlocking(inst) != 0)
    {
        if (inst->fail_count < UINT8_MAX)
        {
            inst->fail_count++;
        }
        if (inst->fail_count >= IST8310_RECOVER_FAIL_TH)
        {
            (void)IST8310_Recover(inst);
        }
        return (IST8310_Data_t){0};
    }

    inst->fail_count = 0;

    /* 喂狗 */
    DaemonReload(inst->daemon);

    IST8310_Data_t data = {0};
    IST8310_PackFromRx(inst->i2c_inst->rx_buff, data.mag);
    data.time_stamp = DWT_GetTimeUs(); /* 阻塞模式：读取完成时刻即时间戳 */

    return data;
}

IST8310_Data_t IST8310ReadInt(IST8310Instance *inst)
{
    if (inst == NULL)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance is NULL");
        return (IST8310_Data_t){0};
    }
    if (inst->work_mode != IST8310_MODE_INT)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "IST8310ReadInt called in POLLING mode, use IST8310ReadBlocking instead");
        return (IST8310_Data_t){0};
    }

    /* 取最新已发布帧（双缓冲，见 IST8310_I2CCpltCallback） */
    IST8310_Data_t data = {0};
    if (inst->frame_valid)
    {
        data = inst->frame[inst->latest_idx];
    }

    /* 错误回调提出的恢复请求：恢复含阻塞 I2C 与 DeInit/Init，只能在任务上下文做。
     * 本周期只做恢复、不触发新测量，下一周期把采集链接回来 */
    if (inst->recover_request)
    {
        inst->recover_request = 0;
        (void)IST8310_Recover(inst);
        return data;
    }

    /* --- armed 看门狗：DRDY 边沿丢了要能自己兜底 ---
     * 手册 §3.3：DRDY 引脚只有"读输出寄存器"才会被拉低，测量完成本身并不会先把它拉低。
     * 于是一次 IT 读失败（噪声 NACK、I2C_WaitReady 超时……）会留下这样的死局：
     *   错误回调清了 armed，但数据寄存器并没真被读走 → DRDY 引脚仍停在高位 →
     *   任务重新 ArmSingle、测量也按时完成，却**不产生新的上升沿** →
     *   边沿触发的 EXTI 此后再也不进 → armed 永远挂着、fail_count 也永远够不到恢复阈值
     *   → 通道静默返回旧帧，直到整机复位。
     * 这里用时间兜底：armed 超过 arm_timeout_us 还没落地，就按"边沿丢了"计一次失败，
     * 攒够阈值交给恢复链（IST8310_Recover 里的软复位会把 DRDY 清回低位，采集链即可复活）。
     *
     * 同一个计时器顺带兜住第二种"回调再也不来"：IT 传输既没完成也没报错。
     * HAL 的 IT 模式内部**没有超时**，总线从此静默（SDA 被拉死、从机半途掉电）时
     * 完成回调和错误回调都不会来，transfer_busy 就永久卡在 1 —— 同样再也 ArmSingle 不了。
     * 所以这里不看 transfer_busy：arm_timeout_us = 测量延时 + 一个 DRDY 超时余量(20ms)，
     * 远大于一次正常往返（测量延时 + 六字节读 ~0.2ms），只有真出事了才会走到。 */
    if (inst->armed && (DWT_GetTimeUs() - inst->arm_us) > inst->arm_timeout_us)
    {
        inst->armed = 0;
        inst->transfer_busy = 0; /* 卡住的在途传输一并清掉，否则下面永远不会重新触发 */
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Armed timeout (DRDY edge / cplt lost?), fail=%d",
               (int)inst->fail_count);

        if (inst->fail_count < UINT8_MAX)
        {
            inst->fail_count++;
        }
        if (inst->fail_count >= IST8310_RECOVER_FAIL_TH)
        {
            inst->recover_request = 1;
            return data; /* 本周期不再触发新测量，下一周期由恢复流程重新接回采集链 */
        }
    }

    /* 续上下一次测量：仅在空闲且本帧尚未触发时做，保证"一帧一次触发"。
     * 这是本函数与 BMI088ReadInt 最大的不同 —— 它有副作用 */
    if (!inst->armed && !inst->transfer_busy)
    {
        if (IST8310_ArmSingle(inst) != 0)
        {
            if (inst->fail_count < UINT8_MAX)
            {
                inst->fail_count++;
            }
            if (inst->fail_count >= IST8310_RECOVER_FAIL_TH)
            {
                inst->recover_request = 1; /* 同样留给任务上下文，别在这里做恢复 */
            }
        }
    }

    return data;
}

uint8_t IST8310IsOnline(IST8310Instance *inst)
{
    if (inst == NULL || inst->daemon == NULL)
    {
        return 0;
    }
    return DaemonIsOnline(inst->daemon);
}

#endif /* defined(HAL_I2C_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */

#endif /* DRV_IST8310_USED */

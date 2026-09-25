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

#define IST8310_DRDY_TIMEOUT_MS 20            // 等就绪的默认超时上限(ms)，IST8310Sample 的 timeout_ms=0 时用它
#define IST8310_POLL_INTERVAL_MS 1            // 两次检查就绪之间的默认间隔(ms)，Sample 的 poll_interval_ms=0 时用它
#define IST8310_RECOVER_FAIL_TH 3             // 连续失败多少次触发恢复
#define IST8310_RECOVER_COOLDOWN_US 500000ull // 两次恢复之间的最小间隔 (500ms)
#define IST8310_PROBE_TRIALS 3                // I2CIsDeviceReady 探测重试次数
#define IST8310_WAI_RETRY_DELAY_S 0.001f      // WAI 重试间隔 (1ms)

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
static int8_t IST8310_WaitXfer(IST8310Instance *inst, uint32_t timeout_ms);
static int8_t IST8310_ReadReg(IST8310Instance *inst, uint8_t reg, uint8_t len,
                              BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
static int8_t IST8310_WriteReg(IST8310Instance *inst, uint8_t reg, uint8_t data,
                               BSP_Transfer_Mode_e mode, uint32_t timeout_ms);
/* 采集链记账 */
static void IST8310_NoteFailure(IST8310Instance *inst);
/* 器件操作 */
static void IST8310_ResetPulse(IST8310Instance *inst);
static int8_t IST8310_InitDeviceBlocking(IST8310Instance *inst);
static int8_t IST8310_InitDevice(IST8310Instance *inst);
static int8_t IST8310_TriggerMeas(IST8310Instance *inst, uint8_t wait);
static int8_t IST8310_Recover(IST8310Instance *inst);
static void IST8310_ServiceRecover(IST8310Instance *inst);
static void IST8310_PackFromRx(const uint8_t *raw, float *out);
static IST8310_Data_t IST8310_ReadPolling(IST8310Instance *inst);
static IST8310_Data_t IST8310_ReadInt(IST8310Instance *inst);
/* 回调 */
static void IST8310_IntCallback(GPIOInstance *gpio_inst);
static void IST8310_I2CRxCpltCallback(I2CInstance *i2c_inst);
static void IST8310_I2CTxCpltCallback(I2CInstance *i2c_inst);
static void IST8310_I2CErrCallback(I2CInstance *i2c_inst, I2C_ErrReason_e reason);

/*============================ BSP 子模块封装 ============================*/

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
 * @param inst       I2C 实例
 * @param reg        寄存器地址
 * @param len        读取长度
 * @param mode       本次传输模式（BSP_BLOCK_MODE / BSP_IT_MODE / BSP_DMA_MODE）
 * @param timeout_ms BLOCK 透传给 HAL 作传输超时；IT/DMA 用于等"总线就绪"与本层等完成回调
 * @retval 0 成功；-1 失败
 *
 * @note 对调用方而言三种模式**语义完全一致**：返回 0 时缓冲里一定是新数据。
 *       异步模式下本函数自己等回调（IST8310_WaitXfer），所以上层代码不用区分模式，
 *       差别只在传输期间 CPU 是空闲的。
 * @note **模式与超时都每次传参**而不是从 inst 取：初始化序列这类必须同步完成的地方
 *       要显式传 BSP_BLOCK_MODE，而中断里发起时超时必须传 0（等就绪要按 tick 自旋，
 *       中断里 tick 不前进）—— 两种需求都不该被"实例默认值"绑住。
 * @note 传 IT/DMA 时只能在任务上下文调用（本函数要等完成回调）。
 */
static int8_t IST8310_ReadReg(IST8310Instance *inst, uint8_t reg, uint8_t len,
                              BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    if (mode == BSP_BLOCK_MODE)
    {
        return (I2CMemRead(inst->i2c_inst, IST8310_I2C_ADDR_7BIT, reg,
                           I2C_MEM_ADDR_SIZE_8BIT, len, mode, timeout_ms) == BSP_OK)
                   ? 0
                   : -1;
    }

    /* 发起前清标志：否则上一笔残留的完成标记会让本笔立刻"成功"返回，
     * 调用方拿到的其实是上一次读剩下的旧缓冲 */
    inst->xfer_done = 0;
    inst->xfer_error = 0;

    if (I2CMemRead(inst->i2c_inst, IST8310_I2C_ADDR_7BIT, reg,
                   I2C_MEM_ADDR_SIZE_8BIT, len, mode, timeout_ms) != BSP_OK)
    {
        return -1; /* 启动失败不会有完成回调，bsp_i2c 已复位句柄并回调了 err_callback */
    }

    return IST8310_WaitXfer(inst, timeout_ms);
}

/**
 * @brief 写从机寄存器
 * @param inst       I2C 实例
 * @param reg        寄存器地址
 * @param data       待写字节
 * @param mode       本次传输模式，语义同 IST8310_ReadReg
 * @param timeout_ms 超时语义同 IST8310_ReadReg
 * @retval 0 成功；-1 失败
 *
 * @note data 是调用方的栈变量。阻塞模式下 HAL 同步用完即返回；
 *       异步模式下本函数会等到传输真正结束才返回，那时 HAL 早已不再持有该指针 ——
 *       所以传栈变量是安全的，「不能传栈变量」的约束不适用于本函数。
 *       （发起后**不等待**的异步写不适用这条，见 IST8310_TriggerMeas 用 req_buff 的理由。）
 */
static int8_t IST8310_WriteReg(IST8310Instance *inst, uint8_t reg, uint8_t data,
                               BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    if (mode == BSP_BLOCK_MODE)
    {
        return (I2CMemWrite(inst->i2c_inst, IST8310_I2C_ADDR_7BIT, reg,
                            I2C_MEM_ADDR_SIZE_8BIT, &data, 1, mode, timeout_ms) == BSP_OK)
                   ? 0
                   : -1;
    }

    inst->xfer_done = 0;
    inst->xfer_error = 0;

    if (I2CMemWrite(inst->i2c_inst, IST8310_I2C_ADDR_7BIT, reg,
                    I2C_MEM_ADDR_SIZE_8BIT, &data, 1, mode, timeout_ms) != BSP_OK)
    {
        return -1;
    }

    return IST8310_WaitXfer(inst, timeout_ms);
}

/*============================ 采集链记账 ============================*/

/**
 * @brief 记一次失败，攒够阈值就提出恢复请求
 * @note 只在"整帧失败"的粒度上调用：一次中间读成功不足以清零 fail_count
 *       （否则 STAT1 读得到、数据读不到的死局会被反复清零，永远够不到阈值）。
 */
static void IST8310_NoteFailure(IST8310Instance *inst)
{
    if (inst->fail_count < UINT8_MAX)
    {
        inst->fail_count++;
    }
    if (inst->fail_count >= IST8310_RECOVER_FAIL_TH)
    {
        inst->recover_request = 1;
    }
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
 * @brief 器件初始化主体（内部一律用 BSP_BLOCK_MODE，调用方无需切模式）
 * @retval 0 成功；-1 失败
 * @note 为什么要阻塞：序列里全是"写完马上读回校验"和轮询 SRST 自清，用异步还得逐笔等。
 *       模式既然是每次调用传参的，这里显式传 BSP_BLOCK_MODE 即可 ——
 *       旧版为此要在进/出各调一次 I2CConfig 切模式，现在不需要了。
 *
 * @note 序列依据数据手册 §3.1：
 *       RSTN 脉冲（可选）→ 等 POR → WAI 校验 → 软复位并等 SRST 自清 → 等 POR
 *       → PDCNTL（脉冲宽度，§3.1.1 要求初始写 Normal）→ AVGCNTL（平均次数）
 *       → CNTL2（DRDY 使能 + 固定高有效）→ CNTL1（待机）→ 回读校验
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
        if (IST8310_ReadReg(inst, IST8310_WAI_REG, 1, BSP_BLOCK_MODE, inst->i2c_timeout_ms) == 0)
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
    if (IST8310_WriteReg(inst, IST8310_CNTL2_REG, IST8310_CNTL2_SRST, BSP_BLOCK_MODE, inst->i2c_timeout_ms) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "SRST write failed");
        return -1;
    }

    uint64_t start_us = DWT_GetTimeUs();
    while (1)
    {
        if (IST8310_ReadReg(inst, IST8310_CNTL2_REG, 1, BSP_BLOCK_MODE, inst->i2c_timeout_ms) != 0)
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
    if (IST8310_WriteReg(inst, IST8310_PDCNTL_REG, (uint8_t)inst->pd_pulse, BSP_BLOCK_MODE, inst->i2c_timeout_ms) != 0)
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
        if (IST8310_WriteReg(inst, IST8310_AVGCNTL_REG, avg_reg, BSP_BLOCK_MODE, inst->i2c_timeout_ms) != 0)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "AVGCNTL write failed");
            return -1;
        }
    }

    /* 6) CNTL2：使能 DRDY（总开关）+ 固定高电平有效（DRP=1）
     * 极性不是可配项：CubeMX 里两块板的 DRDY EXTI 都配了上升沿
     * （DJI_A 的 PE3 / DJI_C 的 PG3），器件侧就必须是 DRP=1，否则一个中断都等不来。 */
    uint8_t cntl2 = (uint8_t)(IST8310_CNTL2_DREN | IST8310_CNTL2_DRP);
    if (IST8310_WriteReg(inst, IST8310_CNTL2_REG, cntl2, BSP_BLOCK_MODE, inst->i2c_timeout_ms) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "CNTL2 write failed");
        return -1;
    }

    /* 7) 回到待机（软复位后的默认态，显式写一次让语义明确） */
    if (IST8310_WriteReg(inst, IST8310_CNTL1_REG, IST8310_CNTL1_MODE_STANDBY, BSP_BLOCK_MODE, inst->i2c_timeout_ms) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "CNTL1 standby write failed");
        return -1;
    }

    /* 8) 回读校验：DREN/DRP 可读，能证明前面的写确实落到了器件里 */
    if (IST8310_ReadReg(inst, IST8310_CNTL2_REG, 1, BSP_BLOCK_MODE, inst->i2c_timeout_ms) != 0)
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
 * @brief 器件初始化（内部走阻塞传输）
 * @note 薄封装，保留只为让调用点读起来是"器件操作"而不是"寄存器序列"
 */
static int8_t IST8310_InitDevice(IST8310Instance *inst)
{
    return IST8310_InitDeviceBlocking(inst);
}

/**
 * @brief 发出一次单次测量请求（写 CNTL1 = SINGLE）
 * @param inst I2C 实例
 * @param wait 1 = 等这笔写完成；0 = **发起即返回**
 * @retval 0 已发出；-1 失败
 *
 * @note 请求字节取自实例常驻缓冲 `req_buff`，不是调用方的栈变量：`wait=0` 时本函数
 *       返回后 HAL 仍持有该指针（IT 模式的中断搬字节、DMA 模式的 DMA 搬运都在之后），
 *       用栈变量会读到被后续中断帧覆盖的内存。DMA 模式还要求它在 DMA 可访问的 RAM。
 * @note `wait=0` 只能在 I2C 完成回调里用，而且**必须**免等：该回调本身就运行在
 *       I2C 事件中断里，这笔新写的完成回调要靠同一个中断才能进来，同优先级不会重入，
 *       在这里等完成回调就是永久自旋 —— 且 tick 仍在走，喂狗也救不回来，
 *       现场表现是"传感器静默、日志里一个错误都没有"。
 * @note 传输方式统一取 `i2c_mode`（与"请求按传输模式发"的约定一致）：`wait=1` 时
 *       超时取实例配置值，`wait=0` 时必须传 0（中断里等就绪要按 tick 自旋）。
 */
static int8_t IST8310_TriggerMeas(IST8310Instance *inst, uint8_t wait)
{
    inst->req_buff[0] = (uint8_t)IST8310_CNTL1_MODE_SINGLE;

    if (wait && inst->i2c_mode != BSP_BLOCK_MODE)
    {
        /* 发起前清标志：否则上一笔残留的完成标记会让本笔立刻"成功"返回 */
        inst->xfer_done = 0;
        inst->xfer_error = 0;
    }

    if (I2CMemWrite(inst->i2c_inst, IST8310_I2C_ADDR_7BIT, IST8310_CNTL1_REG,
                    I2C_MEM_ADDR_SIZE_8BIT, inst->req_buff, 1,
                    inst->i2c_mode, wait ? inst->i2c_timeout_ms : 0) != BSP_OK)
    {
        /* 启动失败不会有完成回调，bsp_i2c 已复位句柄并回调了 err_callback */
        return -1;
    }

    if (wait && inst->i2c_mode != BSP_BLOCK_MODE)
    {
        return IST8310_WaitXfer(inst, inst->i2c_timeout_ms);
    }

    /* BLOCK 模式同步写完；wait=0 时故意不等完成回调 */
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

    /* 1) 外设级总线恢复（DeInit + Init + 复位句柄状态）。
     *    `(void)` 是有意的：本例给出的判据（连续 3 次传输失败 / 看门狗离线）都是**实例级**
     *    现象，而 bsp 只在自己读到**总线级**证据（`I2C_FLAG_BUSY` 置位且无在途传输）时才动手，
     *    否则返回 BSP_BUSY 什么都不做 —— 总线已放开时的失败由下面第 2 步的探测（不应答再脉冲
     *    RSTN）去救才是对症的。见 bsp_i2c.h 的 I2CBusRecover 契约。 */
    (void)I2CBusRecover(inst->i2c_inst);

    /* 2) 探测从机是否应答；不应答就先硬复位再试一次 */
    if (I2CIsDeviceReady(inst->i2c_inst, IST8310_I2C_ADDR_7BIT, IST8310_PROBE_TRIALS, inst->i2c_timeout_ms) != BSP_OK)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Device not ready after bus recover");
        if (!inst->has_rstn)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "No RSTN wired, cannot reset device");
            return -1;
        }
        IST8310_ResetPulse(inst);
        if (I2CIsDeviceReady(inst->i2c_inst, IST8310_I2C_ADDR_7BIT, IST8310_PROBE_TRIALS, inst->i2c_timeout_ms) != BSP_OK)
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
    inst->trigger_pending = 0;
    inst->armed = 0;
    inst->data_ready = 0;
    /* 异步传输标志一并清干净：重建外设后旧标志已无意义，
     * 留着会让恢复后的第一笔读"立刻完成"并拿回旧缓冲 */
    inst->xfer_done = 0;
    inst->xfer_error = 0;
    /* 已发布的帧一并作废：它属于恢复前那条已经断掉的采集链（器件还被软复位重配过），
     * 留着会让中断模式恢复后交出的"第一帧"其实是断链前的旧数据 —— 帧内时间戳虽能
     * 判新旧，但语义上它已经不代表当前这个器件状态了。作废后 `IST8310Read` 会返回
     * 全 0，直到新链推出第一帧。 */
    inst->frame_valid = 0;
    inst->frame_wr = 0;
    inst->latest_idx = 0;

    /* 4) 重新发出测量请求，把采集链接回去（中断模式：此后由完成回调自维持，
     *    armed/link_us 也由 IST8310Request 一并置好；
     *    轮询模式：下次 Request/Sample 会自己发，这里不必多做） */
    if (inst->work_mode == IST8310_MODE_INT && IST8310Request(inst) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Re-trigger after recover failed");
        return -1;
    }

    BSPLOG(&g_ist8310_log, LOG_LEVEL_INFO, "Recover done");
    return 0;
}

/**
 * @brief 消费"需执行恢复"的请求（错误回调置位，这里执行）
 *
 * @note 为什么要单独一个函数：`IST8310_I2CErrCallback` 可能运行在中断里，那里只能
 *       置位 —— `IST8310_Recover` 含阻塞 I2C 与 DeInit/Init。真正执行只能在任务上下文，
 *       于是约定由**每个任务上下文的公开入口**（`IST8310Read` / `IST8310Sample`）在入口
 *       消费一次。
 * @note 放在入口而不是各条失败出口上：失败出口有很多条（Request 失败、STAT1 连读失败、
 *       等就绪超时…），漏掉任意一条都会让"链路彻底坏了"这种**最该恢复**的情况再也进不了
 *       恢复流程 —— 而它恰恰是每条出口都会走到的场景。挂在入口上就没有这个漏网问题。
 * @note 只能在任务上下文调用。
 */
static void IST8310_ServiceRecover(IST8310Instance *inst)
{
    if (!inst->recover_request)
    {
        return;
    }
    inst->recover_request = 0;
    (void)IST8310_Recover(inst);
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
 * @note **数据读**只从 EXTI 发起，不从完成回调里发起；**测量请求**相反，
 *       正常运行时由完成回调续（见 IST8310_I2CRxCpltCallback）—— 两者不冲突：
 *       请求是写完就走的短写，读才是要等 DRDY 的那件事。
 */
static void IST8310_IntCallback(GPIOInstance *gpio_inst)
{
    IST8310Instance *inst = (IST8310Instance *)gpio_inst->parent;
    if (inst == NULL || inst->work_mode != IST8310_MODE_INT)
    {
        return;
    }

    /* 未请求就来 DRDY（复位残留、或上一次已处理完的边沿）、或测量请求还在写、
     * 或已有一笔读在途：直接丢弃，别起一笔错位的读 */
    if (!inst->armed || inst->transfer_busy || inst->trigger_pending)
    {
        return;
    }

    inst->int_timestamp = DWT_GetTimeUs();
    inst->transfer_busy = 1;

    /* timeout_ms 必须是 0：中断里不能等就绪 —— 等不到就绪也没法在这里做收尾
     * （收尾要经 HAL_DMA_Abort / HAL_I2C_Init 按 tick 自旋，中断里 tick 不前进）。
     * 忙（BSP_BUSY）时本帧直接放弃，交给任务侧看门狗（link_us 停滞）补发请求 */
    if (I2CMemRead(inst->i2c_inst, IST8310_I2C_ADDR_7BIT, IST8310_DATAXL_REG,
                   I2C_MEM_ADDR_SIZE_8BIT, IST8310_DATA_LEN, inst->i2c_mode, 0) != BSP_OK)
    {
        /* 启动失败不会有完成回调。I2CMemRead 内部已复位 BSP 句柄并回调了
         * 本驱动的 err_callback，这里再兜一次传输锁，双保险 */
        inst->transfer_busy = 0;
        inst->armed = 0;
    }
}

/**
 * @brief I2C 读传输完成回调（Mem 读的 HAL 完成回调）
 * @note 这是**自维持采集链的闭合点**：发布本帧数据后紧接着发出下一次测量请求，
 *       于是中断模式不需要任何任务侧"续命"（§ 头文件里的采集模型）。
 * @note 重新发请求**必须发起即返回**（`wait=0`）：本回调运行在 I2C 事件中断里
 *       （中断模式只能是 IT，见 IST8310Config），
 *       而这笔新写的完成回调要靠同一个中断才能进来，同优先级不会重入 ——
 *       在这里等完成回调就是永久自旋，且 tick 仍在走、喂狗也救不回来，
 *       现场表现是"传感器静默、日志里一个错误都没有"。
 *       敢在这里发起新传输，依据是 bsp_i2c 的完成回调契约：派发回调前 State 已复位为
 *       READY、Lock 已释放（这与 bsp_spi 的 DMA 流未释放不同，已核对 bsp_i2c.c）。
 * @note 与写完成**分成两个回调**（bsp_i2c 按读/写分别派发）：读完成才是"一帧到手"，
 *       写完成只表示"请求落盘了"。合成一个入口的话，轮询模式下用 IT/DMA 写 CNTL1
 *       的完成会和数据读完成混在一起，只能靠调用方临时切模式来分辨。
 */
static void IST8310_I2CRxCpltCallback(I2CInstance *i2c_inst)
{
    IST8310Instance *inst = (IST8310Instance *)i2c_inst->parent;
    if (inst == NULL)
    {
        return;
    }

    /* 先记账：轮询模式下的 IT/DMA 传输靠它让 IST8310_WaitXfer 收尾 */
    inst->xfer_done = 1;

    /* 轮询模式：这笔读是 IST8310_ReadReg 自己发起的，可能是 STAT1 这种中间读，
     * 调用方在 WaitXfer 返回后自己去取 rx_buff。
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
    inst->armed = 0; /* 本帧已收完，下面的续请求会重新置起来 */
    inst->fail_count = 0;

    /* 喂狗 */
    DaemonReload(inst->daemon);

    /* --- 续下一帧：立刻再发一次测量请求，让链路自己转起来 ---
     * 顺序有讲究：先置 trigger_pending 再发 —— 写期间器件还没开始新测量，
     * 若此时 DRDY 还是高（上一次输出寄存器没被真正读走），残留的上升沿会立刻
     * 进 EXTI，起一笔读到旧数据的错位传输；trigger_pending 就是挡它的。
     * 写完成回调（IST8310_I2CTxCpltCallback）负责清掉这个标志。 */
    inst->trigger_pending = 1;
    inst->armed = 1;
    if (IST8310_TriggerMeas(inst, 0) != 0)
    {
        /* 启动失败不会有写完成回调，标志得自己清，否则 EXTI 门控被永久挡住。
         * armed 也收回：本帧测量没发出去，等谁都不来 —— 交给任务侧的链路看门狗
         * （link_us 停滞）补发请求 */
        inst->trigger_pending = 0;
        inst->armed = 0;
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Next measurement request failed in cplt");
        return;
    }

    inst->link_us = DWT_GetTimeUs(); /* 链路推进，看门狗重新计时 */
}

/**
 * @brief I2C 写传输完成回调（Mem 写的 HAL 完成回调）
 * @note 只记账：轮询模式下配成 IT/DMA 时，IST8310_WaitXfer 靠它收尾。
 *       写没有"发布磁力帧"这回事，也不该动 fail_count —— 一次中间写成功
 *       不能把前面真失败攒下的计数清零。
 * @note 中断模式下它还有个关键职责：**放开 EXTI 门控**。测量请求由读完成回调
 *       发起（发起即返回），请求真正落盘之前 trigger_pending 一直挡着 DRDY 边沿；
 *       本回调就是"请求确实写完了"的信号。
 */
static void IST8310_I2CTxCpltCallback(I2CInstance *i2c_inst)
{
    IST8310Instance *inst = (IST8310Instance *)i2c_inst->parent;
    if (inst == NULL)
    {
        return;
    }

    inst->xfer_done = 1;
    inst->trigger_pending = 0;
}

/**
 * @brief I2C 传输错误回调（bsp_i2c 在传输未能正常发起 / HAL 报错 / 强制收尾时调用）
 * @param i2c_inst I2C 实例（parent 指向 IST8310Instance）
 * @param reason   出错原因（I2C_ERR_HW 硬件错 / I2C_ERR_ABORT 被 bsp 收尾）
 *
 * @note 这类失败不会再有完成回调，必须在这里复位传输状态，
 *       否则 transfer_busy/armed 恒为 1，驱动永久失联。
 *       两种原因的处理相同（都是丢弃本次传输、攒失败计数），故不细分。
 * @note 真正的恢复（DeInit/Init + 阻塞 I2C）留给任务上下文，
 *       这里**只置请求位**：本回调可能运行在中断里。
 * @note 中断模式下会清掉 `armed` 与 `trigger_pending`（本帧作废，EXTI 不再放行）。
 *       链路不会因此永久停摆：没人再发请求，`link_us` 就不动，
 *       任务侧 `IST8310Read` 的链路看门狗会在 `link_timeout_us` 后补发。
 */
static void IST8310_I2CErrCallback(I2CInstance *i2c_inst, I2C_ErrReason_e reason)
{
    IST8310Instance *inst = (IST8310Instance *)i2c_inst->parent;
    if (inst == NULL)
    {
        return;
    }

    (void)reason;

    /* 先记账：轮询模式下调用方靠它知道这笔传输没成功（I2CMemRead 那头也会返回非 BSP_OK，
     * 两条路都指向同一个结论） */
    inst->xfer_error = 1;

    if (inst->work_mode != IST8310_MODE_INT)
    {
        /* 轮询模式的失败计数与恢复统一由 IST8310Read 管，
         * 这里再计一次会变成"一次失败记两笔" */
        return;
    }

    inst->transfer_busy = 0;
    inst->armed = 0;           /* 丢弃本帧，等链路看门狗重新发请求 */
    inst->trigger_pending = 0; /* 请求写失败没有完成回调，门控必须在这里放开 */

    IST8310_NoteFailure(inst);
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

    /* 不做本层的防重复注册检查：I2C 实例的 parent 改由 I2CConfig 写入（读直接读、
     * 写必须用函数），不能再用它当"已注册"标志；而各子模块自己都有防重
     * （I2CRegister / GPIORegister / DaemonRegister 都会拒绝重复注册并打日志），
     * 重复调用本函数的首个失败点就会被拦住。 */
    inst->drdy->parent = inst;
    inst->rstn->parent = inst;

    if (I2CRegister(inst->i2c_inst) != BSP_OK)
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
    BSP_RETURN_IF_TRUE_LOG(config->i2c_mode > BSP_DMA_MODE, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Invalid i2c_mode=%d!", (int)config->i2c_mode));
    /* 中断模式的传输方式只允许 IT（BLOCK/DMA 都拒绝）。理由分三层：
     * - **BLOCK**：这两笔传输都由中断发起（DRDY EXTI 发起读、读完成回调发起下一个请求），
     *   而 HAL 的阻塞 I2C 用 HAL_GetTick 计时 —— tick 中断优先级低于 EXTI，中断里 tick
     *   不前进，从机不应答就是死循环。这个组合没有合法用途。
     * - **DMA**：本层的采集链**全在 ISR 里自维持**，任务侧没有"下一笔传输"的发起入口。
     *   一次失败若留下需收尾的残留，只有 tick 依赖的动作能清（HAL_DMA_Abort / DeInit+Init），
     *   而它们在 ISR 里做不了，只能等任务上下文的补刀点 —— 那条链的补刀点是低频的
     *   IST8310Read 的链路看门狗，动作是整段恢复（IST8310_Recover → I2CBusRecover：
     *   器件重初始化 + 必要时 DeInit/Init 整条总线重建），代价远不止"丢一帧"。
     *   其中"重建整条总线"那一步还有条件：bsp 要先自证总线级判据（`I2C_FLAG_BUSY` 置位且
     *   无在途传输，见 bsp_i2c.h），总线没被占住时它不会做 —— 也就是说这条链**不能指望
     *   一定能把外设整个重来一遍**，所以这里直接拒绝 DMA。
     *   DMA 偏偏多引入一个与总线好坏无关的失败源（DMA 流的 State），一旦它非 READY，
     *   此后每一笔 DMA 都在启动阶段直接失败，形成确定性的级联失败。
     * - **IT**：没有 DMA 流可留，下一帧能否成功只取决于总线是否已放开（NACK / 仲裁丢失
     *   之后总线通常是空闲的，链路往往自愈）；且 6 字节读 + 1 字节写在 400kHz 下约 0.2ms，
     *   DMA 省下的 CPU 时间在这里毫无意义。
     * 轮询模式不受此限：每笔传输都由任务发起，调用方自己就是补刀点，DMA 无妨。 */
    BSP_RETURN_IF_TRUE_LOG(config->work_mode == IST8310_MODE_INT && config->i2c_mode != BSP_IT_MODE, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR,
                                  "INT mode requires BSP_IT_MODE (got %d): the self-sustaining chain issues transfers from ISR",
                                  (int)config->i2c_mode));
    BSP_RETURN_IF_TRUE_LOG(config->avg >= IST8310_AVG_NUM, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Invalid avg=%d!", (int)config->avg));
    BSP_RETURN_IF_TRUE_LOG(config->pd_pulse != IST8310_PD_PULSE_LONG && config->pd_pulse != IST8310_PD_PULSE_NORMAL, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Invalid pd_pulse=0x%02X!", (unsigned)config->pd_pulse));
    BSP_RETURN_IF_TRUE_LOG(config->i2c_timeout_ms == 0, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "i2c_timeout_ms must be > 0!"));
    /* 中断模式没接 DRDY 就等于没有数据来源，直接拒绝配置而不是留个哑巴实例 */
    BSP_RETURN_IF_TRUE_LOG(config->work_mode == IST8310_MODE_INT && config->drdy_e >= GPIO_NUM_MAX, -1,
                           BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "INT mode requires a valid drdy_e!"));

    /* --- 保存器件参数 --- */
    inst->work_mode = config->work_mode;
    inst->i2c_mode = config->i2c_mode; /* 运行时默认传输模式：每次 I2C 调用时透传给 bsp */
    inst->avg = config->avg;
    inst->pd_pulse = config->pd_pulse;
    inst->i2c_timeout_ms = config->i2c_timeout_ms;
    inst->meas_delay_ms = s_meas_delay_ms[config->avg];
    /* RSTN 用 GPIO_NUM_MAX 作为"未接"哨兵，未接时不做复位脉冲 */
    inst->has_rstn = (config->rstn_e < GPIO_NUM_MAX) ? 1 : 0;
    /* 链路看门狗阈值：采集链正常推进的周期是"测量延时 + 一笔六字节读"，
     * 这里按测量延时再宽限一个 DRDY 超时余量，远大于正常往返，只有真停住才会触发 */
    inst->link_timeout_us = ((uint64_t)inst->meas_delay_ms + (uint64_t)IST8310_DRDY_TIMEOUT_MS) * 1000ull;

    /* --- 清零运行态字段（**必须赶在挂 DRDY 回调之前**） ---
     * Config 是可重复调用的。重复配置时上一轮可能留着 armed=1，若 DRDY 回调已经挂着，
     * 一个迟到的边沿就会在 EXTI 里走到 I2CMemRead。现在模式是每次传参的、EXTI 里传 0 超时，
     * 不会再出现旧版"HAL 阻塞 I2C 跑在中断里"的死循环，但这个迟到的边沿仍会白白起一笔
     * 错位的读（读到的数据没有对应的 armed 上下文），所以顺序照旧保持。 */
    inst->data_ready = 0;
    inst->transfer_busy = 0;
    inst->trigger_pending = 0;
    inst->armed = 0;
    inst->recover_request = 0;
    inst->xfer_done = 0;
    inst->xfer_error = 0;
    inst->int_timestamp = 0;
    inst->link_us = 0;
    inst->frame_wr = 0;
    inst->latest_idx = 0;
    inst->frame_valid = 0;
    memset(inst->frame, 0, sizeof(inst->frame));
    inst->fail_count = 0;
    inst->recover_count = 0;
    inst->last_recover_us = 0;

    /* --- 配置 I2C 子实例 ---
     * 传输模式不在这里定（每次收发调用传参），这里只挂总线级参数与三个回调。
     * 回调挂上不影响初始化：初始化序列全部显式传 BSP_BLOCK_MODE，而 BLOCK 传输
     * 不产生任何回调（bsp_i2c 只在 IT/DMA 完成时派发）。 */
    {
        I2C_Config_s i2c_cfg = {
            .i2c_e = config->i2c_e,
            .parent = inst,
            .rx_callback = IST8310_I2CRxCpltCallback,
            .tx_callback = IST8310_I2CTxCpltCallback,
            .err_callback = IST8310_I2CErrCallback,
        };
        if (I2CConfig(inst->i2c_inst, &i2c_cfg) != BSP_OK)
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
     * 极性固定为高有效（见 InitDevice 里 CNTL2 的 DRP=1），故 CubeMX 侧该 EXTI
     * 必须是上升沿 —— 由 CubeMX 定死，本层不做配置项。 */
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

    /* --- 器件初始化（内部一律阻塞传输） --- */
    if (IST8310_InitDevice(inst) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Device init failed");
        return -1;
    }

    /* --- 中断模式：发出第一条测量请求，把自维持采集链点起来 ---
     * 器件没有连续测量模式，没有这一帧"种子"链路就永远不会开始转。
     * 此后每次 DRDY 由 EXTI 发起读、读完成回调发布并续请求，不再需要调用方参与。 */
    if (inst->work_mode == IST8310_MODE_INT && IST8310Request(inst) != 0)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Initial measurement request failed");
        return -1;
    }

    BSPLOG(&g_ist8310_log, LOG_LEVEL_INFO, "IST8310 config success (mode=%d, i2c=%d, avg=%d, meas_delay=%dms)",
           (int)inst->work_mode, (int)inst->i2c_mode, (int)inst->avg, inst->meas_delay_ms);
    return 0;
}

/**
 * @brief 发送一次单次测量请求（写 CNTL1 = SINGLE）
 * @note 轮询模式：清掉就绪记录（新一帧还没测完），再发请求。
 *       中断模式：这次请求是用来**接回采集链**的（Config 末尾、恢复流程、
 *       以及链路看门狗停滞时的补发）—— 先把可能卡住的在途读丢掉，置 armed 让
 *       EXTI 放行，写完再刷新 link_us 让看门狗重新计时。正常运行时调用方不必调它。
 */
int8_t IST8310Request(IST8310Instance *inst)
{
    if (inst == NULL)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance is NULL");
        return -1;
    }

    /* 新一帧开始：上一次就绪结论作废（IST8310CheckReady 会重新置位） */
    inst->data_ready = 0;

    if (inst->work_mode == IST8310_MODE_INT)
    {
        /* 丢掉可能卡住的在途读与残留门控：本函数在任务上下文，等得起。
         * 置 trigger_pending 挡住写期间的 DRDY 边沿，等到写完成（wait=1）再放开 */
        inst->transfer_busy = 0;
        inst->trigger_pending = 1;
    }

    if (IST8310_TriggerMeas(inst, 1) != 0)
    {
        if (inst->work_mode == IST8310_MODE_INT)
        {
            inst->trigger_pending = 0;
        }
        IST8310_NoteFailure(inst);
        return -1;
    }

    if (inst->work_mode == IST8310_MODE_INT)
    {
        /* wait=1 时写已完成、TxCplt 已清过门控；这里再兜一次，避免回调没派发的极端情况 */
        inst->trigger_pending = 0;
        inst->armed = 1;
        inst->link_us = DWT_GetTimeUs();
    }

    return 0;
}

/**
 * @brief 检查数据是否就绪（读 STAT1.DRDY）
 * @note 读 STAT1 不会清 DRDY（只有读输出寄存器/STAT2 才会），因此可以反复查。
 */
int8_t IST8310CheckReady(IST8310Instance *inst)
{
    if (inst == NULL)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance is NULL");
        return -1;
    }
    if (inst->work_mode == IST8310_MODE_INT)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "CheckReady is meaningless in INT mode (DRDY is expressed by the interrupt)");
        return -1;
    }

    if (IST8310_ReadReg(inst, IST8310_STAT1_REG, 1, inst->i2c_mode, inst->i2c_timeout_ms) != 0)
    {
        IST8310_NoteFailure(inst);
        return -1;
    }

    uint8_t ready = ((inst->i2c_inst->rx_buff[0] & (uint8_t)IST8310_STAT1_DRDY) != 0) ? 1 : 0;
    inst->data_ready = ready;
    return (int8_t)ready;
}

/**
 * @brief 轮询模式读一帧：连读输出寄存器并换算（不做任何触发，触发是 Request 的事）
 * @note 调用方跳过 CheckReady 时读到的可能是上一帧的旧磁场值，这里打告警 ——
 *       不在这里替他查一次，是因为"就绪记录"就是给调用方看的（多一次 I2C 也改变不了
 *       数据可能是旧的这一事实）。
 */
static IST8310_Data_t IST8310_ReadPolling(IST8310Instance *inst)
{
    if (!inst->data_ready)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Read without a confirmed ready (CheckReady skipped?), frame may be stale");
    }

    if (IST8310_ReadReg(inst, IST8310_DATAXL_REG, IST8310_DATA_LEN,
                        inst->i2c_mode, inst->i2c_timeout_ms) != 0)
    {
        IST8310_NoteFailure(inst);
        return (IST8310_Data_t){0};
    }

    inst->data_ready = 0;
    inst->fail_count = 0; /* 整帧成功才清零：中间读成功不足以证明链路健康 */

    IST8310_Data_t data = {0};
    IST8310_PackFromRx(inst->i2c_inst->rx_buff, data.mag);
    data.time_stamp = DWT_GetTimeUs(); /* 轮询模式的采样时刻就是读完成时刻 */

    DaemonReload(inst->daemon);
    return data;
}

/**
 * @brief 中断模式取最新已发布帧，并顺带做链路看门狗
 *
 * @note --- 为什么看门狗要用"距上次推进的时长"而不是某个标志位 ---
 *       中断模式的链路有几种不同的死法，共同现象都是"链路不再推进"：
 *         - 读完成回调里的续请求发起失败（trigger_pending 已清、armed 已清）；
 *         - 错误回调丢弃了一帧，之后没人再发请求；
 *         - DRDY 边沿丢了 / 卡在高位：手册 §3.3 说 DRDY 只有"读输出寄存器"才会被拉低，
 *           于是一次读失败会留下死局 —— 输出寄存器没被读走、DRDY 仍停在高位，
 *           器件此后**不会产生新的上升沿**，边沿触发的 EXTI 再也不进；
 *         - IT 传输既不完成也不报错（HAL 的 IT 模式内部没有超时）。
 *       旧版只看 `armed`：请求写失败时 armed=0，看门狗反而不响 —— 恰好漏掉最常见的一种。
 *       故判据取 `link_us`（请求发出或一帧发布时刷新），超过 `link_timeout_us` 即认定停滞。
 * @note 停滞时的动作分两级：先**补发一次请求**（能救活前两种死法），
 *       连续失败攒够 `IST8310_RECOVER_FAIL_TH` 再升级为整段恢复
 *       （后两种死法要靠恢复流程里的软复位把 DRDY 清回低位）。
 */
static IST8310_Data_t IST8310_ReadInt(IST8310Instance *inst)
{
    /* 取最新已发布帧（双缓冲，见 IST8310_I2CRxCpltCallback） */
    IST8310_Data_t data = {0};
    if (inst->frame_valid)
    {
        data = inst->frame[inst->latest_idx];
    }

    if ((DWT_GetTimeUs() - inst->link_us) > inst->link_timeout_us)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Sample link stalled (no progress in %dms), re-requesting (fail=%d)",
               (int)(inst->link_timeout_us / 1000ull), (int)inst->fail_count);
        IST8310_NoteFailure(inst);

        if (inst->recover_request)
        {
            /* 攒够阈值：交给恢复流程，本周期不再补发（恢复末尾会把链接回去） */
            inst->recover_request = 0;
            (void)IST8310_Recover(inst);
            return data;
        }

        (void)IST8310Request(inst); /* 补发一次，把链子接回来；失败会在里面再记一笔 */
    }

    return data;
}

/**
 * @brief 读取一帧磁力数据
 * @note 见头文件的分流说明：轮询模式读寄存器，中断模式取已发布帧。
 */
IST8310_Data_t IST8310Read(IST8310Instance *inst)
{
    if (inst == NULL)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance is NULL");
        return (IST8310_Data_t){0};
    }

    /* 错误回调提出的恢复请求（见 IST8310_ServiceRecover）：恢复会重建器件并把链子
     * 重新接上，但**轮询模式这一帧的组织已经被打断**：继续往下读，拿到的可能是恢复前的
     * 旧数据配上新时间戳，从外部完全看不出，故直接作废这一帧，让调用方重新 Request。
     * 中断模式恢复后链路已在跑，返回缓冲里最新的有效帧即可。 */
    uint8_t recovered = inst->recover_request;
    IST8310_ServiceRecover(inst);
    if (recovered && inst->work_mode != IST8310_MODE_INT)
    {
        return (IST8310_Data_t){0};
    }

    if (inst->work_mode == IST8310_MODE_INT)
    {
        return IST8310_ReadInt(inst);
    }

    return IST8310_ReadPolling(inst);
}

/**
 * @brief 轮询模式的一站式封装：发请求 → 等就绪 → 读数据
 * @note 超时那一帧顺手读一次输出寄存器：手册 §3.3 里这是**唯一**能拉低 DRDY 的途径，
 *       不做的话 DRDY 会一直停在高位，而轮询模式虽然不靠边沿，却会把器件留在
 *       一个"下一次读回来的仍是这一帧"的状态里。返回全 0，调用方按失败处理。
 */
IST8310_Data_t IST8310Sample(IST8310Instance *inst, uint32_t poll_interval_ms, uint32_t timeout_ms)
{
    if (inst == NULL)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_ERROR, "Instance is NULL");
        return (IST8310_Data_t){0};
    }
    if (inst->work_mode == IST8310_MODE_INT)
    {
        BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "IST8310Sample is polling-only, use IST8310Read in INT mode");
        return (IST8310_Data_t){0};
    }

    if (poll_interval_ms == 0)
    {
        poll_interval_ms = IST8310_POLL_INTERVAL_MS;
    }
    if (timeout_ms == 0)
    {
        timeout_ms = IST8310_DRDY_TIMEOUT_MS;
    }

    /* 入口先消费恢复请求（同 IST8310Read）。这里本帧还没开始编排，恢复完照常往下走；
     * 挂在入口而不是下面那几条失败出口上，见 IST8310_ServiceRecover 的说明
     * ——"链路彻底坏了、每次 Sample 都失败"正是最该恢复的场景，而它在失败出口上永远
     * 走不到恢复（每条出口都直接 return 了）。 */
    IST8310_ServiceRecover(inst);

    if (IST8310Request(inst) != 0)
    {
        return (IST8310_Data_t){0};
    }

    /* 先等满器件要求的最小测量间隔，再去查就绪：否则头几次查都是白跑的 I2C */
    DWT_Delay((float)inst->meas_delay_ms / 1000.0f);

    uint64_t start_us = DWT_GetTimeUs();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000ull;
    uint8_t err_streak = 0;

    while (1)
    {
        int8_t ready = IST8310CheckReady(inst);
        if (ready > 0)
        {
            break; /* 就绪：交给 IST8310Read 去读，读路径只有一处 */
        }
        if (ready < 0)
        {
            /* CheckReady 已记过失败。一两次 NACK 可能只是总线上的噪声，
             * 连着失败就没必要陪它耗满超时了 —— 直接作废这一帧 */
            if (++err_streak >= IST8310_RECOVER_FAIL_TH)
            {
                BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Sample aborted: %d consecutive status read failures", (int)err_streak);
                return (IST8310_Data_t){0};
            }
        }
        else
        {
            err_streak = 0;
        }

        if ((DWT_GetTimeUs() - start_us) > timeout_us)
        {
            BSPLOG(&g_ist8310_log, LOG_LEVEL_WARNING, "Sample timeout (%dms), no valid frame", (int)timeout_ms);
            /* 拉低 DRDY，把器件放回干净状态（详见本函数说明） */
            (void)IST8310_ReadReg(inst, IST8310_DATAXL_REG, IST8310_DATA_LEN,
                                  inst->i2c_mode, inst->i2c_timeout_ms);
            inst->data_ready = 0;
            IST8310_NoteFailure(inst);
            return (IST8310_Data_t){0};
        }

        DWT_Delay((float)poll_interval_ms / 1000.0f);
    }

    return IST8310Read(inst);
}

#endif /* defined(HAL_I2C_MODULE_ENABLED) && defined(HAL_GPIO_MODULE_ENABLED) */

#endif /* DRV_IST8310_USED */

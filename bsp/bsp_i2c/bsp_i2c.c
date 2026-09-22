/**
 * @file bsp_i2c.c
 * @brief I2C驱动封装实现
 *
 * @note 只负责实例管理和回调分发，不负责硬件配置和从机地址管理
 */

#include "bsp_i2c.h"
#include "app_cfg.h"

#ifdef BSP_I2C_USED

#if I2C_INSTANCE_NUM > 0

#include "bsp_dwt.h"
#include "bsp_log.h"

/*------------- 私有类型定义 --------------*/

/**
 * @brief 主机发送函数指针类型（统一5参数，IT/DMA忽略timeout）
 * @note HAL_I2C_Master_Transmit 的 pData 参数是 uint8_t*
 */
typedef HAL_StatusTypeDef (*I2C_MasterTxFunc)(I2C_HandleTypeDef *, uint16_t, const uint8_t *, uint16_t, uint32_t);

/**
 * @brief 主机接收函数指针类型
 */
typedef HAL_StatusTypeDef (*I2C_MasterRxFunc)(I2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);

/**
 * @brief 寄存器读函数指针类型（统一7参数，IT/DMA忽略timeout）
 */
typedef HAL_StatusTypeDef (*I2C_MemReadFunc)(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, uint8_t *, uint16_t, uint32_t);

/**
 * @brief 寄存器写函数指针类型
 * @note HAL_I2C_Mem_Write 的 pData 参数是 uint8_t*
 */
typedef HAL_StatusTypeDef (*I2C_MemWriteFunc)(I2C_HandleTypeDef *, uint16_t, uint16_t, uint16_t, const uint8_t *, uint16_t, uint32_t);

/*------------- 私有变量 --------------*/

static uint8_t s_i2c_idx = 0;
#ifndef BSP_I2C_LOG_LIMIT
#define BSP_I2C_LOG_LIMIT 10
#endif                                                     // !BSP_I2C_LOG_LIMIT
LOG_INSTANCE_DEF(g_i2c_log, "bsp_i2c", BSP_I2C_LOG_LIMIT); /* I2C 日志实例 */
static I2CInstance *s_i2c_instance[I2C_INSTANCE_NUM] = {NULL};
static I2CInstance *s_i2c_last_route = NULL;

/*------------- 函数指针数组 --------------*/

/**
 * @brief 主机发送函数指针数组
 */
static const I2C_MasterTxFunc s_master_tx_funcs[] = {
    [I2C_BLOCK_MODE] = (I2C_MasterTxFunc)HAL_I2C_Master_Transmit,
    [I2C_IT_MODE] = (I2C_MasterTxFunc)HAL_I2C_Master_Transmit_IT,
    [I2C_DMA_MODE] = (I2C_MasterTxFunc)HAL_I2C_Master_Transmit_DMA,
};

/**
 * @brief 主机接收函数指针数组
 */
static const I2C_MasterRxFunc s_master_rx_funcs[] = {
    [I2C_BLOCK_MODE] = HAL_I2C_Master_Receive,
    [I2C_IT_MODE] = (I2C_MasterRxFunc)HAL_I2C_Master_Receive_IT,
    [I2C_DMA_MODE] = (I2C_MasterRxFunc)HAL_I2C_Master_Receive_DMA,
};

/**
 * @brief 寄存器读函数指针数组
 */
static const I2C_MemReadFunc s_mem_read_funcs[] = {
    [I2C_BLOCK_MODE] = HAL_I2C_Mem_Read,
    [I2C_IT_MODE] = (I2C_MemReadFunc)HAL_I2C_Mem_Read_IT,
    [I2C_DMA_MODE] = (I2C_MemReadFunc)HAL_I2C_Mem_Read_DMA,
};

/**
 * @brief 寄存器写函数指针数组
 */
static const I2C_MemWriteFunc s_mem_write_funcs[] = {
    [I2C_BLOCK_MODE] = (I2C_MemWriteFunc)HAL_I2C_Mem_Write,
    [I2C_IT_MODE] = (I2C_MemWriteFunc)HAL_I2C_Mem_Write_IT,
    [I2C_DMA_MODE] = (I2C_MemWriteFunc)HAL_I2C_Mem_Write_DMA,
};

/*------------- 私有函数声明 --------------*/

static I2CInstance *I2CFindInstanceByHandle(I2C_HandleTypeDef *hi2c);
static void I2C_ResetHandle(I2CInstance *instance);
static void I2C_AbortOnError(I2CInstance *instance, const char *reason);
static int8_t I2C_WaitReady(I2CInstance *instance, uint32_t timeout_ms, const char *reason);
static uint16_t I2C_ToHalMemAddrSize(I2C_MemAddrSize_e mem_addr_size);

/*------------- 私有函数实现 --------------*/

/**
 * @brief 根据 I2C 句柄查找实例（带最近命中缓存）
 */
static I2CInstance *I2CFindInstanceByHandle(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == NULL)
    {
        return NULL;
    }

    if (s_i2c_last_route != NULL && s_i2c_last_route->handle == hi2c)
    {
        return s_i2c_last_route;
    }

    for (uint8_t i = 0; i < s_i2c_idx; i++)
    {
        if (s_i2c_instance[i]->handle == hi2c)
        {
            s_i2c_last_route = s_i2c_instance[i];
            return s_i2c_instance[i];
        }
    }

    return NULL;
}

/**
 * @brief 当前是否运行在中断上下文
 * @note 用来把"会阻塞的收尾动作"挡在中断之外。CMSIS 的 IPSR 非 0 即处于异常处理中。
 */
static inline uint8_t I2C_InIsr(void)
{
    return (__get_IPSR() != 0U) ? 1 : 0;
}

/**
 * @brief 把 I2C 句柄恢复成"空闲可用"的兜底状态
 *
 * @note 与 SPI 不同，I2C 的 HAL 在正常完成/错误回调前多数分支已复位 State 并解锁，
 *       但仍有几处只置 ErrorCode 就返回（尤其等待 BUSY 标志超时的分支），
 *       会把句柄留在 HAL_I2C_STATE_BUSY_* 且 Lock 残留，使后续调用直接返回 HAL_BUSY。
 *       这里统一兜底，两处调用点（AbortOnError / BusRecover）共用。
 * @note PreviousState 的"空值"是 HAL 私有宏 I2C_STATE_NONE，其定义就等于
 *       HAL_I2C_MODE_NONE（见 F4/H7 的 stm32xxxx_hal_i2c.c），此处直接写后者。
 * @note XferISR 是 H7 独有字段（F4 句柄里没有），故按内核条件编译。
 */
static void I2C_ResetHandle(I2CInstance *instance)
{
    I2C_HandleTypeDef *h = instance->handle;

    /* 停掉异步传输与中断源：不关的话残留在途传输会在复位后又把状态改回 BUSY。
     * 但中断上下文里必须跳过 DMA 中止 —— HAL_DMA_Abort 在 State==BUSY 且 DMA 的
     * EN 位不落时用 HAL_GetTick() 死等（F4 里是 while(...) + HAL_TIMEOUT_DMA_ABORT），
     * 而 tick 中断优先级低于 EXTI，中断里 HAL_GetTick() 根本不前进 → 那个 while
     * 就是死循环。中断里本也不该有用 DMA 在飞的传输（DMA 模式禁止从 ISR 发起），
     * 真卡住了交给任务上下文的 I2CBusRecover 收尾。 */
    if (!I2C_InIsr())
    {
        if (h->hdmarx != NULL)
        {
            (void)HAL_DMA_Abort(h->hdmarx);
        }
        if (h->hdmatx != NULL)
        {
            (void)HAL_DMA_Abort(h->hdmatx);
        }
    }

#if defined(CPU_CORE) && (CPU_CORE == CORTEX_M7)
    /* H7：中断源在 CR1，逐个关掉 */
    __HAL_I2C_DISABLE_IT(h, I2C_IT_ERRI | I2C_IT_TCI | I2C_IT_STOPI | I2C_IT_NACKI |
                                I2C_IT_ADDRI | I2C_IT_RXI | I2C_IT_TXI);
    h->XferISR = NULL;
#else
    /* F4：中断源在 CR2 */
    __HAL_I2C_DISABLE_IT(h, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);
#endif

    h->State = HAL_I2C_STATE_READY;
    h->Mode = HAL_I2C_MODE_NONE;
    h->PreviousState = HAL_I2C_MODE_NONE;
    h->ErrorCode = HAL_I2C_ERROR_NONE;
    h->Lock = HAL_UNLOCKED;
}

/**
 * @brief 外设级重建：DeInit + Init，真正把卡住的 I2C 松开
 *
 * @note 为什么光复位结构体字段不够：HAL 在传输中途出错时硬件 BUSY 标志可能仍置位，
 *       此后每次调用一进 HAL 就立刻返回 HAL_BUSY（F4 的阻塞版还会先在
 *       I2C_TIMEOUT_BUSY_FLAG 上白等 25ms 才认输），形成"每次调用都失败"的死循环，
 *       外部现象是"I2C 从机永久失联"。
 *       `HAL_I2C_DeInit` 会 `__HAL_I2C_DISABLE` 掉 PE（这是 HAL 自己在
 *       I2C_ITError/I2C_DMAAbort 里用的同一手法）并调 MspDeInit 关时钟、放引脚；
 *       `HAL_I2C_Init` 再按 hi2c->Init 的原始值重建外设。这是不翻转 SCL/SDA 的前提下
 *       唯一能真正松开总线的路径，与 I2CBusRecover 是同一套动作。
 * @note 含 MspDeInit/MspInit（时钟、GPIO、NVIC 重配），**只能在任务上下文调用**。
 */
static void I2C_RebuildPeriph(I2CInstance *instance)
{
    I2C_HandleTypeDef *h = instance->handle;

    if (HAL_I2C_DeInit(h) != HAL_OK)
    {
        BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "HAL_I2C_DeInit failed");
    }
    if (HAL_I2C_Init(h) != HAL_OK)
    {
        BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "HAL_I2C_Init failed");
    }
}

/**
 * @brief 传输失败收尾：停掉异步传输、复位 HAL 状态、通知上层
 * @param instance I2C实例
 * @param reason   失败原因（仅日志）
 *
 * @note 必须做这一步的原因：本文件"等待 State==READY"的循环超时后，若不复位，
 *       句柄永久停在 HAL_I2C_STATE_BUSY_*，而 DRV 层等不到完成回调、其传输标志
 *       也永远清不掉 → 整个从机永久失联。
 * @note 不在这里调 HAL_I2C_Master_Abort_IT：它是异步的、依赖 I2C 中断，
 *       总线卡死时永远不会完成（等于二次卡死）；且本函数可能运行在错误回调的
 *       中断上下文里。
 */
static void I2C_AbortOnError(I2CInstance *instance, const char *reason)
{
    BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "%s (i2c_e=%d), state=0x%02X, err=0x%lX, lock=%d",
           reason, (int)instance->i2c_e, (unsigned)instance->handle->State,
           instance->handle->ErrorCode, (int)instance->handle->Lock);

    I2C_ResetHandle(instance);

    /* 硬件 BUSY 兜底：只复位软件字段救不了"外设还占着总线"的情况（见 I2C_RebuildPeriph）。
     * 仅在对总线真的还 busy 时才重建 —— 常见的 NACK / 从机不在 之类错误之后总线是空闲的，
     * 不做无谓的时钟与 GPIO 重建。
     * 中断上下文里不重建（含 MspDeInit/MspInit），留给任务上下文：drv_ist8310 攒够失败
     * 次数后会调 I2CBusRecover，走的正是同一条重建路径。 */
    if (!I2C_InIsr() && (__HAL_I2C_GET_FLAG(instance->handle, I2C_FLAG_BUSY) == SET))
    {
        BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Bus still busy after abort, rebuilding peripheral");
        I2C_RebuildPeriph(instance);
        I2C_ResetHandle(instance);
    }

    if (instance->err_callback != NULL)
    {
        instance->err_callback(instance);
    }
}

/**
 * @brief IT/DMA 模式下等待总线就绪
 * @retval 0 就绪；-1 超时（已调用 I2C_AbortOnError）
 * @note 阻塞模式由 HAL 自己等，此处直接放行
 * @note timeout_ms 传 0 在 HAL 语义里是"无限等待"。阻塞模式下那只是个很长的等待，
 *       但异步模式的本循环是 **busy-wait**，若它跑在中断上下文（例如 DRDY EXTI 里
 *       发起 IT 传输）就等于死循环。故异步模式明确拒绝 0 超时。
 */
static int8_t I2C_WaitReady(I2CInstance *instance, uint32_t timeout_ms, const char *reason)
{
    if (instance->work_mode == I2C_BLOCK_MODE)
    {
        return 0;
    }

    BSP_RETURN_IF_TRUE_LOG(timeout_ms == 0, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Async mode requires a non-zero timeout!"));

    uint64_t start_time = DWT_GetTimeUs();
    uint64_t timeout_us = (uint64_t)timeout_ms * 1000;

    while (instance->handle->State != HAL_I2C_STATE_READY)
    {
        if (timeout_ms > 0 && (DWT_GetTimeUs() - start_time) > timeout_us)
        {
            /* 等不到就绪同样是"传输没发出去"，必须通知上层，否则其传输标志永久卡死 */
            I2C_AbortOnError(instance, reason);
            return -1;
        }
    }

    return 0;
}

/**
 * @brief 寄存器地址宽度枚举 → HAL 常量
 */
static uint16_t I2C_ToHalMemAddrSize(I2C_MemAddrSize_e mem_addr_size)
{
    return (mem_addr_size == I2C_MEM_ADDR_SIZE_16BIT) ? I2C_MEMADD_SIZE_16BIT : I2C_MEMADD_SIZE_8BIT;
}

/*------------- HAL回调函数重写 --------------*/

/**
 * @brief 主机发送完成回调
 * @note 发送完成也调用 rx_callback，保持接口统一（与 bsp_spi 一致）
 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2CInstance *instance = I2CFindInstanceByHandle(hi2c);
    if (instance != NULL)
    {
        instance->rx_len = 0;
        if (instance->rx_callback != NULL)
        {
            instance->rx_callback(instance);
        }
    }
}

/**
 * @brief 主机接收完成回调
 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2CInstance *instance = I2CFindInstanceByHandle(hi2c);
    if (instance != NULL)
    {
        instance->rx_len = instance->last_xfer_len;
        if (instance->rx_callback != NULL)
        {
            instance->rx_callback(instance);
        }
    }
}

/**
 * @brief 寄存器写完成回调
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2CInstance *instance = I2CFindInstanceByHandle(hi2c);
    if (instance != NULL)
    {
        instance->rx_len = 0;
        if (instance->rx_callback != NULL)
        {
            instance->rx_callback(instance);
        }
    }
}

/**
 * @brief 寄存器读完成回调
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    I2CInstance *instance = I2CFindInstanceByHandle(hi2c);
    if (instance != NULL)
    {
        instance->rx_len = instance->last_xfer_len;
        if (instance->rx_callback != NULL)
        {
            instance->rx_callback(instance);
        }
    }
}

/**
 * @brief I2C错误回调
 * @note HAL 的错误路径在调用本回调前已复位 State 并解锁，故这里只解码日志 +
 *       通知上层，不在中断上下文里再做 DMA/Abort 收尾
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    I2CInstance *instance = I2CFindInstanceByHandle(hi2c);
    if (instance != NULL)
    {
        /* 直接展开 hi2c->ErrorCode 而不取局部变量：BSPLOG 在日志关闭时会被
         * 整个编译掉，局部变量就成了未使用变量并触发 -Wunused-variable */
        BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Error detected, code=0x%lX (BERR:%d ARLO:%d AF:%d OVR:%d DMA:%d TIMEOUT:%d)",
               hi2c->ErrorCode,
               (hi2c->ErrorCode & HAL_I2C_ERROR_BERR) ? 1 : 0,     // 总线错误（起始/停止时序错乱）
               (hi2c->ErrorCode & HAL_I2C_ERROR_ARLO) ? 1 : 0,     // 仲裁丢失
               (hi2c->ErrorCode & HAL_I2C_ERROR_AF) ? 1 : 0,       // 从机 NACK（设备不在/地址错）
               (hi2c->ErrorCode & HAL_I2C_ERROR_OVR) ? 1 : 0,      // 溢出
               (hi2c->ErrorCode & HAL_I2C_ERROR_DMA) ? 1 : 0,      // DMA错误
               (hi2c->ErrorCode & HAL_I2C_ERROR_TIMEOUT) ? 1 : 0); // 超时

        if (instance->err_callback != NULL)
        {
            instance->err_callback(instance);
        }
    }
}

/**
 * @brief I2C地址匹配回调（仅从机/Listen 模式会触发）
 * @note BSP 层只做主模式，正常不会进来；重写为日志便于发现误配
 */
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
    I2CInstance *instance = I2CFindInstanceByHandle(hi2c);
    if (instance == NULL)
    {
        return;
    }

    BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Unexpected AddrCallback (dir=%d, code=0x%X), check work mode!",
           (int)TransferDirection, (unsigned)AddrMatchCode);
}

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册I2C实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 I2CConfig 负责）
 */
int8_t I2CRegister(I2CInstance *instance)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_i2c_idx >= I2C_INSTANCE_NUM, -1, BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_i2c_idx; i++)
    {
        if (s_i2c_instance[i] == instance)
        {
            BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance already registered!");
            return -1;
        }
    }

    s_i2c_instance[s_i2c_idx++] = instance;

    BSPLOG(&g_i2c_log, LOG_LEVEL_INFO, "I2C instance registered, idx=%d", s_i2c_idx - 1);
    return 0;
}

/**
 * @brief 配置I2C实例（填充硬件句柄 + 工作模式 + 回调，可重复调用）
 * @note 要求先调用 I2CRegister 注册实例
 */
int8_t I2CConfig(I2CInstance *instance, const I2C_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->i2c_e >= I2C_NUM_MAX, -1, BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "i2c_e out of range!"));
    BSP_RETURN_IF_TRUE_LOG(config->work_mode != I2C_BLOCK_MODE && config->work_mode != I2C_IT_MODE && config->work_mode != I2C_DMA_MODE, -1, BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid work_mode=%d!", config->work_mode));

    // 填充枚举和硬件句柄
    instance->i2c_e = config->i2c_e;
    instance->handle = i2c_map[instance->i2c_e].handle;

    BSP_RETURN_IF_TRUE_LOG(instance->handle == NULL, -1, BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "I2C handle is NULL, check CubeMX configuration!"));

    instance->work_mode = config->work_mode;
    instance->rx_callback = config->rx_callback;
    instance->err_callback = config->err_callback;

    return 0;
}

int8_t I2CMemRead(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                  I2C_MemAddrSize_e mem_addr_size, uint16_t len, uint32_t timeout_ms)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL || instance->handle == NULL, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid instance!"));
    BSP_RETURN_IF_TRUE_LOG(len == 0 || len > instance->buff_size, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "len=%d out of range (buff_size=%d)!",
                                  (int)len, (int)instance->buff_size));
    BSP_RETURN_IF_TRUE_LOG(mem_addr_size != I2C_MEM_ADDR_SIZE_8BIT && mem_addr_size != I2C_MEM_ADDR_SIZE_16BIT, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid mem_addr_size=%d!", (int)mem_addr_size));

    if (I2C_WaitReady(instance, timeout_ms, "I2C mem read busy timeout") != 0)
    {
        return -1;
    }

    instance->last_dev_addr = dev_addr;
    instance->last_mem_addr = mem_addr;
    instance->last_xfer_len = len;
    instance->rx_len = 0;

    if (s_mem_read_funcs[instance->work_mode](instance->handle, dev_addr, mem_addr,
                                              I2C_ToHalMemAddrSize(mem_addr_size),
                                              instance->rx_buff, len, timeout_ms) != HAL_OK)
    {
        /* 启动失败不会有完成回调，必须在这里复位并把失败抛给上层 */
        I2C_AbortOnError(instance, "I2C mem read start failed");
        return -1;
    }

    if (instance->work_mode == I2C_BLOCK_MODE)
    {
        instance->rx_len = len; // 阻塞模式：返回即完成
    }
    return 0;
}

int8_t I2CMemWrite(I2CInstance *instance, uint16_t dev_addr, uint16_t mem_addr,
                   I2C_MemAddrSize_e mem_addr_size, const uint8_t *data, uint16_t len,
                   uint32_t timeout_ms)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL || instance->handle == NULL, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid instance!"));
    BSP_RETURN_IF_TRUE_LOG(data == NULL || len == 0, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Invalid write data/len!"));
    BSP_RETURN_IF_TRUE_LOG(mem_addr_size != I2C_MEM_ADDR_SIZE_8BIT && mem_addr_size != I2C_MEM_ADDR_SIZE_16BIT, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid mem_addr_size=%d!", (int)mem_addr_size));

    if (I2C_WaitReady(instance, timeout_ms, "I2C mem write busy timeout") != 0)
    {
        return -1;
    }

    instance->last_dev_addr = dev_addr;
    instance->last_mem_addr = mem_addr;
    instance->last_xfer_len = len;
    instance->rx_len = 0;

    if (s_mem_write_funcs[instance->work_mode](instance->handle, dev_addr, mem_addr,
                                               I2C_ToHalMemAddrSize(mem_addr_size),
                                               data, len, timeout_ms) != HAL_OK)
    {
        I2C_AbortOnError(instance, "I2C mem write start failed");
        return -1;
    }

    return 0;
}

int8_t I2CMasterTransmit(I2CInstance *instance, uint16_t dev_addr, const uint8_t *data,
                         uint16_t len, uint32_t timeout_ms)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL || instance->handle == NULL, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid instance!"));
    BSP_RETURN_IF_TRUE_LOG(data == NULL || len == 0, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Invalid transmit parameters!"));

    if (I2C_WaitReady(instance, timeout_ms, "I2C transmit busy timeout") != 0)
    {
        return -1;
    }

    instance->last_dev_addr = dev_addr;
    instance->last_xfer_len = len;
    instance->rx_len = 0;

    /* 注意：从机地址自身占一笔"数据"，故 HAL 的 Size 就是 len（不含地址字节） */
    if (s_master_tx_funcs[instance->work_mode](instance->handle, dev_addr, data, len, timeout_ms) != HAL_OK)
    {
        I2C_AbortOnError(instance, "I2C transmit start failed");
        return -1;
    }

    return 0;
}

int8_t I2CMasterReceive(I2CInstance *instance, uint16_t dev_addr, uint16_t len,
                        uint32_t timeout_ms)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL || instance->handle == NULL, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid instance!"));
    BSP_RETURN_IF_TRUE_LOG(len == 0 || len > instance->buff_size, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "len=%d out of range (buff_size=%d)!",
                                  (int)len, (int)instance->buff_size));

    if (I2C_WaitReady(instance, timeout_ms, "I2C receive busy timeout") != 0)
    {
        return -1;
    }

    instance->last_dev_addr = dev_addr;
    instance->last_xfer_len = len;
    instance->rx_len = 0;

    if (s_master_rx_funcs[instance->work_mode](instance->handle, dev_addr,
                                               instance->rx_buff, len, timeout_ms) != HAL_OK)
    {
        I2C_AbortOnError(instance, "I2C receive start failed");
        return -1;
    }

    if (instance->work_mode == I2C_BLOCK_MODE)
    {
        instance->rx_len = len; // 阻塞模式：返回即完成
    }
    return 0;
}

int8_t I2CIsDeviceReady(I2CInstance *instance, uint16_t dev_addr, uint32_t trials,
                        uint32_t timeout_ms)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL || instance->handle == NULL, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "Invalid instance!"));

    if (I2C_WaitReady(instance, timeout_ms, "I2C IsDeviceReady busy timeout") != 0)
    {
        return -1;
    }

    return (HAL_I2C_IsDeviceReady(instance->handle, dev_addr, trials, timeout_ms) == HAL_OK) ? 0 : -1;
}

int8_t I2CBusRecover(I2CInstance *instance)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL || instance->handle == NULL, -1,
                           BSPLOG(&g_i2c_log, LOG_LEVEL_ERROR, "BusRecover: invalid instance!"));

    I2C_HandleTypeDef *h = instance->handle;

    BSPLOG(&g_i2c_log, LOG_LEVEL_WARNING, "Bus recover start (i2c_e=%d), state=0x%02X, err=0x%lX",
           (int)instance->i2c_e, (unsigned)h->State, h->ErrorCode);

    /* 1) 先让外设静默 + 解锁 + 清状态（Lock 残留会让后续 HAL 调用直接 HAL_BUSY） */
    I2C_ResetHandle(instance);

    /* 2) 外设级重建（与 I2C_AbortOnError 的兜底共用一份实现，见 I2C_RebuildPeriph） */
    I2C_RebuildPeriph(instance);

    /* 3) 兜底复位（Init 正常时已复位，防 Init 失败留下脏状态） */
    I2C_ResetHandle(instance);

    /* 4) 用 BUSY 标志判定总线是否真被引脚拉死（从机把 SCL/SDA 一直拉低） */
    {
        uint8_t busy = (__HAL_I2C_GET_FLAG(h, I2C_FLAG_BUSY) == SET) ? 1 : 0;
        BSPLOG(&g_i2c_log, busy ? LOG_LEVEL_ERROR : LOG_LEVEL_INFO,
               "Bus recover done, busy=%d (1 = 引脚被从机拉死，需从机侧复位)", busy);
        return busy ? -1 : 0;
    }
}

#endif /* I2C_INSTANCE_NUM > 0 */

#endif /* BSP_I2C_USED */

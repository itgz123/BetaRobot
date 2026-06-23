/**
 * @file drv_sbus.c
 * @brief SBUS 遥控器驱动实现
 *
 * @note DRV 层职责：
 *       1. BSP 回调中解析 SBUS 数据并传递到 APP 层
 *       2. 不使用 FreeRTOS（队列由 APP 层管理）
 */

#include "drv_sbus.h"

#ifdef HAL_UART_MODULE_ENABLED

#include "bsp_uart_log.h"
#include "bsp_dwt.h"

// 函数声明
static void SBUSUARTRxCallback(USARTInstance *usart_inst);

/*------------- daemon回调函数实现 --------------*/
static void SBUSDaemonCallback(void *owner)
{
    if (!owner)
        return;

    SBUSInstance *sbus_inst = (SBUSInstance *)owner;
    // daemon 超时说明连续收不到数据，直接标志信号丢失
    sbus_inst->signal_lost = 1;
}

/*------------- 外部接口实现 --------------*/

int8_t SBUSRegister(SBUSInstance *instance, const SBUS_Init_Config_s *config)
{
    if (instance == NULL)
    {
        LOGERROR("[drv_sbus] Instance is NULL!");
        return -1;
    }

    if (config == NULL)
    {
        LOGERROR("[drv_sbus] Config is NULL!");
        return -1;
    }

    if (instance->usart_inst == NULL)
    {
        LOGERROR("[drv_sbus] usart_inst is NULL!");
        return -1;
    }

    // 设置 parent 指针，用于 BSP 回调时获取 DRV 实例
    instance->usart_inst->parent = instance;

    // 注册 BSP 层 USART 实例
    USART_Init_Config_s usart_cfg = {
        .uart_e = config->uart_e,
        .tx_mode = USART_DMA_MODE,
        .rx_callback = SBUSUARTRxCallback,
    };
    if (USARTRegister(instance->usart_inst, &usart_cfg) != 0)
    {
        LOGERROR("[drv_sbus] USART register failed!");
        return -1;
    }

    // 注册 daemon 看门狗
    if (instance->daemon != NULL)
    {
        Daemon_Init_Config_s daemon_cfg = {
            .reload_count = config->daemon_reload,
            .fault_action = config->daemon_fault,
            .callback = SBUSDaemonCallback,
            .owner_id = instance,
        };
        DaemonRegister(instance->daemon, &daemon_cfg);
    }

    // 初始化信号丢失超时
    instance->lost_timeout_us = (uint64_t)config->lost_timeout_ms * 1000;

    instance->lost_start_time_us = 0;
    instance->signal_lost = 0;

    LOGINFO("[drv_sbus] SBUS instance registered, lost_timeout=%dms",
            (int)(instance->lost_timeout_us / 1000));
    return 0;
}

static SBUS_Data_t SBUSDecodeFrame(const uint8_t *data, uint16_t len)
{
    SBUS_Data_t result = {0};

    // 参数检查
    if (data == NULL || len < SBUS_FRAME_SIZE)
    {
        LOGWARNING("[drv_sbus] Invalid frame data, len=%d", len);
        result.frame_lost = 1;
        return result;
    }

    // 帧头检查
    if (data[0] != SBUS_HEADER)
    {
        LOGWARNING("[drv_sbus] Invalid frame header: 0x%02X", data[0]);
        result.frame_lost = 1;
        return result;
    }

    // 帧尾检查
    if (data[24] != SBUS_FOOTER && data[24] != SBUS_FOOTER_FRAME_LOST && data[24] != SBUS_FOOTER_FAILSAFE)
    {
        LOGWARNING("[drv_sbus] Invalid frame footer: 0x%02X", data[24]);
        result.frame_lost = 1;
        return result;
    }

    // 解析 16 个模拟通道（每通道 11 位）并归一化到 -1.0 ~ 1.0
    // 通道数据从字节 1 开始，到字节 22 结束
    // 归一化公式：(raw - center) / (max - center)
    static const float scale = 1.0f / (float)(SBUS_CH_MAX - SBUS_CH_CENTER);

    uint16_t raw;
    raw = (data[1] | (data[2] << 8)) & 0x07FF;
    result.ch[0] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[2] >> 3 | (data[3] << 5)) & 0x07FF;
    result.ch[1] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[3] >> 6 | (data[4] << 2) | (data[5] << 10)) & 0x07FF;
    result.ch[2] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[5] >> 1 | (data[6] << 7)) & 0x07FF;
    result.ch[3] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[6] >> 4 | (data[7] << 4)) & 0x07FF;
    result.ch[4] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[7] >> 7 | (data[8] << 1) | (data[9] << 9)) & 0x07FF;
    result.ch[5] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[9] >> 2 | (data[10] << 6)) & 0x07FF;
    result.ch[6] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[10] >> 5 | (data[11] << 3)) & 0x07FF;
    result.ch[7] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[12] | (data[13] << 8)) & 0x07FF;
    result.ch[8] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[13] >> 3 | (data[14] << 5)) & 0x07FF;
    result.ch[9] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[14] >> 6 | (data[15] << 2) | (data[16] << 10)) & 0x07FF;
    result.ch[10] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[16] >> 1 | (data[17] << 7)) & 0x07FF;
    result.ch[11] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[17] >> 4 | (data[18] << 4)) & 0x07FF;
    result.ch[12] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[18] >> 7 | (data[19] << 1) | (data[20] << 9)) & 0x07FF;
    result.ch[13] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[20] >> 2 | (data[21] << 6)) & 0x07FF;
    result.ch[14] = (float)(raw - SBUS_CH_CENTER) * scale;
    raw = (data[21] >> 5 | (data[22] << 3)) & 0x07FF;
    result.ch[15] = (float)(raw - SBUS_CH_CENTER) * scale;

    // 解析数字通道和标志位（字节 23）
    // bit 0: ch17 (数字通道 1)
    // bit 1: ch18 (数字通道 2)
    // bit 2: frame_lost (帧丢失)
    // bit 3: failsafe (失控保护)
    result.digital_ch[0] = (data[23] & 0x01) ? 1 : 0;
    result.digital_ch[1] = (data[23] & 0x02) ? 1 : 0;
    result.frame_lost = (data[23] & 0x04) ? 1 : 0;
    result.failsafe = (data[23] & 0x08) ? 1 : 0;

    // 帧尾标志也包含状态信息
    if (data[24] == SBUS_FOOTER_FRAME_LOST)
    {
        result.frame_lost = 1;
    }
    else if (data[24] == SBUS_FOOTER_FAILSAFE)
    {
        result.failsafe = 1;
    }

    return result;
}

/*------------- 私有函数实现 --------------*/

/**
 * @brief BSP 层 UART 接收回调
 * @param usart_inst USART 实例指针
 * @note 通过 parent 字段获取 SBUSInstance，调用 APP 回调
 */
static void SBUSUARTRxCallback(USARTInstance *usart_inst)
{
    // 参数检查
    if (usart_inst == NULL)
    {
        return;
    }

    // 检查帧长度
    if (usart_inst->rx_len != SBUS_FRAME_SIZE)
    {
        LOGWARNING("[drv_sbus] Frame length error: %d (expected %d)", usart_inst->rx_len, SBUS_FRAME_SIZE);
        return;
    }

    // 通过 parent 字段获取 SBUSInstance 指针
    SBUSInstance *sbus_inst = (SBUSInstance *)usart_inst->parent;

    // 调用 APP 层回调（传递解析后的数据）
    if (sbus_inst != NULL)
    {
        // 在中断上下文中解析原始数据为通道数据
        sbus_inst->sbus_data = SBUSDecodeFrame(usart_inst->rx_buff, usart_inst->rx_len);
        DaemonReload(sbus_inst->daemon);

        // ---- 信号丢失超时检测 ----
        if (sbus_inst->sbus_data.frame_lost || sbus_inst->sbus_data.failsafe)
        {
            // 丢帧/失控状态：如果尚未计时则记录时间戳
            if (sbus_inst->lost_start_time_us == 0)
            {
                sbus_inst->lost_start_time_us = DWT_GetTimeUs();
            }

            // 检查是否超过超时时间
            if ((DWT_GetTimeUs() - sbus_inst->lost_start_time_us) >= sbus_inst->lost_timeout_us)
            {
                sbus_inst->signal_lost = 1;
            }
        }
        else
        {
            // 信号恢复正常：清除计时和丢失标志
            sbus_inst->lost_start_time_us = 0;
            sbus_inst->signal_lost = 0;
        }
    }
}

#endif /* HAL_UART_MODULE_ENABLED */

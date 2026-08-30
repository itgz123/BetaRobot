/**
 * @file drv_dbus.c
 * @brief DBUS 遥控器驱动实现
 *
 * @note DRV 层职责：
 *       1. BSP 回调中解析 DBUS 数据并传递到 APP 层
 *       2. 不使用 FreeRTOS（队列由 APP 层管理）
 *
 * @note 协议参考 RoboMaster 遥控器（接收机）用户手册"字节控制帧结构"：
 *       帧长 18 字节，ch0~ch3 各 11bit、S1/S2 各 2bit、鼠标 3×16bit、
 *       鼠标左右键各 8bit、键盘 16bit、保留(拨轮) 16bit。
 */

#include "drv_dbus.h"
#include "app_cfg.h"

#ifdef DRV_DBUS_USED

#ifdef HAL_UART_MODULE_ENABLED

#include "bsp_log.h"
#include "bsp_dwt.h"

// 函数声明
static void DBUSUARTRxCallback(USARTInstance *usart_inst);

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册 DBUS 实例（仅调用一次）
 */
int8_t DBUSRegister(DBUSInstance *instance)
{
    if (instance == NULL)
    {
        LOGERROR("[drv_dbus] Instance is NULL!");
        return -1;
    }

    if (instance->usart_inst == NULL)
    {
        LOGERROR("[drv_dbus] usart_inst is NULL!");
        return -1;
    }

    // 注册 BSP 层 USART 实例（USARTRegister 自身有防重复检查）
    if (USARTRegister(instance->usart_inst) != 0)
    {
        LOGERROR("[drv_dbus] USART register failed!");
        return -1;
    }

    // 注册 daemon（占位，Config 更新运行参数）
    if (instance->daemon)
    {
        DaemonRegister(instance->daemon);
    }

    return 0;
}

/**
 * @brief 配置 DBUS 实例（可重复调用）
 */
int8_t DBUSConfig(DBUSInstance *instance, const DBUS_Config_s *config)
{
    if (instance == NULL)
    {
        LOGERROR("[drv_dbus] Instance is NULL!");
        return -1;
    }

    if (config == NULL)
    {
        LOGERROR("[drv_dbus] Config is NULL!");
        return -1;
    }

    if (instance->usart_inst == NULL)
    {
        LOGERROR("[drv_dbus] usart_inst is NULL!");
        return -1;
    }

    // 设置 parent 指针，用于 BSP 回调时获取 DRV 实例
    instance->usart_inst->parent = instance;

    // 配置 USART DMA 模式和接收回调
    USART_Config_s usart_cfg = {
        .uart_e = config->uart_e,
        .tx_mode = USART_DMA_MODE,
        .rx_callback = DBUSUARTRxCallback,
        .tx_callback = NULL,
    };
    if (USARTConfig(instance->usart_inst, &usart_cfg) != 0)
    {
        LOGERROR("[drv_dbus] USART config failed!");
        return -1;
    }

    // 更新 daemon 运行参数（可重入）
    if (instance->daemon)
    {
        Daemon_Config_s daemon_cfg = {
            .reload_count = config->daemon_reload,
            .fault_action = config->daemon_fault,
            .owner_id = instance,
        };
        DaemonConfig(instance->daemon, &daemon_cfg);
    }

    // 初始化信号丢失超时
    instance->lost_timeout_us = (uint64_t)config->lost_timeout_ms * 1000;
    instance->lost_start_time_us = 0;
    instance->signal_lost = 0;

    return 0;
}

static DBUS_Data_t DBUSDecodeFrame(const uint8_t *data, uint16_t len)
{
    DBUS_Data_t result = {0};

    // 参数检查
    if (data == NULL || len < DBUS_FRAME_SIZE)
    {
        LOGWARNING("[drv_dbus] Invalid frame data, len=%d", len);
        result.frame_lost = 1;
        return result;
    }

    // 解析 4 个摇杆通道（每通道 11 位）并归一化到 -1.0 ~ 1.0
    // 归一化公式：(raw - center) / (max - center)
    // 注意 ch2 涉及 data[4]<<10，中间量需用 uint32_t 防止 int 提升溢出
    static const float scale = 1.0f / (float)(DBUS_CH_MAX - DBUS_CH_CENTER);

    uint32_t raw_ch[DBUS_CHANNEL_COUNT];
    raw_ch[0] = (uint32_t)(data[0] | (data[1] << 8)) & 0x07FFu;                          //!< 位偏移 0
    raw_ch[1] = (uint32_t)((data[1] >> 3) | (data[2] << 5)) & 0x07FFu;                   //!< 位偏移 11
    raw_ch[2] = (uint32_t)((data[2] >> 6) | (data[3] << 2) | (data[4] << 10)) & 0x07FFu; //!< 位偏移 22
    raw_ch[3] = (uint32_t)((data[4] >> 1) | (data[5] << 7)) & 0x07FFu;                   //!< 位偏移 33

    for (uint8_t i = 0; i < DBUS_CHANNEL_COUNT; i++)
    {
        result.ch[i] = (float)((int32_t)raw_ch[i] - DBUS_CH_CENTER) * scale;
    }

    // 开关 S1/S2（字节 5 的高 4 位，位偏移 44/46）
    result.s2 = (DBUS_SW_e)((data[5] >> 4) & 0x03);        //!< S2 低位 (偏移 44)
    result.s1 = (DBUS_SW_e)(((data[5] >> 4) & 0x0C) >> 2); //!< S1 高位 (偏移 46)

    // 鼠标解析（16 位有符号，位偏移 48/64/80）
    result.mouse_x = (int16_t)(data[6] | (data[7] << 8));
    result.mouse_y = (int16_t)(data[8] | (data[9] << 8));
    result.mouse_z = (int16_t)(data[10] | (data[11] << 8));
    result.mouse_press_l = data[12]; //!< 位偏移 96
    result.mouse_press_r = data[13]; //!< 位偏移 104

    // 键盘解析（16 位位域，位偏移 112，W/S/A/D/Q/E/Shift/Ctrl 在低 8 位）
    result.key.value = (uint16_t)(data[14] | (data[15] << 8));

    // 侧边拨轮（保留字段，位偏移 128，取 11 位，中心 1024，归一化）
    result.dial = (float)((int32_t)((uint16_t)(data[16] | (data[17] << 8)) & 0x07FFu) - DBUS_CH_CENTER) * scale;

    return result;
}

/*------------- 私有函数实现 --------------*/

/**
 * @brief BSP 层 UART 接收回调
 * @param usart_inst USART 实例指针
 * @note 通过 parent 字段获取 DBUSInstance，调用 APP 回调
 */
static void DBUSUARTRxCallback(USARTInstance *usart_inst)
{
    // 参数检查
    if (usart_inst == NULL)
    {
        return;
    }

    // 检查帧长度
    if (usart_inst->rx_len != DBUS_FRAME_SIZE)
    {
        LOGWARNING("[drv_dbus] Frame length error: %d (expected %d)", usart_inst->rx_len, DBUS_FRAME_SIZE);
        return;
    }

    // 通过 parent 字段获取 DBUSInstance 指针
    DBUSInstance *dbus_inst = (DBUSInstance *)usart_inst->parent;

    // 调用 APP 层回调（传递解析后的数据）
    if (dbus_inst != NULL)
    {
        // 在中断上下文中解析原始数据为通道数据
        dbus_inst->dbus_data = DBUSDecodeFrame(usart_inst->rx_buff, usart_inst->rx_len);
        DaemonReload(dbus_inst->daemon);

        // ---- 信号丢失超时检测 ----
        if (dbus_inst->dbus_data.frame_lost || dbus_inst->dbus_data.failsafe)
        {
            // 丢帧/失控状态：如果尚未计时则记录时间戳
            if (dbus_inst->lost_start_time_us == 0)
            {
                dbus_inst->lost_start_time_us = DWT_GetTimeUs();
            }

            // 检查是否超过超时时间
            if ((DWT_GetTimeUs() - dbus_inst->lost_start_time_us) >= dbus_inst->lost_timeout_us)
            {
                dbus_inst->signal_lost = 1;
            }
        }
        else
        {
            // 信号恢复正常：清除计时和丢失标志
            dbus_inst->lost_start_time_us = 0;
            dbus_inst->signal_lost = 0;
        }
    }
}

#endif /* HAL_UART_MODULE_ENABLED */

#endif /* DRV_DBUS_USED */

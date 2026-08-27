/**
 * @file bsp_bxcan.c
 * @brief BxCAN驱动封装实现（F4 平台，经典 CAN）
 *
 * @note 只负责实例管理和外设重配置，不负责滤波器/收发（后续实现）。
 *       本文件仅在 BSP_CAN_IP == BSP_CAN_IP_BXCAN 时编译。
 */

#include "bsp_can.h"
#include "app_cfg.h"

#ifdef BSP_CAN_USED

#if BSP_CAN_IP == BSP_CAN_IP_BXCAN

#include "bsp_assert.h"
#include "bsp_uart_log.h"

/*------------- 私有变量 --------------*/
static uint8_t s_can_idx = 0;
#if CAN_INSTANCE_NUM > 0
static CANInstance *s_can_instance[CAN_INSTANCE_NUM] = {NULL};
#else
static CANInstance **s_can_instance = NULL;
#endif

/*------------- 外部接口实现 --------------*/

/**
 * @brief 注册CAN实例（仅调用一次，修改 static 管理数组）
 * @note 仅注册，不配置硬件参数（由 CANConfig 负责）
 */
int8_t CANRegister(CANInstance *instance)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_can_idx >= CAN_INSTANCE_NUM, -1, LOGERROR("[bsp_can] Exceeded max instance count!"));

    // 防重复注册检查
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        if (s_can_instance[i] == instance)
        {
            LOGERROR("[bsp_can] Instance already registered!");
            return -1;
        }
    }

    s_can_instance[s_can_idx++] = instance;

    LOGINFO("[bsp_can] CAN Instance registered, idx=%d", s_can_idx - 1);
    return 0;
}

/**
 * @brief 配置CAN实例（填充硬件映射 + 工作模式 + 父指针，可重复调用）
 * @note 要求先调用 CANRegister 注册实例
 */
int8_t CANConfig(CANInstance *instance, const CAN_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_can] Config is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config->can_e >= CAN_NUM_MAX, -1, LOGERROR("[bsp_can] can_e out of range!"));

    // 填充枚举和硬件句柄
    instance->can_e = config->can_e;
    instance->map = can_map[instance->can_e];
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, -1, LOGERROR("[bsp_can] CAN handle is NULL, check bsp_map mapping!"));

    // 重复注册检查（同一 handle 不能重复注册）
    for (uint8_t i = 0; i < s_can_idx; i++)
    {
        if (s_can_instance[i] == instance)
            continue;
        if (s_can_instance[i]->map.handle == instance->map.handle)
        {
            LOGERROR("[bsp_can] Same CAN handle already registered!");
            return -1;
        }
    }

    // F4 BxCAN 仅支持经典 CAN（FD 帧需 H7 FDCAN），非 CLASSIC 一律拒绝
    BSP_RETURN_IF_TRUE_LOG(config->mode != CAN_FRAME_FORMAT_CLASSIC, -1, LOGERROR("[bsp_can] BxCAN only supports CLASSIC frame format (mode=%d)!", config->mode));

    instance->mode = config->mode;
    instance->parent = config->parent;
    instance->filter = config->filter; // 软件过滤器（结构体拷贝）

    // 首次配置：硬过滤全通 + 启动外设 + 使能接收中断（HAL_CAN_Start 后 State: READY → LISTENING）
    if (instance->map.handle->State == HAL_CAN_STATE_READY)
    {
        CAN_FilterTypeDef hw_filter = {0};
        uint32_t rx_it;

        hw_filter.FilterIdHigh = 0;
        hw_filter.FilterIdLow = 0;
        hw_filter.FilterMode = CAN_FILTERMODE_IDMASK;  // 掩码模式
        hw_filter.FilterScale = CAN_FILTERSCALE_32BIT; // 32位
        hw_filter.FilterMaskIdHigh = 0;
        hw_filter.FilterMaskIdLow = 0; // 掩码全 0 = 全通过
        hw_filter.FilterActivation = ENABLE;

        // CAN1 用 bank 0..13/FIFO0，CAN2 用 bank 14..27/FIFO1（SlaveStartFilterBank=14）
        if (instance->map.handle->Instance == CAN1)
        {
            hw_filter.FilterBank = 0;
            hw_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
            rx_it = CAN_IT_RX_FIFO0_MSG_PENDING;
        }
        else
        {
            hw_filter.FilterBank = 14;
            hw_filter.FilterFIFOAssignment = CAN_RX_FIFO1;
            rx_it = CAN_IT_RX_FIFO1_MSG_PENDING;
        }

        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_ConfigFilter(instance->map.handle, &hw_filter) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_ConfigFilter failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_Start(instance->map.handle) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_Start failed!"));
        BSP_RETURN_IF_TRUE_LOG(HAL_CAN_ActivateNotification(instance->map.handle, rx_it) != HAL_OK, -1, LOGERROR("[bsp_can] HAL_CAN_ActivateNotification failed!"));
    }

    return 0;
}

/**
 * @brief 发送一帧CAN数据
 * @param instance CAN实例
 * @param pack     数据包（id / frame_type / len / data）
 * @retval 50~53 发送成功，返回值 = CAN_TX_MAILBOX_FREE_BASE + 剩余空闲邮箱数（0/1/2/3）
 * @retval -1 参数非法 / 长度超限 / 帧类型非法 / 邮箱全满 / 加入邮箱失败
 */
int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack)
{
    uint8_t free_level;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, LOGERROR("[bsp_can] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(instance->map.handle == NULL, -1, LOGERROR("[bsp_can] CAN handle is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(pack == NULL, -1, LOGERROR("[bsp_can] Pack is NULL!"));

    // 长度校验：经典 CAN 单帧最大 8 字节
    BSP_RETURN_IF_TRUE_LOG(pack->len > 8, -1, LOGERROR("[bsp_can] Length %d exceeds classic CAN max (8)!", pack->len));

    // 错误帧为虚拟事件，不可发送
    BSP_RETURN_IF_TRUE_LOG(pack->frame_type == CAN_ERROR_FRAME, -1, LOGERROR("[bsp_can] Error frame is a virtual event, cannot transmit!"));

    // 由帧类型填充发送头（IDE/RTR/ID）
    switch (pack->frame_type)
    {
    case CAN_STANDARD_DATA_FRAME:
        instance->tx_header.IDE = CAN_ID_STD;
        instance->tx_header.RTR = CAN_RTR_DATA;
        instance->tx_header.StdId = pack->id;
        break;
    case CAN_EXTENDED_DATA_FRAME:
        instance->tx_header.IDE = CAN_ID_EXT;
        instance->tx_header.RTR = CAN_RTR_DATA;
        instance->tx_header.ExtId = pack->id;
        break;
    case CAN_STANDARD_REMOTE_FRAME:
        instance->tx_header.IDE = CAN_ID_STD;
        instance->tx_header.RTR = CAN_RTR_REMOTE;
        instance->tx_header.StdId = pack->id;
        break;
    case CAN_EXTENDED_REMOTE_FRAME:
        instance->tx_header.IDE = CAN_ID_EXT;
        instance->tx_header.RTR = CAN_RTR_REMOTE;
        instance->tx_header.ExtId = pack->id;
        break;
    default:
        LOGERROR("[bsp_can] Invalid frame_type=%d!", pack->frame_type);
        return -1;
    }
    instance->tx_header.DLC = pack->len;

    // 邮箱空闲检查：三个发送邮箱全满则拒绝
    free_level = HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle);
    BSP_RETURN_IF_TRUE_LOG(free_level == 0, -1, LOGERROR("[bsp_can] TX mailboxes full!"));

    // 加入发送邮箱
    if (HAL_CAN_AddTxMessage(instance->map.handle, &instance->tx_header, pack->data, &instance->tx_mailbox) != HAL_OK)
    {
        LOGERROR("[bsp_can] HAL_CAN_AddTxMessage failed!");
        return -1;
    }

    // 再次查询剩余邮箱数：返回值 = 基值 + 剩余数（0/1/2/3 → 50/51/52/53）
    free_level = HAL_CAN_GetTxMailboxesFreeLevel(instance->map.handle);
    return (int8_t)(CAN_TX_MAILBOX_FREE_BASE + free_level);
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_BXCAN */

#endif /* BSP_CAN_USED */

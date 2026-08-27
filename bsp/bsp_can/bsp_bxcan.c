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

    return 0;
}

int8_t CANTransmit(CANInstance *instance, const CAN_Pack_s *pack)
{
    return -1; /* TODO: 后续实现 */
}

#endif /* BSP_CAN_IP == BSP_CAN_IP_BXCAN */

#endif /* BSP_CAN_USED */

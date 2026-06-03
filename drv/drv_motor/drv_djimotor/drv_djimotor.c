#include "drv_djimotor.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_math.h"

#define CAN_TRANSMIT_TIMEOUT 1

#define DJI_MOTOR_GROUP_0 0   // 接收ID: 0x201-0x204
#define DJI_MOTOR_GROUP_1 1   // 接收ID: 0x205-0x208
#define DJI_MOTOR_GROUP_2 2   // 接收ID: 0x209-0x20b
#define DJI_MOTOR_GROUP_NUM 3 // 3组电机

typedef struct
{
    DJIMotorInstance *motors[4]; // 组内4个电机指针
    uint8_t motor_init_flag[4];  // 电机是否初始化标志
} DJIMotorSendGroup_t;

static DJIMotorSendGroup_t s_send_groups[CAN_NUM_MAX][DJI_MOTOR_GROUP_NUM] = {0};

/**
 * @brief DJI电机接收回调函数
 * @param can CAN实例指针
 * @note 数据格式：
 *       DATA[0-1]: 转子机械角度 (0~8191 对应 0~360°)
 *       DATA[2-3]: 转子转速
 *       DATA[4-5]: 实际转矩电流 (int16)
 *       DATA[6]:    电机温度 (°C)
 *       DATA[7]:    错误码
 */
static void DJIMotorRxCallback(CANInstance *can)
{
    if (!can || !can->parent)
        return;

    DJIMotorInstance *motor = (DJIMotorInstance *)can->parent;
    uint8_t *data = can->rx_buff;

    // 解析原始数据
    motor->raw_encoder = ((uint16_t)data[0] << 8) | data[1];
    motor->raw_velocity = ((int16_t)data[2] << 8) | data[3];
    motor->raw_current = ((int16_t)data[4] << 8) | data[5];
    motor->raw_temperature_motor = (int8_t)data[6];
    motor->error_code = data[7];

    // 获取电机参数（查表）
    uint8_t model = motor->model;
    if (model >= DJI_MODEL_NUM)
        return;

    float reduction_ratio = dji_motor_params[model].reduction_ratio;
    float torque_coef = dji_motor_params[model].torque_coef;
    uint16_t current_max = dji_motor_params[model].current_max;

    // 计算单圈位置 (rad) [0, 2π)
    motor->position_single = (float)motor->raw_encoder * M_2PI / DJI_ENCODER_RESOLUTION;

    // 计算扭矩 (N·m)
    motor->torque = (float)motor->raw_current * torque_coef;

    // 速度: rpm -> rad/s (考虑减速比)
    motor->speed = (float)motor->raw_velocity * M_2PI / 60.0f / reduction_ratio;

    // 电流/电压值归一化
    motor->current = (float)motor->raw_current / current_max;

    // 温度
    motor->temperature = (float)motor->raw_temperature_motor;

    // 错误标志
    motor->error_flags = motor->error_code;

    // 喂狗（如果使用了守护进程）
    if (motor->daemon)
    {
        DaemonReload(motor->daemon);
    }
}

int8_t DJIMotorRegister(DJIMotorInstance *inst, DJIMotor_Init_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    // 计算发送ID和接收ID
    uint16_t tx_id = can_tx_id[cfg->model][(cfg->motor_id - 1) / 4];
    uint16_t rx_id = dji_motor_params[cfg->model].can_rx_id_base + cfg->motor_id;
    uint8_t group_idx = (rx_id - 0x201) / 4;          // 计算结果0-2。0x201-0x204/0x205-0x208/0x209-0x20b
    uint8_t motor_idx_in_group = (rx_id - 0x201) % 4; // 结果0-3
    BoardCAN_e can_e = cfg->can_e;

    // 检查是否已经初始化
    if (1 == s_send_groups[can_e][group_idx].motor_init_flag[motor_idx_in_group])
        return -1;

    // 注册到发送分组
    s_send_groups[can_e][group_idx].motors[motor_idx_in_group] = inst;
    s_send_groups[can_e][group_idx].motor_init_flag[motor_idx_in_group] = 1;

    // 初始化DJIMotorInstance实例
    inst->model = cfg->model;
    inst->set_ref = 0.0f;
    inst->enable = 0;
    inst->motor_id = cfg->motor_id;

    if (inst->can)
    {
        CAN_Init_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .tx_id = tx_id,
            .filter_mode = CAN_FILTER_MODE_LIST,
            .rx_id_list = {rx_id, CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED},
            .rx_mask = 0,
            .rx_callback = DJIMotorRxCallback,
        };
        CANRegister(inst->can, &can_cfg);
        inst->can->parent = inst; // 设置父实例指针
    }

    if (inst->daemon)
    {
        Daemon_Init_Config_s config = {
            .callback = cfg->lost_callback,
            .fault_action = cfg->fault_action,
            .owner_id = inst,
            .reload_count = cfg->reload_count};
        DaemonRegister(inst->daemon, &config);
    }

    return 0;
}

void DJIMotorEnable(DJIMotorInstance *inst)
{
    if (!inst)
        return;
    inst->enable = 1;
}

void DJIMotorDisable(DJIMotorInstance *inst)
{
    if (!inst)
        return;
    inst->enable = 0;
}

void DJIMotorSetRef(DJIMotorInstance *inst, float ref)
{
    if (!inst)
        return;
    inst->set_ref = ref;
}

void DJIMotorSend(DJIMotorInstance *inst)
{
    if (!inst)
        return;

    uint8_t motor_id = inst->motor_id;
    uint8_t group_idx = (motor_id - 1) / 4;
    BoardCAN_e can_e = inst->can->can_e;

    DJIMotorSendGroup_t *group = &s_send_groups[can_e][group_idx];

    // 打包整组数据
    CANInstance *can = inst->can;
    for (int i = 0; i < 4; i++)
    {
        int16_t cur = 0;
        DJIMotorInstance *m = group->motors[i];
        if (m && m->enable)
        {
            cur = (int16_t)m->set_ref;
        }
        can->tx_buff[i * 2] = (uint8_t)(cur >> 8);       // 高字节
        can->tx_buff[i * 2 + 1] = (uint8_t)(cur & 0xFF); // 低字节
    }

    CANTransmit(can, CAN_TRANSMIT_TIMEOUT);
}

#endif // HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED
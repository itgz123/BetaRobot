#include "drv_djimotor.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"

#define CAN_TRANSMIT_TIMEOUT 1

typedef enum
{
    DJI_MOTOR_GROUP_0 = 0, // 接收ID: 0x201-0x204
    DJI_MOTOR_GROUP_1 = 1, // 接收ID: 0x205-0x208
    DJI_MOTOR_GROUP_2 = 2, // 接收ID: 0x209-0x20b
    DJI_MOTOR_GROUP_NUM,   // 3组电机
} DJIMotorGroup_e;         // 一个CAN上最多3组DJI电机

typedef struct
{
    DJIMotorInstance *motors[4]; // 组内4个电机指针
    uint8_t motor_init_flag[4];  // 电机是否初始化标志
} DJIMotorSendGroup_t;

static DJIMotorSendGroup_t s_send_groups[CAN_NUM_MAX][DJI_MOTOR_GROUP_NUM] = {0};
const uint16_t can_tx_id[DJI_MODEL_NUM][2] = {
    [DJI_MODEL_M2006][0] = 0x200,  // 1-4号
    [DJI_MODEL_M2006][1] = 0x1ff,  // 5-8号
    [DJI_MODEL_M3508][0] = 0x200,  // 1-4号
    [DJI_MODEL_M3508][1] = 0x1ff,  // 5-8号
    [DJI_MODEL_GM6020][0] = 0x1fe, // 1-4号
    [DJI_MODEL_GM6020][1] = 0x2fe, // 5-7号
};
const uint16_t can_rx_id_base[DJI_MODEL_NUM] = {
    [DJI_MODEL_M2006] = 0x200,
    [DJI_MODEL_M3508] = 0x200,
    [DJI_MODEL_GM6020] = 0x204,
};

int8_t DJIMotorRegister(DJIMotorInstance *inst, DJIMotor_Init_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    // 计算发送ID和接收ID
    uint16_t tx_id = can_tx_id[cfg->model][(cfg->motor_id - 1) / 4];
    uint16_t rx_id = can_rx_id_base[cfg->model] + cfg->motor_id;
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
            .rx_callback = NULL, // todo:之后要添加回调函数
        };
        CANRegister(inst->can, &can_cfg);
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
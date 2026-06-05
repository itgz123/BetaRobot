#include "drv_djimotor.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_math.h"
#include <string.h>

#define CAN_TRANSMIT_TIMEOUT 1

#define DJI_MOTOR_GROUP_0 0   // 接收ID: 0x201-0x204
#define DJI_MOTOR_GROUP_1 1   // 接收ID: 0x205-0x208
#define DJI_MOTOR_GROUP_2 2   // 接收ID: 0x209-0x20b
#define DJI_MOTOR_GROUP_NUM 3 // 3组电机

// 编码器分辨率 (14位)
#define DJI_ENCODER_RESOLUTION 8192

// 电调电流范围 (原始值)
#define C610_CURRENT_MAX 10000   // C610 (M2006) 电流范围 ±10000 → ±10A
#define C620_CURRENT_MAX 16384   // C620 (M3508) 电流范围 ±16384 → ±20A
#define GM6020_CURRENT_MAX 16384 // GM6020 电流范围 ±16384 → ±3A

// 电流转换系数 (实际电流A / 原始值)
#define C610_CURRENT_SCALE (10.0f / 10000.0f)  // 0.001 A/LSB
#define C620_CURRENT_SCALE (20.0f / 16384.0f)  // ≈0.00122 A/LSB
#define GM6020_CURRENT_SCALE (3.0f / 16384.0f) // ≈0.000183 A/LSB

/*============================================
 *              电机参数表定义
 *============================================*/
const MotorParams_s dji_motor_params[DJI_MODEL_NUM] = {
    [DJI_MODEL_M3508] = {C620_CURRENT_MAX, DJI_ENCODER_RESOLUTION, 0.3f, 3591.0f / 187.0f, 482.0f, 469.0f, 3.0f, 10.0f, C620_CURRENT_SCALE},
    [DJI_MODEL_M3508_DIRECT] = {C620_CURRENT_MAX, DJI_ENCODER_RESOLUTION, 0.02f, 1.0f, 9400.0f, 9085.0f, 0.16f, 10.0f, C620_CURRENT_SCALE},
    [DJI_MODEL_M2006] = {C610_CURRENT_MAX, DJI_ENCODER_RESOLUTION, 0.18f, 36.0f, 500.0f, 416.0f, 1.0f, 3.0f, C610_CURRENT_SCALE},
    [DJI_MODEL_GM6020] = {GM6020_CURRENT_MAX, DJI_ENCODER_RESOLUTION, 0.741f, 1.0f, 320.0f, 132.0f, 1.2f, 1.62f, GM6020_CURRENT_SCALE},
};

const uint16_t can_tx_id[DJI_MODEL_NUM][2] = {
    [DJI_MODEL_M3508][0] = 0x200,        // 1-4号
    [DJI_MODEL_M3508][1] = 0x1ff,        // 5-8号
    [DJI_MODEL_M3508_DIRECT][0] = 0x200, // 1-4号
    [DJI_MODEL_M3508_DIRECT][1] = 0x1ff, // 5-8号
    [DJI_MODEL_M2006][0] = 0x200,        // 1-4号
    [DJI_MODEL_M2006][1] = 0x1ff,        // 5-8号
    [DJI_MODEL_GM6020][0] = 0x1fe,       // 1-4号
    [DJI_MODEL_GM6020][1] = 0x2fe,       // 5-7号
};

const uint16_t can_rx_id_base[DJI_MODEL_NUM] = {
    [DJI_MODEL_M3508] = 0x200,
    [DJI_MODEL_M3508_DIRECT] = 0x200,
    [DJI_MODEL_M2006] = 0x200,
    [DJI_MODEL_GM6020] = 0x204,
};

static DJIMotorSendGroup_s s_send_groups[CAN_NUM_MAX][DJI_MOTOR_GROUP_NUM] = {0};

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

    // 双缓冲索引切换
    uint8_t now_idx = motor->data_now_idx;
    uint8_t last_idx = 1 - now_idx;

    // 解析原始数据
    motor->data_raw.raw_encoder = ((uint16_t)data[0] << 8) | data[1];
    motor->data_raw.raw_velocity = ((int16_t)data[2] << 8) | data[3];
    motor->data_raw.raw_current = ((int16_t)data[4] << 8) | data[5];
    motor->data_raw.raw_temperature_motor = (int8_t)data[6];
    motor->data_raw.error_code = data[7];

    // 获取电机参数
    uint8_t model = motor->base.model;
    if (model >= DJI_MODEL_NUM)
        return;

    uint16_t current_max = dji_motor_params[model].current_max;
    uint16_t encoder_resolution = dji_motor_params[model].encoder_resolution;

    // 计算单圈位置 (rad) [0, 2π)
    motor->data[now_idx].position_single = (float)motor->data_raw.raw_encoder * M_2PI / encoder_resolution;

    // 计算多圈位置 (rad)
    motor->data[now_idx].position_multi = motor->data[last_idx].position_multi + BSP_Math_AngleDiff(motor->data[last_idx].position_single, motor->data[now_idx].position_single);

    // 计算速度 (rad/s) - 转子速度
    if (motor->speed_src == MOTOR_SPEED_SRC_FEEDBACK)
    {
        // 使用反馈速度
        motor->data[now_idx].speed = (float)motor->data_raw.raw_velocity * M_2PI / 60.0f;
    }
    else
    {
        // 使用位置差分计算速度
        uint64_t now_time_us = DWT_GetTimeUs();
        float dt = (now_time_us - motor->data[last_idx].time_stamp) * 1e-6f; // us -> s
        if (dt > 0.0f)
        {
            motor->data[now_idx].speed = (motor->data[now_idx].position_multi - motor->data[last_idx].position_multi) / dt;
        }
    }

    // 电流 (A)
    motor->data[now_idx].current = (float)motor->data_raw.raw_current / current_max;

    // 温度
    motor->data[now_idx].temperature = (float)motor->data_raw.raw_temperature_motor;

    // 记录时间戳
    motor->data[now_idx].time_stamp = DWT_GetTimeUs();

    // 切换索引，更新当前数据
    motor->data_now_idx = last_idx;

    // 喂狗
    if (motor->base.daemon)
    {
        DaemonReload(motor->base.daemon);
    }
}

/*============================================
 *              反馈获取函数
 *============================================*/
static float DJIMotor_GetAngle(DJIMotorInstance *inst)
{
    MotorController_s *ctrl = &inst->base.controller;
    if (ctrl->angle_src == MOTOR_FEEDBACK_EXTERNAL && ctrl->angle_external_ptr)
    {
        return *ctrl->angle_external_ptr;
    }
    return inst->data[inst->data_now_idx].position_multi;
}

static float DJIMotor_GetSpeed(DJIMotorInstance *inst)
{
    MotorController_s *ctrl = &inst->base.controller;
    if (ctrl->speed_src == MOTOR_FEEDBACK_EXTERNAL && ctrl->speed_external_ptr)
    {
        return *ctrl->speed_external_ptr;
    }
    return inst->data[inst->data_now_idx].speed;
}

int8_t DJIMotorRegister(DJIMotorInstance *inst, DJIMotor_Init_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    // 计算发送ID和接收ID
    uint16_t tx_id = can_tx_id[cfg->model][(cfg->motor_id - 1) / 4];
    uint16_t rx_id = can_rx_id_base[cfg->model] + cfg->motor_id;
    uint8_t group_idx = (rx_id - 0x201) / 4;
    uint8_t motor_idx_in_group = (rx_id - 0x201) % 4;
    BoardCAN_e can_e = cfg->can_e;

    // 检查是否已经初始化
    if (1 == s_send_groups[can_e][group_idx].motor_init_flag[motor_idx_in_group])
        return -1;

    // 注册到发送分组
    s_send_groups[can_e][group_idx].motors[motor_idx_in_group] = inst;
    s_send_groups[can_e][group_idx].motor_init_flag[motor_idx_in_group] = 1;

    // 初始化基本属性
    inst->base.brand = MOTOR_BRAND_DJI;
    inst->base.model = cfg->model;
    inst->motor_id = cfg->motor_id;
    inst->base.enable = 0;

    // 初始化控制器设置
    inst->base.controller.loop_type = cfg->controller_setting.loop_type;
    inst->base.controller.motor_direction = cfg->controller_setting.motor_direction;
    inst->base.controller.feedback_direction = cfg->controller_setting.feedback_direction;
    inst->base.controller.angle_src = cfg->controller_setting.angle_src;
    inst->base.controller.speed_src = cfg->controller_setting.speed_src;
    inst->base.controller.max_angle = cfg->controller_setting.max_angle;
    inst->base.controller.speed_feedforward_ptr = cfg->controller_setting.speed_feedforward_ptr;
    inst->base.controller.position_feedforward_ptr = cfg->controller_setting.position_feedforward_ptr;
    inst->base.controller.angle_external_ptr = cfg->controller_setting.angle_external_ptr;
    inst->base.controller.speed_external_ptr = cfg->controller_setting.speed_external_ptr;
    inst->base.controller.ref = 0.0f;
    inst->base.controller.output = 0.0f;

    // 初始化 PID 控制器
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_SPEED)
    {
        PIDInit(&inst->base.controller.pid_speed, &cfg->controller_setting.pid_speed_config);
    }
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_ANGLE)
    {
        PIDInit(&inst->base.controller.pid_angle, &cfg->controller_setting.pid_angle_config);
    }

    // 初始化速度来源
    inst->speed_src = cfg->speed_src;

    // 初始化数据缓冲
    inst->data_now_idx = 0;
    memset(&inst->data_raw, 0, sizeof(DJIMotorRawData_s));
    memset(inst->data, 0, sizeof(inst->data));

    // 保存分组信息
    inst->sender_group = &s_send_groups[can_e][group_idx];
    inst->motor_idx_in_group = motor_idx_in_group;

    // 注册CAN实例
    if (inst->base.can)
    {
        CAN_Init_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .tx_id = tx_id,
            .filter_mode = CAN_FILTER_MODE_LIST,
            .rx_id_list = {rx_id, CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED},
            .rx_mask = 0,
            .rx_callback = DJIMotorRxCallback,
        };
        CANRegister(inst->base.can, &can_cfg);
        inst->base.can->parent = inst;
    }

    // 注册守护进程
    if (inst->base.daemon)
    {
        Daemon_Init_Config_s config = {
            .callback = cfg->lost_callback,
            .fault_action = cfg->fault_action,
            .owner_id = inst,
            .reload_count = cfg->reload_count};
        DaemonRegister(inst->base.daemon, &config);
    }

    return 0;
}

void DJIMotorEnable(DJIMotorInstance *inst)
{
    if (!inst)
        return;
    inst->base.enable = 1;
}

void DJIMotorDisable(DJIMotorInstance *inst)
{
    if (!inst)
        return;
    inst->base.enable = 0;
}

void DJIMotorSetRef(DJIMotorInstance *inst, float ref)
{
    if (!inst)
        return;
    inst->base.controller.ref = ref;
}

void DJIMotorSend(DJIMotorInstance *inst)
{
    if (!inst || !inst->sender_group || !inst->base.can)
        return;

    DJIMotorSendGroup_s *group = inst->sender_group;
    CANInstance *can = inst->base.can;

    // ===== 控制计算 =====
    if (inst->base.enable)
    {
        MotorController_s *ctrl = &inst->base.controller;

        float ref = ctrl->ref;
        float setpoint = ref;
        float measure;
        float output = 0.0f;

        // 获取电流限幅
        uint8_t model = inst->base.model;
        if (model < DJI_MODEL_NUM)
        {
            uint16_t current_max = dji_motor_params[model].current_max;

            // 电机方向处理 (直接乘)
            setpoint *= ctrl->motor_direction;

            // 位置环 (最外环)
            if (ctrl->loop_type & MOTOR_LOOP_ANGLE)
            {
                if (ctrl->position_feedforward_ptr)
                {
                    setpoint += *ctrl->position_feedforward_ptr;
                }
                measure = DJIMotor_GetAngle(inst);
                setpoint = PIDCalculate(&ctrl->pid_angle, setpoint, measure, 0.0f);
            }

            // 速度环
            if (ctrl->loop_type & MOTOR_LOOP_SPEED)
            {
                if (ctrl->speed_feedforward_ptr)
                {
                    setpoint += *ctrl->speed_feedforward_ptr;
                }
                measure = DJIMotor_GetSpeed(inst);
                output = PIDCalculate(&ctrl->pid_speed, setpoint, measure, 0.0f);
            }
            else
            {
                // 无速度环时，直接输出
                output = setpoint;
            }

            // 反馈方向处理 (直接乘)
            output *= ctrl->feedback_direction;

            // 输出限幅
            ctrl->output = BSP_Math_Clamp(output, -(float)current_max, (float)current_max);
        }
    }

    // ===== 打包发送 =====
    for (int i = 0; i < 4; i++)
    {
        int16_t cur = 0;
        DJIMotorInstance *m = group->motors[i];
        if (m && m->base.enable)
        {
            cur = (int16_t)m->base.controller.output;
        }
        can->tx_buff[i * 2] = (uint8_t)(cur >> 8);       // 高字节
        can->tx_buff[i * 2 + 1] = (uint8_t)(cur & 0xFF); // 低字节
    }

    CANTransmit(can, CAN_TRANSMIT_TIMEOUT);
}

#endif // HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED

#include "drv_djimotor.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_math.h"
#include <string.h>

#define CAN_TRANSMIT_TIMEOUT 1

/*
 * ============================================
 *              DJI 电机分组策略说明
 * ============================================
 *
 * 【问题背景】
 * DJI 电机通过 CAN 总线通信，一帧 CAN 数据最多携带 4 个电机的控制值。
 * 因此需要将电机分组，每组共用一帧发送报文。
 *
 * 【两种分组策略】
 *
 * 策略 A：按发送 ID 分组（参考代码采用）
 *   - 优点：发送简单，每组固定使用一个 tx_id
 *   - 缺点：M3508/M2006 ID 5-8 和 GM6020 ID 1-4 的 rx_id 重叠（都是 0x205-0x208）
 *           如果挂载在同一 CAN 总线上，会导致接收 ID 冲突
 *
 * 策略 B：按接收 ID 分组（本代码采用）✓
 *   - 优点：天然避免接收 ID 冲突，同一 CAN 总线上不会有两个电机响应同一个 rx_id
 *   - 缺点：同一组内可能有不同的 tx_id，需要在发送时额外处理
 *
 * 【本代码实现】
 * 采用策略 B，按接收 ID 分组：
 *   - Group 0: rx_id 0x201-0x204（M3508/M2006 ID 1-4，tx_id=0x200）
 *   - Group 1: rx_id 0x205-0x208（M3508/M2006 ID 5-8 tx_id=0x1FF，或 GM6020 ID 1-4 tx_id=0x1FE）
 *   - Group 2: rx_id 0x209-0x20B（GM6020 ID 5-7，tx_id=0x2FE）
 *
 * 发送处理：
 *   由于 Group 1 可能包含不同 tx_id 的电机，DJIMotorSend() 会检查组内电机的 tx_id，
 *   将相同 tx_id 的电机数据打包到同一帧发送，不同 tx_id 则分别发送多帧。
 *
 * 【关键约束】
 *   如果两个电机的 rx_id 不同，它们的 tx_id 必然不同，不会共用同一发送帧。
 *   因此按 rx_id 分组是安全的，只需在发送时处理 tx_id 差异。
 */

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

// 电调电流范围 (安培)
#define C610_CURRENT_MAX_A 10.0f  // C610 (M2006) 最大电流 10A
#define C620_CURRENT_MAX_A 20.0f  // C620 (M3508) 最大电流 20A
#define GM6020_CURRENT_MAX_A 3.0f // GM6020 最大电流 3A

// 扭矩系数 (Nm/A)
#define M3508_TORQUE_COEFF 0.02f
#define M2006_TORQUE_COEFF 0.005f
#define GM6020_TORQUE_COEFF 0.741f

// 扭矩范围
#define M3508_TORQUE_MAX_NM 0.4f
#define M2006_TORQUE_MAX_NM 0.05f
#define GM6020_TORQUE_MAX_NM 2.223f

/*============================================
 *              电机参数表定义
 *============================================*/
const DJIMotorParams_s dji_motor_params[DJI_MODEL_NUM] = {
    [DJI_MODEL_M3508] = {C620_CURRENT_MAX, C620_CURRENT_MAX_A, DJI_ENCODER_RESOLUTION, M3508_TORQUE_COEFF, M3508_TORQUE_MAX_NM},
    [DJI_MODEL_M2006] = {C610_CURRENT_MAX, C610_CURRENT_MAX_A, DJI_ENCODER_RESOLUTION, M2006_TORQUE_COEFF, M2006_TORQUE_MAX_NM},
    [DJI_MODEL_GM6020] = {GM6020_CURRENT_MAX, GM6020_CURRENT_MAX_A, DJI_ENCODER_RESOLUTION, GM6020_TORQUE_COEFF, GM6020_TORQUE_MAX_NM},
};

const uint16_t can_tx_id[DJI_MODEL_NUM][2] = {
    [DJI_MODEL_M3508][0] = 0x200,  // 1-4号
    [DJI_MODEL_M3508][1] = 0x1ff,  // 5-8号
    [DJI_MODEL_M2006][0] = 0x200,  // 1-4号
    [DJI_MODEL_M2006][1] = 0x1ff,  // 5-8号
    [DJI_MODEL_GM6020][0] = 0x1fe, // 1-4号
    [DJI_MODEL_GM6020][1] = 0x2fe, // 5-7号
};

const uint16_t can_rx_id_base[DJI_MODEL_NUM] = {
    [DJI_MODEL_M3508] = 0x200,
    [DJI_MODEL_M2006] = 0x200,
    [DJI_MODEL_GM6020] = 0x204,
};

static DJIMotorSendGroup_s s_send_groups[CAN_NUM_MAX][DJI_MOTOR_GROUP_NUM] = {0};

/*============================================
 *              虚函数表实例
 *============================================*/
void DJIMotor_Enable(void *inst);
void DJIMotor_Disable(void *inst);
void DJIMotor_SetRef(void *inst, float ref);
void DJIMotor_Send(void *inst);
float DJIMotor_GetAngle(void *inst);
float DJIMotor_GetSpeed(void *inst);

const static MotorVTable_s s_dji_motor_vtable = {
    .enable = DJIMotor_Enable,
    .disable = DJIMotor_Disable,
    .set_ref = DJIMotor_SetRef,
    .send = DJIMotor_Send,
    .get_angle = DJIMotor_GetAngle,
    .get_speed = DJIMotor_GetSpeed,
};

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
    motor->data_raw.raw_encoder = ((uint16_t)data[0] << 8) | data[1];
    motor->data_raw.raw_velocity = ((int16_t)data[2] << 8) | data[3];
    motor->data_raw.raw_current = ((int16_t)data[4] << 8) | data[5];
    motor->data_raw.raw_temperature_motor = (int8_t)data[6];
    motor->data_raw.error_code = data[7];

    // 获取电机参数
    uint8_t model = motor->base.model;
    if (model >= DJI_MODEL_NUM)
        return;

    float current_max_a = dji_motor_params[model].current_max_a;
    uint16_t current_max = dji_motor_params[model].current_max;
    uint16_t encoder_resolution = dji_motor_params[model].encoder_resolution;

    // 保存上次数据
    float last_position_single = motor->data.position_single;
    float last_position_multi = motor->data.position_multi;
    motor->data.last_speed = motor->data.speed;
    uint64_t last_time_stamp = motor->data.last_time_stamp;

    // 计算单圈位置 (rad) [0, 2π)
    motor->data.position_single = (float)motor->data_raw.raw_encoder * M_2PI / encoder_resolution;

    // 计算多圈位置 (rad)
    // BSP_Math_AngleDiff 返回最短角度差，正确处理边界穿越
    motor->data.position_multi = last_position_multi + BSP_Math_AngleDiff(last_position_single, motor->data.position_single);

    // 记录时间戳
    uint64_t now_time_us = DWT_GetTimeUs();
    motor->data.last_time_stamp = now_time_us;
    float dt = (now_time_us - last_time_stamp) * 1e-6f; // us -> s

    // 计算速度 (rad/s) - 转子速度
    float raw_speed;
    if (motor->base.speed_src == MOTOR_SPEED_SRC_FEEDBACK)
    {
        // 使用反馈速度
        raw_speed = (float)motor->data_raw.raw_velocity * M_2PI / 60.0f;
    }
    else
    {
        // 使用位置差分计算速度
        if (dt > 0.0f)
        {
            raw_speed = (motor->data.position_multi - last_position_multi) / dt;
        }
        else
        {
            raw_speed = 0.0f;
        }
    }

    // 速度低通滤波: speed = (1-alpha) * raw + alpha * last, alpha = RC / (RC + dt)
    // alpha 越大，滤波越强（响应越慢）
    if (motor->base.speed_lpf_enable == MOTOR_SPEED_LPF_ENABLE && dt > 0.0f)
    {
        float alpha = motor->base.speed_lpf_rc / (motor->base.speed_lpf_rc + dt);
        motor->data.speed = (1.0f - alpha) * raw_speed + alpha * motor->data.last_speed;
    }
    else
    {
        motor->data.speed = raw_speed;
    }

    // 电流 (A) - 使用安培单位
    motor->data.current = (float)motor->data_raw.raw_current / (float)current_max * current_max_a;

    // 温度
    motor->data.temperature = (float)motor->data_raw.raw_temperature_motor;

    // 喂狗
    if (motor->base.daemon)
    {
        DaemonReload(motor->base.daemon);
    }
}

/*============================================
 *              反馈获取函数 (虚函数实现)
 *============================================*/
float DJIMotor_GetAngle(void *inst)
{
    if (!inst)
        return 0.0f;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    MotorControllerSetting_s *setting = &motor->base.setting;
    float angle;
    if (setting->angle_src == MOTOR_FEEDBACK_EXTERNAL && setting->angle_external_ptr)
    {
        angle = *setting->angle_external_ptr;
    }
    else
    {
        angle = motor->data.position_multi;
    }
    // 反馈方向修正
    return angle * setting->feedback_direction;
}

float DJIMotor_GetSpeed(void *inst)
{
    if (!inst)
        return 0.0f;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    MotorControllerSetting_s *setting = &motor->base.setting;
    float speed;
    if (setting->speed_src == MOTOR_FEEDBACK_EXTERNAL && setting->speed_external_ptr)
    {
        speed = *setting->speed_external_ptr;
    }
    else
    {
        speed = motor->data.speed;
    }
    // 反馈方向修正
    return speed * setting->feedback_direction;
}

/**
 * @brief DJI电机守护进程回调函数
 * @param owner 守护进程所有者指针 (DJIMotorInstance*)
 * @note 电机离线时调用，清空 PID 状态避免积分累积
 */
static void DJIMotorDaemonCallback(void *owner)
{
    if (!owner)
        return;

    DJIMotorInstance *motor = (DJIMotorInstance *)owner;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);
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
    inst->base.enable = MOTOR_DISABLE;
    inst->base.vtable = &s_dji_motor_vtable;

    // 初始化控制器设置
    inst->base.setting = cfg->controller_setting;

    // 初始化控制器状态
    inst->base.controller.ref = 0.0f;
    inst->base.controller.output = 0.0f;

    // 初始化速度环 PID
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_SPEED)
    {
        cfg->pid_speed_setting.config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL;
        PIDInit(&inst->base.controller.pid_speed, &cfg->pid_speed_setting);
    }

    // 初始化位置环 PID
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_ANGLE)
    {
        cfg->pid_angle_setting.config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL;
        PIDInit(&inst->base.controller.pid_angle, &cfg->pid_angle_setting);
    }

    // 初始化速度来源
    inst->base.speed_src = cfg->speed_src;
    inst->base.speed_lpf_enable = cfg->speed_lpf_enable;
    inst->base.speed_lpf_rc = cfg->speed_lpf_rc;

    // 初始化数据缓冲
    memset(&inst->data_raw, 0, sizeof(DJIMotorRawData_s));
    memset(&inst->data, 0, sizeof(DJIMotorData_s));

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
            .callback = DJIMotorDaemonCallback,
            .fault_action = cfg->fault_action,
            .owner_id = inst,
            .reload_count = cfg->reload_count};
        DaemonRegister(inst->base.daemon, &config);
    }

    return 0;
}

/**
 * @brief 单个电机控制计算
 */
static void DJIMotor_Calculate(DJIMotorInstance *inst)
{
    if (!inst || !inst->base.enable)
        return;

    MotorControllerSetting_s *setting = &inst->base.setting;
    MotorController_s *ctrl = &inst->base.controller;

    float setpoint = ctrl->ref;
    float measure;
    float output = 0.0f;

    uint8_t model = inst->base.model;
    if (model >= DJI_MODEL_NUM)
        return;

    // 位置环 (最外环)
    if (setting->loop_type & MOTOR_LOOP_ANGLE)
    {
        // 位置输入限位
        if ((setting->angle_limit_enable == MOTOR_ANGLE_LIMIT_ENABLE) && (setting->angle_limit_min < setting->angle_limit_max))
        {
            setpoint = BSP_Math_Clamp(setpoint, setting->angle_limit_min, setting->angle_limit_max);
        }

        float position_feedforward = 0.0f;
        if (setting->position_feedforward_src == MOTOR_FEEDFORWARD_EXTERNAL &&
            setting->position_feedforward_ptr)
        {
            position_feedforward = *setting->position_feedforward_ptr;
        }
        measure = DJIMotor_GetAngle(inst);
        setpoint = PIDCalculate(&ctrl->pid_angle, setpoint, measure, position_feedforward);
    }

    // 速度环
    if (setting->loop_type & MOTOR_LOOP_SPEED)
    {
        float speed_feedforward = 0.0f;
        if (setting->speed_feedforward_src == MOTOR_FEEDFORWARD_EXTERNAL &&
            setting->speed_feedforward_ptr)
        {
            speed_feedforward = *setting->speed_feedforward_ptr;
        }
        measure = DJIMotor_GetSpeed(inst);
        output = PIDCalculate(&ctrl->pid_speed, setpoint, measure, speed_feedforward);
    }
    else
    {
        // 无速度环时，直接输出
        output = setpoint;
    }

    // 电机方向修正: motor_direction修正电机安装方向, feedback_direction已在反馈端修正
    output *= setting->motor_direction;

    // 扭矩 -> CAN 原始值: I_raw = T / K_t * current_max / current_max_a
    // 简化: I_raw = T / (K_t * current_max_a / current_max) = T / torque_to_current_ratio
    float torque_coeff = dji_motor_params[model].torque_coeff;
    float current_max = (float)dji_motor_params[model].current_max;
    float current_max_a = dji_motor_params[model].current_max_a;

    // 扭矩转电流原始值, 并限幅到 ±current_max
    float current_raw = (output / torque_coeff) * (current_max / current_max_a);
    ctrl->output = BSP_Math_Clamp(current_raw, -current_max, current_max);
}

/*============================================
 *              虚函数实现
 *============================================*/
void DJIMotor_Enable(void *inst)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    motor->base.enable = MOTOR_ENABLE;
}

void DJIMotor_Disable(void *inst)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    motor->base.enable = MOTOR_DISABLE;
}

/**
 * @brief 设置电机控制参考值
 * @param inst DJIMotorInstance 指针
 * @param ref 参考值
 *
 * 方向标定流程:
 *   1. 人为设定正方向（顺时针或逆时针）
 *   2. 开环控制，发送正的较小力矩值，观察实际旋转方向和反馈方向
 *   3. 如果实际旋转方向与正方向相反，设置 motor_direction = MOTOR_DIRECTION_REVERSE
 *   4. 如果反馈正负与实际旋转方向相反，设置 feedback_direction = MOTOR_DIRECTION_REVERSE
 *
 * 方向处理逻辑:
 *   - motor_direction: 修正电机安装方向，在输出端翻转扭矩方向
 *   - feedback_direction: 修正反馈极性，在反馈获取函数中翻转反馈值符号
 *   - PID 计算在统一的坐标系下进行，setpoint 和 measure 都已正确处理方向
 *
 * 控制模式说明:
 *   - MOTOR_LOOP_OPEN (力矩开环): ref = 扭矩(Nm) → 扭矩/系数 → 电流 → CAN发送
 *   - MOTOR_LOOP_SPEED (速度环): ref = 速度(rad/s) → PID(扭矩) → 扭矩/系数 → 电流 → CAN发送
 *   - MOTOR_LOOP_ANGLE (位置环): ref = 位置(rad) → PID(扭矩) → 扭矩/系数 → 电流 → CAN发送
 *   - MOTOR_LOOP_ANGLE | MOTOR_LOOP_SPEED (位置-速度双环):
 *       ref = 位置(rad) → PID(速度) → PID(扭矩) → 扭矩/系数 → 电流 → CAN发送
 *
 * 最终电流统一限幅到 ±current_max (CAN原始值范围)
 */
void DJIMotor_SetRef(void *inst, float ref)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    motor->base.controller.ref = ref;
}

void DJIMotor_Send(void *inst)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;

    if (!motor->sender_group || !motor->base.can)
        return;

    DJIMotorSendGroup_s *group = motor->sender_group;
    CANInstance *can = motor->base.can;

    // ===== 控制计算：组内所有已初始化电机 =====
    for (int i = 0; i < 4; i++)
    {
        if (group->motor_init_flag[i] && group->motors[i])
        {
            DJIMotor_Calculate(group->motors[i]);
        }
    }

    // ===== 按 tx_id 分帧发送 =====
    // 由于按 rx_id 分组，同一组内可能存在不同 tx_id 的电机
    // 例如 Group 1 (rx_id 0x205-0x208) 可能包含：
    //   - M3508/M2006 ID 5-8 (tx_id=0x1FF)
    //   - GM6020 ID 1-4 (tx_id=0x1FE)
    // 因此需要收集组内所有不同的 tx_id，分别打包发送

    // Step 1: 收集组内不同的 tx_id
    uint16_t tx_ids[4] = {0};
    uint8_t tx_id_count = 0;

    for (int i = 0; i < 4; i++)
    {
        if (group->motor_init_flag[i] && group->motors[i] && group->motors[i]->base.can)
        {
            uint16_t id = group->motors[i]->base.can->tx_id;
            uint8_t found = 0;
            for (int j = 0; j < tx_id_count; j++)
            {
                if (tx_ids[j] == id)
                {
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                tx_ids[tx_id_count++] = id;
            }
        }
    }

    // Step 2: 按 tx_id 分别打包发送
    // 每个 tx_id 发送一帧 CAN 报文，包含对应的 4 个电机位置（部分可能为空）
    for (int t = 0; t < tx_id_count; t++)
    {
        uint16_t current_tx_id = tx_ids[t];
        can->tx_id = current_tx_id;

        for (int i = 0; i < 4; i++)
        {
            int16_t cur = 0;
            DJIMotorInstance *m = group->motors[i];
            // 只有匹配当前 tx_id 的电机才填充数据
            if (group->motor_init_flag[i] && m && m->base.enable &&
                m->base.can && m->base.can->tx_id == current_tx_id)
            {
                cur = (int16_t)m->base.controller.output;
            }
            can->tx_buff[i * 2] = (uint8_t)(cur >> 8);
            can->tx_buff[i * 2 + 1] = (uint8_t)(cur & 0xFF);
        }

        CANTransmit(can, CAN_TRANSMIT_TIMEOUT);
    }
}

#endif // HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED

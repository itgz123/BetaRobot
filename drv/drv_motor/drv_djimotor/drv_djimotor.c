/**
 * @file drv_djimotor.c
 * @brief 大疆电机驱动实现
 *
 * @note 支持电机型号：
 *       - M3508 (C620 电调): 电流控制
 *       - M2006 (C610 电调): 电流控制
 *       - GM6020: 电压/电流控制
 */

#include "drv_djimotor.h"

#if defined(BSP_CAN_MODULE_ENABLED)

#include "bsp_log.h"

/*============================================
 *              私有宏定义
 *============================================*/

/* RPM 转 rad/s */
#define RPM_TO_RAD_S (2.0f * 3.14159265358979323846f / 60.0f)

/*============================================
 *              私有函数声明
 *============================================*/

static int8_t DJIMotor_Init(MotorInstance *base);
static void DJIMotor_Enable(MotorInstance *base);
static void DJIMotor_Stop(MotorInstance *base);
static void DJIMotor_SetRef(MotorInstance *base, float ref);
static void DJIMotor_SetSpeed_VTable(MotorInstance *base, float speed);
static void DJIMotor_SetOuterLoop(MotorInstance *base, MotorLoopType_e loop);
static void DJIMotor_GetStatus(MotorInstance *base, MotorStatus_t *status);
static int8_t DJIMotor_SetPID_VTable(MotorInstance *base, MotorLoopType_e loop,
                                     float kp, float ki, float kd, float integral_limit, float max_value);

static void DJIMotor_ParseFeedback(DJIMotorInstance *inst, uint8_t *data);
static void DJIMotor_UpdateOnlineStatus(DJIMotorInstance *inst);
static int16_t DJIMotor_ClampValue(int16_t value, int16_t max);
static float DJIMotor_ConvertCurrent(DJIMotorInstance *inst, int16_t raw);
static void DJIMotor_RxCallback(CANInstance *can_inst);
static void DJIMotorGroup_RxCallback(CANInstance *can_inst);
static void DJIMotorSetSpeed(DJIMotorInstance *inst, float speed);
static void DJIMotorSetAngle(DJIMotorInstance *inst, float angle);

/*============================================
 *              虚函数表定义
 *============================================*/

const MotorInterface_s g_dji_motor_interface = {
    .init = DJIMotor_Init,
    .enable = DJIMotor_Enable,
    .stop = DJIMotor_Stop,
    .set_ref = DJIMotor_SetRef,
    .set_speed = DJIMotor_SetSpeed_VTable,
    .set_outer_loop = DJIMotor_SetOuterLoop,
    .get_status = DJIMotor_GetStatus,
    .set_pid = DJIMotor_SetPID_VTable,
};

/*============================================
 *              私有函数实现
 *============================================*/

/**
 * @brief 限制指令值范围
 */
static int16_t DJIMotor_ClampValue(int16_t value, int16_t max)
{
    if (value > max)
        return max;
    if (value < -max)
        return -max;
    return value;
}

/**
 * @brief 将原始电流值转换为实际电流 (A)
 */
static float DJIMotor_ConvertCurrent(DJIMotorInstance *inst, int16_t raw)
{
    switch (inst->type)
    {
    case DJI_MOTOR_M2006:
        /* M2006: 1000 = 1A */
        return (float)raw / 1000.0f;
    case DJI_MOTOR_M3508:
        /* M3508: 819.2 = 1A */
        return (float)raw / 819.2f;
    case DJI_MOTOR_GM6020:
        /* GM6020: 电流模式下 5461.3 = 1A */
        return (float)raw / 5461.3f;
    default:
        return 0.0f;
    }
}

/**
 * @brief 解析反馈数据
 * @param inst 电机实例指针
 * @param data 8 字节反馈数据
 */
static void DJIMotor_ParseFeedback(DJIMotorInstance *inst, uint8_t *data)
{
    int16_t angle_diff;

    /* 解析原始数据 */
    inst->angle_raw = (uint16_t)((data[0] << 8) | data[1]);
    inst->speed_rpm = (int16_t)((data[2] << 8) | data[3]);
    inst->current_raw = (int16_t)((data[4] << 8) | data[5]);
    inst->temperature = data[6];

    /* 计算多圈角度 */
    angle_diff = (int16_t)inst->angle_raw - (int16_t)inst->last_angle_raw;
    /* 处理过零点 */
    if (angle_diff > 4096)
    {
        angle_diff -= 8192;
    }
    else if (angle_diff < -4096)
    {
        angle_diff += 8192;
    }
    inst->angle_total += angle_diff * DJI_ANGLE_TO_RAD;
    inst->last_angle_raw = inst->angle_raw;

    /* 转换为物理量 */
    inst->angle_rad = inst->angle_raw * DJI_ANGLE_TO_RAD;
    inst->speed_rad_s = inst->speed_rpm * RPM_TO_RAD_S;
    inst->current_a = DJIMotor_ConvertCurrent(inst, inst->current_raw);

    /* 更新接收时间 */
    inst->last_rx_time = DWT_GetTimeline_us();
    inst->online = 1;

    /* 更新基类状态 */
    inst->base.status.speed = inst->speed_rad_s;
    inst->base.status.angle = inst->angle_total;
    inst->base.status.current = inst->current_a;
    inst->base.status.temperature = (float)inst->temperature;
    inst->base.status.online = 1;
}

/**
 * @brief 更新在线状态（检查超时）
 */
static void DJIMotor_UpdateOnlineStatus(DJIMotorInstance *inst)
{
    uint64_t now = DWT_GetTimeline_us();
    if ((now - inst->last_rx_time) > DJI_OFFLINE_TIMEOUT)
    {
        inst->online = 0;
        inst->base.status.online = 0;
    }
}

/**
 * @brief 虚函数表：初始化
 */
static int8_t DJIMotor_Init(MotorInstance *base)
{
    DJIMotorInstance *inst = (DJIMotorInstance *)base;
    if (!inst)
    {
        LOGERROR("[drv_djimotor] Instance is NULL!");
        return -1;
    }

    /* 检查 CAN 实例 */
    if (!inst->can_inst)
    {
        LOGERROR("[drv_djimotor] can_inst is NULL!");
        return -1;
    }

    /* 检查电机 ID */
    if (inst->motor_id < 1 || inst->motor_id > 8)
    {
        LOGERROR("[drv_djimotor] Invalid motor_id: %d", inst->motor_id);
        return -1;
    }

    /* GM6020 ID 范围为 1-7 */
    if (inst->type == DJI_MOTOR_GM6020 && inst->motor_id > 7)
    {
        LOGERROR("[drv_djimotor] GM6020 motor_id must be 1-7");
        return -1;
    }

    /* 设置 CAN 实例的 parent 指针 */
    inst->can_inst->parent = inst;

    /* 设置接收回调 */
    inst->can_inst->rx_callback = DJIMotor_RxCallback;

    /* 设置发送帧 ID 和接收过滤器 */
    uint32_t rx_id;
    switch (inst->type)
    {
    case DJI_MOTOR_M3508:
    case DJI_MOTOR_M2006:
        /* M3508/M2006 反馈 ID: 0x200 + motor_id */
        rx_id = 0x200 + inst->motor_id;
        /* 根据电机 ID 确定发送帧 ID */
        if (inst->motor_id <= 4)
        {
            inst->can_inst->tx_id = 0x200;
        }
        else
        {
            inst->can_inst->tx_id = 0x1FF;
        }
        inst->cmd_max = (inst->type == DJI_MOTOR_M2006) ? DJI_M2006_CURRENT_MAX : DJI_M3508_CURRENT_MAX;
        break;

    case DJI_MOTOR_GM6020:
        /* GM6020 反馈 ID: 0x204 + motor_id */
        rx_id = 0x204 + inst->motor_id;
        /* 根据电机 ID 和模式确定发送帧 ID */
        if (inst->motor_id <= 4)
        {
            inst->can_inst->tx_id = (inst->mode == DJI_MODE_VOLTAGE) ? 0x1FF : 0x1FE;
        }
        else
        {
            inst->can_inst->tx_id = (inst->mode == DJI_MODE_VOLTAGE) ? 0x2FF : 0x2FE;
        }
        inst->cmd_max = (inst->mode == DJI_MODE_VOLTAGE) ? DJI_GM6020_VOLTAGE_MAX : DJI_GM6020_CURRENT_MAX;
        break;

    default:
        LOGERROR("[drv_djimotor] Unknown motor type: %d", inst->type);
        return -1;
    }

    /* 配置过滤器（掩码模式精确匹配） */
    inst->can_inst->filter_mode = CAN_FILTER_MODE_MASK;
    inst->can_inst->rx_id_list[0] = rx_id;
    inst->can_inst->rx_mask = 0x7FF;
    inst->can_inst->rx_id_count = 1;

    /* 初始化时间 */
    inst->last_time = DWT_GetTimeline_us();
    inst->last_rx_time = DWT_GetTimeline_us();

    /* 初始化 PID 控制器 */
    PIDInit(&inst->pid_speed, inst->pid_speed.kp, inst->pid_speed.ki, inst->pid_speed.kd);
    PIDInit(&inst->pid_angle, inst->pid_angle.kp, inst->pid_angle.ki, inst->pid_angle.kd);

    /* 设置 impl 指针 */
    inst->base.impl = inst;

    /* 注册 CAN 实例 */
    CANRegister(inst->can_inst);

    /* 设置 DLC */
    CANSetDLC(inst->can_inst, 8);

    LOGINFO("[drv_djimotor] Motor initialized, type=%d, id=%d", inst->type, inst->motor_id);
    return 0;
}

/**
 * @brief 虚函数表：使能电机
 */
static void DJIMotor_Enable(MotorInstance *base)
{
    DJIMotorInstance *inst = (DJIMotorInstance *)base;
    if (inst)
    {
        inst->cmd_value = 0;
        inst->online = 0;
        inst->last_time = DWT_GetTimeline_us();
        inst->last_rx_time = DWT_GetTimeline_us();
    }
}

/**
 * @brief 虚函数表：停止电机
 */
static void DJIMotor_Stop(MotorInstance *base)
{
    DJIMotorInstance *inst = (DJIMotorInstance *)base;
    if (inst)
    {
        inst->cmd_value = 0;
        inst->outer_loop = MOTOR_LOOP_OPEN;
    }
}

/**
 * @brief 虚函数表：设置参考值
 * @note 根据外层闭环类型决定参考值含义
 */
static void DJIMotor_SetRef(MotorInstance *base, float ref)
{
    DJIMotorInstance *inst = (DJIMotorInstance *)base;
    if (!inst)
        return;

    switch (inst->outer_loop)
    {
    case MOTOR_LOOP_CURRENT:
        /* ref 为电流 (A)，转换为原始值 */
        if (inst->type == DJI_MOTOR_M2006)
        {
            inst->cmd_value = DJIMotor_ClampValue((int16_t)(ref * 1000.0f), DJI_M2006_CURRENT_MAX);
        }
        else if (inst->type == DJI_MOTOR_M3508)
        {
            inst->cmd_value = DJIMotor_ClampValue((int16_t)(ref * 819.2f), DJI_M3508_CURRENT_MAX);
        }
        else if (inst->type == DJI_MOTOR_GM6020)
        {
            inst->cmd_value = DJIMotor_ClampValue((int16_t)(ref * 5461.3f), DJI_GM6020_CURRENT_MAX);
        }
        break;

    case MOTOR_LOOP_SPEED:
        DJIMotorSetSpeed(inst, ref);
        break;

    case MOTOR_LOOP_ANGLE:
        DJIMotorSetAngle(inst, ref);
        break;

    default:
        break;
    }
}

/**
 * @brief 虚函数表：设置速度（PID 控制）
 */
static void DJIMotor_SetSpeed_VTable(MotorInstance *base, float speed)
{
    DJIMotorSetSpeed((DJIMotorInstance *)base, speed);
}

/**
 * @brief 虚函数表：设置外层闭环类型
 */
static void DJIMotor_SetOuterLoop(MotorInstance *base, MotorLoopType_e loop)
{
    DJIMotorInstance *inst = (DJIMotorInstance *)base;
    if (inst)
    {
        inst->outer_loop = loop;
    }
}

/**
 * @brief 虚函数表：获取电机状态
 */
static void DJIMotor_GetStatus(MotorInstance *base, MotorStatus_t *status)
{
    DJIMotorInstance *inst = (DJIMotorInstance *)base;
    if (inst && status)
    {
        DJIMotor_UpdateOnlineStatus(inst);
        *status = inst->base.status;
    }
}

/**
 * @brief 虚函数表：设置 PID 参数
 */
static int8_t DJIMotor_SetPID_VTable(MotorInstance *base, MotorLoopType_e loop,
                                     float kp, float ki, float kd, float integral_limit, float max_value)
{
    DJIMotorInstance *inst = (DJIMotorInstance *)base;
    if (!inst)
    {
        LOGERROR("[drv_djimotor] Instance is NULL!");
        return -1;
    }

    switch (loop)
    {
    case MOTOR_LOOP_SPEED:
        inst->pid_speed.kp = kp;
        inst->pid_speed.ki = ki;
        inst->pid_speed.kd = kd;
        inst->pid_speed.integral_limit = integral_limit;
        inst->max_speed = max_value;
        PIDInit(&inst->pid_speed, kp, ki, kd);
        LOGINFO("[drv_djimotor] Speed PID configured: kp=%.2f, ki=%.2f, kd=%.2f, limit=%.2f, max=%.2f",
                kp, ki, kd, integral_limit, max_value);
        break;

    case MOTOR_LOOP_ANGLE:
        inst->pid_angle.kp = kp;
        inst->pid_angle.ki = ki;
        inst->pid_angle.kd = kd;
        inst->pid_angle.integral_limit = integral_limit;
        PIDInit(&inst->pid_angle, kp, ki, kd);
        LOGINFO("[drv_djimotor] Angle PID configured: kp=%.2f, ki=%.2f, kd=%.2f, limit=%.2f",
                kp, ki, kd, integral_limit);
        break;

    default:
        LOGWARNING("[drv_djimotor] Unsupported loop type: %d", loop);
        return -1;
    }

    return 0;
}

/**
 * @brief CAN 接收回调函数（分组控制）
 * @note 根据接收 ID 匹配电机并解析数据
 */
static void DJIMotorGroup_RxCallback(CANInstance *can_inst)
{
    DJIMotorGroup *group = (DJIMotorGroup *)can_inst->parent;
    if (!group)
        return;

    /* 获取反馈 ID */
    uint32_t rx_id = can_inst->rx_id_matched;

    /* 遍历电机组，匹配 ID */
    for (uint8_t i = 0; i < group->motor_count; i++)
    {
        DJIMotorInstance *motor = group->motors[i];
        if (motor)
        {
            uint32_t expected_id;
            /* 计算期望的反馈 ID */
            if (motor->type == DJI_MOTOR_GM6020)
            {
                expected_id = 0x204 + motor->motor_id;
            }
            else
            {
                expected_id = 0x200 + motor->motor_id;
            }

            if (rx_id == expected_id)
            {
                DJIMotor_ParseFeedback(motor, can_inst->rx_buff);
                break;
            }
        }
    }
}

/**
 * @brief CAN 接收回调函数（独立控制）
 */
static void DJIMotor_RxCallback(CANInstance *can_inst)
{
    DJIMotorInstance *inst = (DJIMotorInstance *)can_inst->parent;
    if (inst)
    {
        DJIMotor_ParseFeedback(inst, can_inst->rx_buff);
    }
}

/*============================================
 *              公有函数实现
 *============================================*/

/*----------------- 电机组接口 -----------------*/

int8_t DJIMotorGroupInit(DJIMotorGroup *group, uint8_t tx_id_base)
{
    if (!group)
    {
        LOGWARNING("[drv_djimotor] DJIMotorGroupInit: invalid params");
        return -1;
    }

    /* 设置发送 ID */
    group->tx_id_base = tx_id_base;
    group->can_inst.tx_id = tx_id_base;
    group->can_inst.parent = group;
    group->can_inst.rx_callback = DJIMotorGroup_RxCallback;
    group->can_inst.filter_mode = CAN_FILTER_MODE_LIST;
    group->can_inst.rx_id_count = 0;
    group->motor_count = 0;

    return 0;
}

int8_t DJIMotorGroupAddMotor(DJIMotorGroup *group, DJIMotorInstance *motor, uint8_t slot_index)
{
    if (!group || !motor || slot_index >= 4)
    {
        LOGWARNING("[drv_djimotor] DJIMotorGroupAddMotor: invalid params");
        return -1;
    }

    if (group->motors[slot_index] != NULL)
    {
        LOGWARNING("[drv_djimotor] DJIMotorGroupAddMotor: slot %d already occupied", slot_index);
        return -1;
    }

    if (group->motor_count >= 4)
    {
        LOGWARNING("[drv_djimotor] DJIMotorGroupAddMotor: group is full");
        return -1;
    }

    /* 添加电机到组 */
    group->motors[slot_index] = motor;
    group->motor_count++;

    /* 设置电机的组和槽位 */
    motor->group = group;
    motor->slot_index = slot_index;
    motor->can_inst = &group->can_inst;

    /* 根据电机型号设置指令最大值 */
    switch (motor->type)
    {
    case DJI_MOTOR_M2006:
        motor->cmd_max = DJI_M2006_CURRENT_MAX;
        break;
    case DJI_MOTOR_M3508:
        motor->cmd_max = DJI_M3508_CURRENT_MAX;
        break;
    case DJI_MOTOR_GM6020:
        motor->cmd_max = (motor->mode == DJI_MODE_VOLTAGE) ? DJI_GM6020_VOLTAGE_MAX : DJI_GM6020_CURRENT_MAX;
        break;
    default:
        break;
    }

    /* 添加接收 ID 到过滤器列表 */
    uint32_t rx_id;
    if (motor->type == DJI_MOTOR_GM6020)
    {
        rx_id = 0x204 + motor->motor_id;
    }
    else
    {
        rx_id = 0x200 + motor->motor_id;
    }

    /* 查找空闲位置添加 ID */
    uint8_t id_idx = group->can_inst.rx_id_count;
    if (id_idx < 4)
    {
        group->can_inst.rx_id_list[id_idx] = rx_id;
        group->can_inst.rx_id_count++;
    }

    /* 初始化时间 */
    motor->last_time = DWT_GetTimeline_us();
    motor->last_rx_time = DWT_GetTimeline_us();

    /* 初始化 PID 控制器 */
    PIDInit(&motor->pid_speed, motor->pid_speed.kp, motor->pid_speed.ki, motor->pid_speed.kd);
    PIDInit(&motor->pid_angle, motor->pid_angle.kp, motor->pid_angle.ki, motor->pid_angle.kd);

    /* 设置 impl 指针 */
    motor->base.impl = motor;

    /* 如果是第一个电机，注册 CAN 实例 */
    if (group->motor_count == 1)
    {
        CANRegister(&group->can_inst);
        CANSetDLC(&group->can_inst, 8);
    }

    return 0;
}

void DJIMotorGroupRefresh(DJIMotorGroup *group)
{
    if (!group || group->motor_count == 0)
        return;

    /* 清空发送缓冲区 */
    for (uint8_t i = 0; i < 8; i++)
    {
        group->can_inst.tx_buff[i] = 0;
    }

    /* 填充各电机的指令值 */
    for (uint8_t i = 0; i < 4; i++)
    {
        if (group->motors[i])
        {
            DJIMotorInstance *motor = group->motors[i];

            /* 如果是速度环或位置环，先计算 PID */
            if (motor->outer_loop == MOTOR_LOOP_SPEED && motor->online)
            {
                DJIMotorSetSpeed(motor, motor->base.status.speed); /* 保持当前目标速度 */
            }
            else if (motor->outer_loop == MOTOR_LOOP_ANGLE && motor->online)
            {
                DJIMotorSetAngle(motor, motor->base.status.angle); /* 保持当前目标角度 */
            }

            /* 填充指令到缓冲区 */
            group->can_inst.tx_buff[i * 2] = (motor->cmd_value >> 8) & 0xFF;
            group->can_inst.tx_buff[i * 2 + 1] = motor->cmd_value & 0xFF;
        }
    }

    /* 发送 CAN 帧 */
    CANTransmit(&group->can_inst, 1U);
}

/*----------------- 控制函数 -----------------*/

void DJIMotorSetRawValue(DJIMotorInstance *inst, int16_t value)
{
    if (!inst)
        return;

    inst->cmd_value = DJIMotor_ClampValue(value, inst->cmd_max);

    /* 如果是独立控制，立即发送 */
    if (inst->can_inst && inst->group == NULL)
    {
        /* 计算在 CAN 帧中的位置 */
        uint8_t slot = (inst->motor_id <= 4) ? (inst->motor_id - 1) : (inst->motor_id - 5);

        /* 填充发送缓冲区 */
        inst->can_inst->tx_buff[slot * 2] = (inst->cmd_value >> 8) & 0xFF;
        inst->can_inst->tx_buff[slot * 2 + 1] = inst->cmd_value & 0xFF;

        /* 发送 */
        CANTransmit(inst->can_inst, 1U);
    }
}

void DJIMotorSetSpeed(DJIMotorInstance *inst, float speed)
{
    if (!inst || !inst->online)
        return;

    uint64_t now = DWT_GetTimeline_us();
    float dt = (now - inst->last_time) / 1000000.0f;
    inst->last_time = now;

    /* 归一化速度 */
    float measure_norm = inst->speed_rad_s / inst->max_speed;
    float setpoint_norm = speed / inst->max_speed;

    /* PID 计算 */
    float output = PIDCalculate(&inst->pid_speed, setpoint_norm, measure_norm, dt, 0.0f);

    /* 转换为原始值 */
    int16_t raw_value = (int16_t)(output * inst->cmd_max);

    DJIMotorSetRawValue(inst, raw_value);
}

void DJIMotorSetAngle(DJIMotorInstance *inst, float angle)
{
    if (!inst || !inst->online)
        return;

    uint64_t now = DWT_GetTimeline_us();
    float dt = (now - inst->last_time) / 1000000.0f;
    inst->last_time = now;

    /* 位置环 PID */
    float speed_ref = PIDCalculate(&inst->pid_angle, angle, inst->angle_total, dt, 0.0f);

    /* 调用速度环 */
    DJIMotorSetSpeed(inst, speed_ref);
}

void DJIMotorSetOuterLoop(DJIMotorInstance *inst, MotorLoopType_e loop)
{
    if (inst)
    {
        inst->outer_loop = loop;
    }
}

/*----------------- 状态获取函数 -----------------*/

float DJIMotorGetAngle(DJIMotorInstance *inst)
{
    if (inst)
    {
        return inst->angle_total;
    }
    return 0.0f;
}

float DJIMotorGetSpeed(DJIMotorInstance *inst)
{
    if (inst)
    {
        return inst->speed_rad_s;
    }
    return 0.0f;
}

float DJIMotorGetCurrent(DJIMotorInstance *inst)
{
    if (inst)
    {
        return inst->current_a;
    }
    return 0.0f;
}

uint8_t DJIMotorGetTemperature(DJIMotorInstance *inst)
{
    if (inst)
    {
        return inst->temperature;
    }
    return 0;
}

uint8_t DJIMotorIsOnline(DJIMotorInstance *inst)
{
    if (inst)
    {
        DJIMotor_UpdateOnlineStatus(inst);
        return inst->online;
    }
    return 0;
}

/*----------------- GM6020 专用函数 -----------------*/

void DJIMotorSetMode(DJIMotorInstance *inst, DJIMotorMode_e mode)
{
    if (!inst || inst->type != DJI_MOTOR_GM6020)
        return;

    inst->mode = mode;
    inst->cmd_max = (mode == DJI_MODE_VOLTAGE) ? DJI_GM6020_VOLTAGE_MAX : DJI_GM6020_CURRENT_MAX;

    /* 更新 CAN 发送 ID */
    if (inst->can_inst && inst->group == NULL)
    {
        if (inst->motor_id <= 4)
        {
            inst->can_inst->tx_id = (mode == DJI_MODE_VOLTAGE) ? 0x1FF : 0x1FE;
        }
        else
        {
            inst->can_inst->tx_id = (mode == DJI_MODE_VOLTAGE) ? 0x2FF : 0x2FE;
        }
    }
}

#endif /* BSP_CAN_MODULE_ENABLED */
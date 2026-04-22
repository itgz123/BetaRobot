/**
 * @file drv_chassis.c
 * @brief 底盘运动学解算实现
 *
 * @note 电机编号：1-4 对应 左前(LF)、左后(LB)、右后(RB)、右前(RF)
 *       坐标系：向前 +X，向左 +Y，逆时针旋转 +W
 *       轮子转向：前进方向为正
 */

#include "drv_chassis.h"

#ifdef HAL_TIM_MODULE_ENABLED

#include "app_cfg.h"
#include "bsp_log.h"

/*============================================
 *              私有常量
 *============================================*/

#define SQRT2 1.41421356f

/*============================================
 *              运动学逆解函数实现
 *============================================*/

void InverseMecanumX(ChassisInstance *inst, float vx, float vy, float w, WheelSpeed_t *out)
{
    float r = inst->wheel_radius;
    float k = inst->chassis_l / r; // 旋转系数
    out->w1 = (vx - vy - w * k) / r;
    out->w2 = (vx + vy - w * k) / r;
    out->w3 = (vx - vy + w * k) / r;
    out->w4 = (vx + vy + w * k) / r;
}

void InverseMecanumO(ChassisInstance *inst, float vx, float vy, float w, WheelSpeed_t *out)
{
    float r = inst->wheel_radius;
    float k = inst->chassis_l / r;
    out->w1 = (vx + vy - w * k) / r;
    out->w2 = (vx - vy - w * k) / r;
    out->w3 = (vx + vy + w * k) / r;
    out->w4 = (vx - vy + w * k) / r;
}

void InverseOmniX(ChassisInstance *inst, float vx, float vy, float w, WheelSpeed_t *out)
{
    float r = inst->wheel_radius;
    float k = inst->chassis_l / r;
    out->w1 = (vx - vy - w * k) / (r * SQRT2);
    out->w2 = (vx + vy - w * k) / (r * SQRT2);
    out->w3 = (vx - vy + w * k) / (r * SQRT2);
    out->w4 = (vx + vy + w * k) / (r * SQRT2);
}

void InverseOmniCross(ChassisInstance *inst, float vx, float vy, float w, WheelSpeed_t *out)
{
    float r = inst->wheel_radius;
    out->w1 = (vy + w * inst->cross_a / r) / r;
    out->w2 = (-vx + w * inst->cross_b / r) / r;
    out->w3 = (vx + w * inst->cross_b / r) / r;
    out->w4 = (-vy + w * inst->cross_a / r) / r;
}

/*============================================
 *              运动学正解函数实现
 *============================================*/

ChassisVelCmd_t ForwardMecanumX(ChassisInstance *inst, WheelSpeed_t *wheel)
{
    ChassisVelCmd_t cmd;
    float r = inst->wheel_radius;
    float k = r / inst->chassis_l;
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * r / 4.0f;
    cmd.vy = (-wheel->w1 + wheel->w2 - wheel->w3 + wheel->w4) * r / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * k / 4.0f;
    cmd.mode = 0;
    return cmd;
}

ChassisVelCmd_t ForwardMecanumO(ChassisInstance *inst, WheelSpeed_t *wheel)
{
    ChassisVelCmd_t cmd;
    float r = inst->wheel_radius;
    float k = r / inst->chassis_l;
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * r / 4.0f;
    cmd.vy = (wheel->w1 - wheel->w2 + wheel->w3 - wheel->w4) * r / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * k / 4.0f;
    cmd.mode = 0;
    return cmd;
}

ChassisVelCmd_t ForwardOmniX(ChassisInstance *inst, WheelSpeed_t *wheel)
{
    ChassisVelCmd_t cmd;
    float r = inst->wheel_radius;
    float k = r / inst->chassis_l;
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * r * SQRT2 / 4.0f;
    cmd.vy = (-wheel->w1 + wheel->w2 - wheel->w3 + wheel->w4) * r * SQRT2 / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * k / 4.0f;
    cmd.mode = 0;
    return cmd;
}

ChassisVelCmd_t ForwardOmniCross(ChassisInstance *inst, WheelSpeed_t *wheel)
{
    ChassisVelCmd_t cmd;
    float r = inst->wheel_radius;
    cmd.vx = (-wheel->w2 + wheel->w3) * r / 2.0f;
    cmd.vy = (wheel->w1 - wheel->w4) * r / 2.0f;
    cmd.w = (wheel->w1 * inst->cross_a + wheel->w2 * inst->cross_b + wheel->w3 * inst->cross_b + wheel->w4 * inst->cross_a) * r / 4.0f;
    cmd.mode = 0;
    return cmd;
}

/*============================================
 *              初始化函数
 *============================================*/

void ChassisInit(ChassisInstance *inst, MotorInstance *m1, MotorInstance *m2, MotorInstance *m3, MotorInstance *m4, float wheel_radius, float wheelbase_a, float wheelbase_b, float chassis_l, float cross_a, float cross_b, ChassisType_e type, float max_speed, float max_w)
{
    if (inst == NULL)
    {
        LOGERROR("[drv_chassis] Instance is NULL!");
        return;
    }

    // 设置配置参数
    inst->wheel_radius = wheel_radius;
    inst->wheelbase_a = wheelbase_a;
    inst->wheelbase_b = wheelbase_b;
    inst->chassis_l = chassis_l;
    inst->cross_a = cross_a;
    inst->cross_b = cross_b;
    inst->type = type;
    inst->max_speed = max_speed;
    inst->max_w = max_w;

    // 设置电机指针
    inst->motors[0] = m1;
    inst->motors[1] = m2;
    inst->motors[2] = m3;
    inst->motors[3] = m4;

    // 根据底盘类型设置运动学函数指针
    switch (type)
    {
    case CHASSIS_TYPE_MECANUM_X:
        inst->inverse_kinematics = InverseMecanumX;
        inst->forward_kinematics = ForwardMecanumX;
        break;
    case CHASSIS_TYPE_MECANUM_O:
        inst->inverse_kinematics = InverseMecanumO;
        inst->forward_kinematics = ForwardMecanumO;
        break;
    case CHASSIS_TYPE_OMNI_X:
        inst->inverse_kinematics = InverseOmniX;
        inst->forward_kinematics = ForwardOmniX;
        break;
    case CHASSIS_TYPE_OMNI_CROSS:
        inst->inverse_kinematics = InverseOmniCross;
        inst->forward_kinematics = ForwardOmniCross;
        break;
    default:
        inst->inverse_kinematics = InverseMecanumX;
        inst->forward_kinematics = ForwardMecanumX;
        break;
    }

    // 初始化状态
    inst->vel_cmd.vx = 0;
    inst->vel_cmd.vy = 0;
    inst->vel_cmd.w = 0;
    inst->vel_cmd.mode = CHASSIS_MODE_DISABLE;
    inst->wheel_speed.w1 = 0;
    inst->wheel_speed.w2 = 0;
    inst->wheel_speed.w3 = 0;
    inst->wheel_speed.w4 = 0;

    LOGINFO("[drv_chassis] Chassis initialized, type=%d", type);
}

/*============================================
 *              控制函数
 *============================================*/

void ChassisStop(ChassisInstance *inst)
{
    if (inst == NULL)
    {
        return;
    }

    // 停止所有电机
    for (int i = 0; i < 4; i++)
    {
        if (inst->motors[i])
        {
            MotorStop(inst->motors[i]);
        }
    }

    // 清零状态
    inst->vel_cmd.vx = 0;
    inst->vel_cmd.vy = 0;
    inst->vel_cmd.w = 0;
    inst->wheel_speed.w1 = 0;
    inst->wheel_speed.w2 = 0;
    inst->wheel_speed.w3 = 0;
    inst->wheel_speed.w4 = 0;
}

void ChassisSetVel(ChassisInstance *inst, ChassisVelCmd_t *cmd)
{
    if (inst == NULL || cmd == NULL)
    {
        return;
    }

    // 保存模式
    inst->vel_cmd.mode = cmd->mode;

    // 根据模式处理
    switch (cmd->mode)
    {
    case CHASSIS_MODE_DISABLE:
        // 失能模式：停止底盘
        ChassisStop(inst);
        return;

    case CHASSIS_MODE_HEADLESS:
        // 无头模式：暂与 DISABLE 相同（待实现）
        // TODO: 需要陀螺仪数据支持
        ChassisStop(inst);
        return;

    case CHASSIS_MODE_ENABLE:
        // 使能模式：正常控制
        break;

    default:
        return;
    }

    // 归一化输入转换为实际速度
    float vx = cmd->vx * inst->max_speed;
    float vy = cmd->vy * inst->max_speed;
    float w = cmd->w * inst->max_w;

    // 保存速度命令
    inst->vel_cmd.vx = vx;
    inst->vel_cmd.vy = vy;
    inst->vel_cmd.w = w;

    // 调用运动学逆解函数指针
    if (inst->inverse_kinematics == NULL)
    {
        return;
    }
    inst->inverse_kinematics(inst, vx, vy, w, &inst->wheel_speed);

    // 通过电机 vtable 控制电机
    if (inst->motors[0])
        MotorSetSpeed(inst->motors[0], inst->wheel_speed.w1);
    if (inst->motors[1])
        MotorSetSpeed(inst->motors[1], inst->wheel_speed.w2);
    if (inst->motors[2])
        MotorSetSpeed(inst->motors[2], inst->wheel_speed.w3);
    if (inst->motors[3])
        MotorSetSpeed(inst->motors[3], inst->wheel_speed.w4);
}

#endif /* HAL_TIM_MODULE_ENABLED */

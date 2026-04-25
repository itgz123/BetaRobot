/**
 * @file drv_motor_base.c
 * @brief 电机抽象基类实现
 *
 * @note 实现统一接口的分发逻辑
 */

#include "drv_motor_base.h"
#include "bsp_log.h"

/*============================================
 *              外部虚函数表声明
 *============================================*/

/* DC 电机虚函数表 */
extern const MotorInterface_s g_dc_motor_interface;

/* DJI 电机虚函数表 */
extern const MotorInterface_s g_dji_motor_interface;

/*============================================
 *              基类统一接口实现
 *============================================*/

/**
 * @brief 初始化电机（统一入口）
 * @note 根据 inst->type 绑定对应的虚函数表，然后调用 init
 */
int8_t MotorInit(MotorInstance *inst)
{
    if (inst == NULL)
    {
        LOGERROR("[MotorInit] inst is NULL");
        return -1;
    }

    /* 根据 motor type 绑定虚函数表 */
    switch (inst->type)
    {
    case MOTOR_TYPE_DC:
        inst->vtable = &g_dc_motor_interface;
        break;
    case MOTOR_TYPE_DJI:
        inst->vtable = &g_dji_motor_interface;
        break;
    case MOTOR_TYPE_DM:
        /* TODO: 添加 DM 电机支持 */
        LOGERROR("[MotorInit] MOTOR_TYPE_DM not implemented");
        return -1;
    case MOTOR_TYPE_LK:
        /* TODO: 添加 LK 电机支持 */
        LOGERROR("[MotorInit] MOTOR_TYPE_LK not implemented");
        return -1;
    default:
        LOGERROR("[MotorInit] Unknown motor type: %d", inst->type);
        return -1;
    }

    /* 调用具体电机类型的初始化函数 */
    if (inst->vtable && inst->vtable->init)
    {
        return inst->vtable->init(inst);
    }

    LOGERROR("[MotorInit] vtable or init is NULL");
    return -1;
}

/**
 * @brief 设置 PID 参数（统一入口）
 * @note 调用具体电机类型的 set_pid 函数
 */
int8_t MotorSetPID(MotorInstance *inst, MotorLoopType_e loop,
                   float kp, float ki, float kd, float integral_limit, float max_value)
{
    if (inst == NULL)
    {
        LOGERROR("[MotorSetPID] inst is NULL");
        return -1;
    }

    if (inst->vtable && inst->vtable->set_pid)
    {
        return inst->vtable->set_pid(inst, loop, kp, ki, kd, integral_limit, max_value);
    }

    LOGERROR("[MotorSetPID] vtable or set_pid is NULL");
    return -1;
}

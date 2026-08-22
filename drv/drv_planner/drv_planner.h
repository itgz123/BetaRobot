/**
 * @file drv_planner.h
 * @brief 单轴运动规划器驱动
 * @author TRW
 * @date 2026-08-22
 *
 * @note 输入目标指令（-1~1，如 dbus 遥控器摇杆通道值），内部按 max_speed 缩放为目标速度，
 *       输出设定位置/设定速度/设定加速度，供 AxisMitLiteCalculate 等控制器使用。
 * @note 位置模式与电机驱动 MotorPositionMode_e 对应（限幅/环绕/连续），
 *       位置限幅、最大速度、最大加速度在初始化时写入。
 * @note 加速度设定 = 当前加速度 + (设定速度 - 当前速度)/dt，并限幅到 ±max_acc；
 *       位置设定 = 当前位置 + 设定速度*dt 后按位置模式处理（限幅/归一化/不限幅）。
 */

#ifndef __DRV_PLANNER_H
#define __DRV_PLANNER_H

#include <stdint.h>

/*============================================
 *              位置模式枚举
 *（与 drv_motor_base.h 的 MotorPositionMode_e 对应）
 *============================================*/
typedef enum : uint8_t
{
    PLANNER_POS_LIMITED = 0,    // 限幅模式：位置限幅到 [pos_limit_min, pos_limit_max]
    PLANNER_POS_WRAP = 1,       // 环绕模式：位置归一化到 [pos_limit_min, pos_limit_max)
    PLANNER_POS_CONTINUOUS = 2, // 连续模式：不限幅
} PlannerPositionMode_e;

/*============================================
 *              初始化配置
 *============================================*/
typedef struct
{
    PlannerPositionMode_e position_mode; // 位置模式
    float pos_limit_min;                 // 位置下限 (rad)，LIMITED: 限幅下限, WRAP: 归一化下限
    float pos_limit_max;                 // 位置上限 (rad)，LIMITED: 限幅上限, WRAP: 归一化上限
    float max_speed;                     // 最大速度 (rad/s)
    float max_acc;                       // 最大加速度 (rad/s²)
} Planner_Init_Config_s;

/*============================================
 *              计算输入（反馈 + 目标）
 *============================================*/
typedef struct
{
    float current_position;     // 当前位置 (rad)
    float current_speed;        // 当前速度 (rad/s)
    float current_acceleration; // 当前加速度 (rad/s²)，无加速度反馈时填 0
    float target_cmd;           // 目标指令 (-1~1)，如 dbus 摇杆通道值；内部按 max_speed 缩放为设定速度
} PlannerInput_s;

/*============================================
 *              计算输出（设定值）
 *============================================*/
typedef struct
{
    float position;     // 设定位置 (rad)
    float speed;        // 设定速度 (rad/s)
    float acceleration; // 设定加速度 (rad/s²)
} PlannerOutput_s;

/*============================================
 *              实例结构体
 *============================================*/
typedef struct
{
    Planner_Init_Config_s cfg; // 配置（初始化写入，运行期只读）
    uint64_t last_time_us;     // 上次计算时间戳 (us)
    uint8_t init_flag;         // 时间戳初始化标志，0=未初始化
} PlannerInstance;

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief 初始化规划器实例
 * @param inst 实例指针
 * @param cfg 配置结构体指针（位置限幅/位置模式/最大加速度/最大速度，只读）
 * @return 0: 成功, -1: 失败
 */
int8_t PlannerInit(PlannerInstance *inst, const Planner_Init_Config_s *cfg);

/**
 * @brief 计算设定值（给定反馈与目标速度，输出设定位置/速度/加速度）
 * @param inst 实例指针
 * @param in 输入结构体指针（当前位置/速度/加速度 + 目标速度，只读）
 * @param out 输出结构体指针（设定位置/速度/加速度）
 * @note 设定速度 = clamp(target_cmd × max_speed, ±max_speed)，作为"设定速度"输出不变；
 *       首次调用 dt 未知，位置输出 = 当前位置，加速度输出 = 当前加速度。
 * @note 相邻两次调用间隔超过 100ms 时积分按 100ms 保护，避免位置跳变。
 */
void PlannerCalculate(PlannerInstance *inst, const PlannerInput_s *in, PlannerOutput_s *out);

#endif // !__DRV_PLANNER_H

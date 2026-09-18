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
 *       位置设定为**内部开环累加**：ref_position += 设定速度*dt，再按位置模式处理
 *       （限幅/归一化/不限幅），处理结果写回累加器。位置设定不读反馈位置：外力
 *       （重力/手推/底盘甩）只体现为下游位置环的误差，指令不变时目标不变，松手后回到原目标。
 * @note 失能（stop）期间、以及视觉控制交还通道源时，调用方置 PlannerInput_s::seed=1，
 *       用当前反馈位置重置累加器，保证使能/接管瞬间目标不突变。
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
    float current_position;     // 当前位置 (rad)；仅用于播种累加器（seed=1 或首次调用），不参与外推
    float current_speed;        // 当前速度 (rad/s)
    float current_acceleration; // 当前加速度 (rad/s²)，无加速度反馈时填 0
    float target_cmd;           // 目标指令 (-1~1)，如 dbus 摇杆通道值；内部按 max_speed 缩放为设定速度
    uint8_t seed;               // 播种标志：1 = 本拍用 current_position 重置内部位置累加器
                                //   （失能期间每拍置 1；视觉交还通道源的首拍置 1）。
                                //   ⚠ 每个调用点都必须显式赋值，否则读到未初始化值会引起随机播种。
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
    uint8_t init_flag;         // 时间戳初始化标志，0=未初始化（只管 dt，与位置累加器无关）
    float ref_position;        // 位置设定累加器（开环积分结果，内部状态）
    uint8_t position_valid;    // 累加器有效标志，0=尚未播种（首次 Calculate 强制用反馈播种）
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
 * @brief 计算设定值（开环位置累加 + 速度/加速度前馈）
 * @param inst 实例指针
 * @param in 输入结构体指针（当前位置/速度/加速度 + 目标速度 + 播种标志，只读）
 * @param out 输出结构体指针（设定位置/速度/加速度）
 * @note 设定速度 = clamp(target_cmd × max_speed, ±max_speed)，作为"设定速度"输出不变。
 * @note 设定位置 = 内部累加器 += 设定速度×dt，再按位置模式处理并写回累加器：
 *       LIMITED 写回限幅值（防积分饱和），WRAP 写回归一化值（防多圈发散/精度丢失）。
 *       不读 current_position 做外推；仅当 in->seed=1 或 position_valid=0（首次调用）时，
 *       用 current_position 重置累加器。
 * @note 相邻两次调用间隔超过 PLANNER_DT_MAX_S（50ms，见 drv_planner.c）时，本次积分按该值
 *       截断，避免任务被拖长导致目标跳变。
 */
void PlannerCalculate(PlannerInstance *inst, const PlannerInput_s *in, PlannerOutput_s *out);

#endif // !__DRV_PLANNER_H

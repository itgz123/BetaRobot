/**
 * @file lib_chassis.h
 * @brief 底盘运动学统一内核：任意轮数、任意轮系的雅可比正/逆解与力分配
 * @author TRW
 * @date 2026-10-08
 *
 * @note 本模块只做几何数学解算（纯计算，不依赖 bsp/drv/motor，不含任何控制闭环）。
 *       麦轮/全向/舵轮、纯一类型/混用、1~8 轮，全部共用同一套代码：每个轮子贡献
 *       若干**约束行**，堆成 m×3 的 J。某一型底盘专有的轮系词汇（如四轮轮序）在
 *       lib_chassis_def.h，与本文件**互不包含**（内核需要的东西都在本文件里）。
 *
 * @note 坐标系（全项目统一）：x+ 向前, y+ 向左, w+ 逆时针（俯视）。
 *
 * @note 单个轮子的几何模型（接触点不滑）
 *       - 轮面滚动方向   e_r = (cos α, sin α)，α 为安装角（舵轮为当前舵角）
 *       - 轮面侧向单位向量 e_t = (-sin α, cos α)（e_r 逆时针转 90°）
 *       - 滚子角 β：驱动方向向量 g = e_r - cot(β)·e_t
 *           · 全向轮 / 舵轮（无斜滚子）：β = π/2 → cotβ = 0 → g = e_r（单位向量）
 *           · 麦轮：β = ±π/4 → cotβ = ±1 → g = e_r ∓ e_t（模长 √2）
 *         g 就是"把接触点速度投影成轮缘线速度 R·ω"的那个方向；麦轮的 √2 来自
 *         两个相互垂直的贡献叠加，不是笔误。
 *       - 接触点速度   v_i = v_center + w × r_i = (vx - w·y_i, vy + w·x_i)
 *
 * @note 每轮贡献的行数由"侧向能否自由滑动"决定（这是全向/麦轮与舵轮的唯一实质差别）：
 *       - 全向轮：滚子让接触点可沿 e_t 自由滑动 → 只有**驱动行**
 *                 R_i·ω_i = g_i · v_i                       →  J_i = [g_ix, g_iy, g_iy·x_i - g_ix·y_i]
 *       - 麦轮：  滚筒同样让接触点可沿 e_t 滑动（只是方向被 cotβ 改写）→ 只有**驱动行**，同式
 *       - 舵轮：  轮面不能侧向滑行（轮胎），除驱动行外还有**侧向约束行**
 *                 g_i^⊥ · v_i = 0，g_i^⊥ = (-g_iy, g_ix)     → [ -g_iy, g_ix, g_ix·x_i + g_iy·y_i ]
 *       于是 m = n_fixed + 2·n_rudder（≤ LIB_CHASSIS_MAX_ROW）。固定轮底盘 m=n；2 轮半舵 m=4，
 *       正是"2n 方程解 3 未知量"的老里程计内核。ξ 为速度时：
 *           s = J·ξ，s 的驱动行 = R_i·ω_i，侧向行 = 0。
 *
 * @note 广义 3 维量一律用 lib_math 的 vector3_t（数学通用，不另设 Twist/Wrench 结构）：
 *       - 速度 ξ = (x, y, z) = (vx [m/s], vy [m/s], w [rad/s])
 *       - 广义力 W = (x, y, z) = (Fx [N], Fy [N], Mz [N·m])
 *       两者数学上同为三维矢量，正解/逆解的输入输出与力分配的输入输出共用同一类型。
 *
 * @note 三个解算都由同一个 J 导出（这就是"代码互通"的落点）。设 s 为行的"值"向量
 *       （驱动行 = R_i·ω_i，侧向行 = 0）、f 为各行的力：
 *       - 逆解（位控）：ξ → 各轮 {舵角, 轮速}        LibChassisInverse       （s = J·ξ，逐轮直解）
 *       - 正解（里程计）：各轮轮速 → ξ               LibChassisForward       （J⁺·s，最小二乘）
 *       - 力分配（力控）：W → 各轮力矩/侧向力        LibChassisAllocateTorque（(J⁺)ᵀ·W，最小力范数）
 *       其中 J⁺ = (JᵀJ)⁻¹Jᵀ。**逆解与力分配不是同一套分配逻辑**（差别不是舍入误差）：
 *       - 逆解是 `s = J·ξ`：ξ 是 3 个未知量，m 个约束行各自独立给出 m 个值，逐轮直解即可，
 *         不需要任何矩阵求逆。固定轮算的 `g_i·v_i` 恰好等于"驱动行 · ξ"；舵轮更省一步——
 *         由 v_i 的方向定目标舵角（atan2 + 最近 180°）、由模长定轮速，完全不经过 J。
 *       - 力分配是 `f = (J⁺)ᵀ·W = J(JᵀJ)⁻¹·W`：输入只有 3 个分量而输出 m 个行力，m>3 时
 *         欠定（有无穷多组 f 满足 Jᵀf = W），必须补"最小力范数"才唯一，多出来的 (JᵀJ)⁻¹
 *         就是这个约束的代价：先解 3×3 方程组 q = (JᵀJ)⁻¹·W，再逐行点乘 q。
 *       两者只在"最后一步是 行 · 某个三维向量"上同形，乘的矩阵（J vs J(JᵀJ)⁻¹）与乘的
 *       对象（ξ vs q）都不同。舵轮的侧向行在力分配里对应**侧向接触力**（由舵向电机经
 *       主销偏移承受），本层只给力值，换算成舵向电机力矩所需的偏移半径归 app。
 *
 * @note 退化（回转中心重合/共线，使 JᵀJ 奇异）时正解与力分配返回 -1；
 *       逆解不做矩阵求逆，退化时仍可给出各轮分量。
 * @note 舵轮逆解输出"离当前舵角最近的等价方向"（|角差| ≤ 90°，需要时驱动反转），
 *       舵向位置环（含积分）与换向执行由 app 负责，本层不闭环。
 */

#ifndef __LIB_CHASSIS_H
#define __LIB_CHASSIS_H

#include <stdint.h>
#include "lib_math_types.h"

/*============================================
 *              规模上限
 *============================================*/
/* 内核按 n（1..MAX）轮通用，n=3 方阵、n>3 冗余由伪逆自动处理；8 轮为混用底盘留的余量 */
#define LIB_CHASSIS_MAX_WHEEL 8

/* 约束行数上限：舵轮每轮 2 行（驱动 + 侧向），其余 1 行，故最多 2·MAX_WHEEL */
#define LIB_CHASSIS_MAX_ROW (LIB_CHASSIS_MAX_WHEEL * 2)

/* 回转中心速度低于此值 [m/s] 视为"不需转向"：舵轮保持当前朝向、驱动置 0 */
#define LIB_CHASSIS_STEER_SPEED_EPS 1e-4f

/*============================================
 *              轮子
 *============================================*/
/** @brief 轮子类型：决定"当前轮面方向"从哪来 */
typedef enum : uint8_t
{
    LIB_CHASSIS_WHEEL_OMNI = 0, // 全向轮：轮面方向 = 安装角（常量），滚子角 β=π/2
    LIB_CHASSIS_WHEEL_MECANUM,  // 麦轮：轮面方向 = 安装角（常量），滚子角 β=±π/4
    LIB_CHASSIS_WHEEL_RUDDER,   // 舵轮：轮面方向 = 当前舵角（时变），滚子角 β=π/2
} LibChassisWheelKind_e;

/**
 * @brief 单个轮子的几何描述
 * @note 全部为车体系量：x 前+，y 左+，角度逆时针+
 * @note radius 只是"轮缘线速度 ↔ 角速度"的换算（R·ω = g·v），
 *       置 1 时本模块输出退化为接触点量；电机侧还要再乘/除减速比，那步归 app。
 */
typedef struct
{
    float x;            // 轮子位置（全向/麦轮为接触点、舵轮为回转中心）x (m)
    float y;            // y (m)
    float mount_angle;  // 安装角 α (rad)：轮面 e_r 的朝向；舵轮忽略（用 state.steer_angle）
    float roller_angle; // 滚子角 β (rad)：全向/舵轮 = π/2，麦轮 = ±π/4
    float radius;       // 轮子半径 R (m)，>0
    LibChassisWheelKind_e kind;
} LibChassisWheel_s;

/*============================================
 *              配置 / 状态 / 输出
 *============================================*/
/** @brief 底盘几何配置：n 个轮子（每轮几何见 LibChassisWheel_s） */
typedef struct
{
    uint8_t num;                                    // 轮数 n（1..LIB_CHASSIS_MAX_WHEEL）
    LibChassisWheel_s wheel[LIB_CHASSIS_MAX_WHEEL]; // 轮子几何
} LibChassisConfig_s;

/**
 * @brief 底盘反馈状态
 * @note steer_angle：舵轮当前舵角（车体系连续角，已含安装偏置）；固定轮忽略。
 *       无舵轮的底盘可传 NULL。
 * @note wheel_speed：各轮**当前**轮缘角速度 [rad/s]（带符号），正解用作输入；
 *       逆解/力分配不用。
 */
typedef struct
{
    float steer_angle[LIB_CHASSIS_MAX_WHEEL]; // 当前舵角 (rad)
    float wheel_speed[LIB_CHASSIS_MAX_WHEEL]; // 当前轮缘角速度 (rad/s)
} LibChassisState_s;

/**
 * @brief 逆解输出
 * @note steer_target：舵轮目标舵角（车体系连续角，"离当前最近"的等价解）；固定轮写安装角。
 * @note wheel_speed：目标轮缘角速度 [rad/s]（带符号，负=反转）。
 */
typedef struct
{
    float steer_target[LIB_CHASSIS_MAX_WHEEL]; // 目标舵角 (rad)
    float wheel_speed[LIB_CHASSIS_MAX_WHEEL];  // 目标轮缘角速度 (rad/s)
} LibChassisRef_s;

/**
 * @brief 力分配输出
 * @note wheel_torque：各轮**驱动**轮缘力矩 [N·m]，符号沿该轮驱动方向 g_i，正=正转。
 * @note lateral_force：各轮**侧向**接触力 [N]（沿 g_i^⊥，正=指向 e_r 逆时针 90° 侧）。
 *       只有舵轮非 0（侧向行对应舵向电机经主销偏移承受的力）；全向/麦轮恒 0。
 *       换算成舵向电机力矩所需的偏移半径（擦胎半径）不在本层，归 app。
 */
typedef struct
{
    float wheel_torque[LIB_CHASSIS_MAX_WHEEL];  // 驱动轮缘力矩 (N·m)
    float lateral_force[LIB_CHASSIS_MAX_WHEEL]; // 侧向接触力 (N)
} LibChassisTorque_s;

/*============================================
 *              实例
 *============================================*/
/**
 * @brief 内核实例：持有几何配置与每次解算的工作区
 * @note jacobian 的每个元素是一行（m×3，行距 = 1 个 vector3_t）；
 *       jtj_inv 为 (JᵀJ)⁻¹（lib_math 的 matrix3_t，row-major）
 * @note row_wheel/row_lateral 记录每行属于哪个轮、是否为侧向约束行，由组行时填好
 * @note valid：最近一次组装是否得到可逆的 JᵀJ。无舵轮底盘的 J 是常量，
 *       LibChassisInit 里就顺带建好；有舵轮的 J 随舵角时变，由每次解算按传入的
 *       舵角重建，此时 Init 后 valid 仍为 0，属正常。
 */
typedef struct
{
    LibChassisConfig_s cfg;                   // 几何配置（Init 时拷贝）
    vector3_t jacobian[LIB_CHASSIS_MAX_ROW];  // J，m×3，每行一个 vector3_t
    uint8_t row_wheel[LIB_CHASSIS_MAX_ROW];   // 该行对应的轮序号
    uint8_t row_lateral[LIB_CHASSIS_MAX_ROW]; // 该行是否侧向约束行（0=驱动行）
    uint8_t row_num;                          // 当前行数 m = n + 舵轮数
    matrix3_t jtj_inv;                        // (JᵀJ)⁻¹，3×3 row-major
    uint8_t valid;                            // 最近一次组装是否成功（JᵀJ 可逆）
} LibChassisInstance_s;

/*============================================
 *              外部接口
 *============================================*/

/**
 * @brief 初始化内核实例
 * @param inst 实例指针
 * @param cfg  几何配置（只读，内部拷贝）
 * @retval 0  成功
 * @retval -1 空指针 / 轮数非法
 * @retval -2 某个轮子 radius ≤ 0
 * @retval -3 某个轮子 kind 非法
 * @note 无舵轮的底盘 J 是常量，这里顺带把 J 与 (JᵀJ)⁻¹ 建好（inst->valid 置 1）；
 *       含舵轮的底盘 J 随舵角时变，留给每次解算按传入舵角重建（Init 后 valid 为 0）。
 */
int8_t LibChassisInit(LibChassisInstance_s *inst, const LibChassisConfig_s *cfg);

/**
 * @brief 逆解（位控）：底盘速度 → 各轮 {目标舵角, 目标轮速}
 * @param inst  实例指针
 * @param st    反馈状态（舵轮取 steer_angle 做"最近解"选择）
 * @param twist 底盘速度 ξ = (vx [m/s], vy [m/s], w [rad/s])
 * @param out   解算结果
 * @note 固定轮：wheel_speed = (J_i·ξ)/R_i，steer_target = 安装角
 * @note 舵轮：接触点速度方向即目标舵角（取最近等价解），wheel_speed = (±|v|)/R_i；
 *       回转中心速度 < LIB_CHASSIS_STEER_SPEED_EPS 时保持当前舵角、轮速置 0
 * @note 不依赖 JᵀJ，也不组装 J：逐轮几何直解，退化（2 轮全向等欠约束）时仍照常输出分量。
 *       与 LibChassisAllocateTorque 不是同一套分配逻辑，见文件头注释。
 */
void LibChassisInverse(LibChassisInstance_s *inst, const LibChassisState_s *st, vector3_t twist, LibChassisRef_s *out);

/**
 * @brief 正解（里程计）：各轮当前轮速 → 底盘速度（最小二乘）
 * @param inst  实例指针
 * @param st    反馈状态：舵轮用 steer_angle，所有轮用 wheel_speed 作为输入
 * @param twist 输出：估算的底盘速度 (vx, vy, w)
 * @retval 0  成功
 * @retval -1 参数非法 / JᵀJ 奇异（此时输出保持 0）
 * @note 解 (JᵀJ)·ξ = Jᵀ·s。舵轮的侧向行带上了"轮面不侧滑"前提，因此 2 轮半舵
 *       （m=4>3）也能定出 ξ；固定轮底盘 m=n，仍需 n≥3 且回转中心不共线。
 */
int8_t LibChassisForward(LibChassisInstance_s *inst, const LibChassisState_s *st, vector3_t *twist);

/**
 * @brief 力分配（力控）：底盘期望广义力 → 各轮驱动轮缘力矩 + 舵轮侧向接触力
 * @param inst   实例指针
 * @param st     反馈状态（舵轮取 steer_angle 构造时变 J）
 * @param wrench 底盘期望广义力 W = (Fx [N], Fy [N], Mz [N·m])
 * @param out    各轮驱动轮缘力矩 [N·m] 与舵轮侧向接触力 [N]
 * @retval 0  成功
 * @retval -1 参数非法 / JᵀJ 奇异（此时输出保持 0）
 * @note f = (J⁺)ᵀ·W = J·(JᵀJ)⁻¹·W（行数 m>3 时即最小**力**范数解）；驱动行
 *       τ_i = R_i·f_i，舵轮侧向行给出 lateral_force。如需按执行器能力加权/限幅，由上层再处理。
 */
int8_t LibChassisAllocateTorque(LibChassisInstance_s *inst, const LibChassisState_s *st, vector3_t wrench, LibChassisTorque_s *out);

/**
 * @brief 在 φ 与 φ+180° 两个等价轮向中，选离当前舵角最近的解
 * @param phi_des 期望轮向 [rad]（任意值，通常是 atan2 结果）
 * @param phi_cur 当前轮向 [rad]（连续值）
 * @param reverse 输出：1=选了反向解，驱动轮需反转；0=不反转。可为 NULL
 * @return 目标轮向 [rad] = phi_cur + 最近角差，|角差| ≤ 90°
 * @note 只做几何选择，不产生控制量（不闭环）
 */
float LibChassisSteerNearest180(float phi_des, float phi_cur, int8_t *reverse);

#endif // !__LIB_CHASSIS_H

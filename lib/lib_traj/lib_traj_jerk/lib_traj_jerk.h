/**
 * @file lib_traj_jerk.h
 * @brief 单轴点到点轨迹规划器：**完整任意起止** + jerk 受限（位置 3 次）
 *
 * @note 输入：起止六边界 (p,v,a) + 三个上限 (vmax, amax, jmax)；
 *       输出：一条填进 LibTraj_Traj_s 的**预计算轨迹**（全部段都是常值 jerk 的
 *       位置 3 次段，故加速度连续、jerk 分段常值 —— 正是"a 不突变、j 可突变"）。
 *       拿到轨迹后每拍 LibTrajStep 采样，目标变了就整条重新 LibTrajJerkPlan。
 *
 * @note 求解思路：把"a = 0 的顶点处的速度 v_c"当作唯一自由标量做**射击**
 *       （而不是 Biagiotti 那样按 a0/a1 分情况推导符号，分支会爆炸）。
 *       两个端点剖面各是闭式解（三段：加加速/匀加速/减加速），总位移 D(v_c)
 *       在 v_c 上**不是单调的**（v0=v1≠0 时最小值在 v_c≈v0 处），故用
 *       「粗扫 + D 极小点三元锚点 + 边界二分 + 局部细分」把 D 打到目标位移上。
 *       另有一条"先停再走"的兜底候选，两只取"可行且更短"者。
 *       完整的候选枚举与判据见 .c 文件头部注释。
 *
 * @note 时间最优性：本规划器**不保证严格时间最优**（少数边界会比理论最优慢一点点），
 *       但保证满足全部约束与终态精度；这是换"任意起止都能解"付出的代价。
 *       常规场景（两端都静止、位移正常）就是标准 7 段 S 曲线，且是时间最优的。
 *
 * @note 命名：本规划器按**被限制的导数**命名（jerk 受限 ⇒ lib_traj_jerk / LibTrajJerk*）。
 *       同族的 snap 受限规划器落在同级的 lib_traj_snap/（前缀 LibTrajSnap*），
 *       与 lib_kf 家族的 lib_lkf/lib_ekf/lib_eskf 同构；两者符号不冲突，可同时编入。
 *
 * @note 编译开关：与核心共用 LIB_TRAJ_USED（不新增宏）；PC 端测试用
 *       -DLIB_TRAJ_USED -DLIB_TRAJ_STANDALONE 跳过 app_cfg.h。
 */

#ifndef __LIB_TRAJ_JERK_H
#define __LIB_TRAJ_JERK_H

#include <stdint.h>

#include "lib_traj.h"

/*============================================
 *              状态码
 *============================================*/
typedef enum : int8_t
{
    LIB_TRAJ_JERK_PLAN_OK = 0,              /* 成功：traj 被整条重写 */
    LIB_TRAJ_JERK_PLAN_ERR_PARAM = -1,      /* 指针为空 / 极限 ≤ 0 / 边界含 NaN */
    LIB_TRAJ_JERK_PLAN_ERR_LIMIT = -2,      /* 起止 v/a 本身就超限（防御性） */
    LIB_TRAJ_JERK_PLAN_ERR_INFEASIBLE = -3, /* 给定极限内无解 */
    LIB_TRAJ_JERK_PLAN_ERR_INTERNAL = -4,   /* 内部自校验失败（不应出现；traj 被清空） */
} LibTrajJerkPlan_Status_e;

/*============================================
 *              请求
 *============================================*/
typedef struct
{
    float p0, v0, a0;       /* 起点 位置(rad) / 速度(rad/s) / 加速度(rad/s²) */
    float p1, v1, a1;       /* 终点 位置/速度/加速度 */
    float vmax, amax, jmax; /* 三个上限，均须 > 0 */
} LibTrajJerkPlan_Request_s;

/*============================================
 *              接口
 *============================================*/

/**
 * @brief 规划一条从 (p0,v0,a0) 到 (p1,v1,a1) 的 jerk 受限轨迹
 * @param traj 轨迹实例（成功时被整条重写并把运行游标归零）
 * @param req  请求（起止六边界 + 三个上限）
 * @return LibTrajJerkPlan_Status_e
 *
 * @note 失败时**不改动 traj**（保留上一版有效计划），便于实时系统直接忽略本次规划；
 *       唯一例外是 ERR_INTERNAL（内部自校验失败，理论上不出现），此时 traj 被清空。
 * @note 成功时 traj 一定**非空**（哪怕"原地不动"也会给一个零时长的段），
 *       所以调用方可以直接 LibTrajStep 拿到正确的参考位置，不必先判空。
 * @note 计算量：约 100 次闭式剖面求解（几百 flop），适合"目标变化时才调"，
 *       不要每个控制周期都调。
 */
LibTrajJerkPlan_Status_e LibTrajJerkPlan(LibTraj_Traj_s *traj, const LibTrajJerkPlan_Request_s *req);

#endif /* __LIB_TRAJ_JERK_H */

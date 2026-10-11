/**
 * @file lib_traj.h
 * @brief 通用单轴分段多项式轨迹（核心）：段构造 / 拼接 / 按时间采样 x-v-a-j
 *
 * @note 职责边界：本库只产出一条"参考轨迹"，输出 p/v/a/j；不碰 PID、不发力矩、
 *       不读编码器、不决定控制模式。位置环/速度环/前馈/限幅全在控制层。
 *
 * @note 模型：一条轨迹 = 若干"段"首尾相接。每段是位置关于**段内时间** τ 的多项式
 *           p(τ) = Σ c[k]·τ^k ,  k = 0..LIB_TRAJ_DEG_MAX(=4)
 *       于是 v/a/j 分别是 p 的一阶/二阶/三阶导数，采样时由系数逐阶求导得到，
 *       无需数值微分，也没有逐拍积分的累计误差。
 *
 * @note 次数 → 连续性（"加速度不突变、加加速度可突变"正好对应 3 次）：
 *         位置 1 次：匀速     a ≡ 0
 *         位置 2 次：匀加速   a 为常数，段间 a 阶跃
 *         位置 3 次：a 线性   → a 连续、j 分段常值（段间跳变）★ 规划器就用这一档
 *         位置 4 次：a 二次   → 用 ConstSnap 构造时 j 也连续（snap 受限）
 *       所以"要 a 不突变"用 3 次就够；4 次多出的自由度用于让 j 也连续，
 *       或者让单段多钉一个边界条件（QuarticHermite）。
 *
 * @note 生命周期（预计算模型）：Reset → 追加若干段 → 每拍 LibTrajStep(dt) 采样。
 *       目标变了就整条重新规划（见 lib_traj_jerk）。运行期采样是 O(1) 的
 *       Horner 求值；Step 借 seg_hint + seg_start 顺序推进，均摊 O(1)。
 *
 * @note 连续性归属（重要契约）：
 *       - LibTrajAppendSeg **只追加、不重算新段初值** —— 这样 LibTrajSegMakeRaw
 *         "给什么系数就是什么"的语义不被破坏，也避免掩盖调用方的 bug。
 *       - LibTrajAppendJerk **负责连续性**：它先读 LibTrajEndState() 拿到
 *         (p,v,a) 作新段初值，再构造 ConstJerk 段，**按构造即 C² 连续**。
 *       规划器正是用 AppendJerk 逐段接力，所以规划出的轨迹段间 p/v/a 连续。
 *
 * @note 与 drvlib_motor（三级级联 PID）的接法 —— 规划库只管给参考量：
 *       级联结构 ref(位置) —ANGLE PID→ 速度设定 —SPEED PID→ 电流设定 —CURRENT PID→ 电流；
 *       前馈是 LibPIDCalculate(pid, sp, meas, ff, dt) 里**加在该环输出上**的，
 *       因此量纲必须与该环**下一环的输入**一致：
 *       | 本库输出          | 接到                                        | 为什么 |
 *       |------------------|---------------------------------------------|--------|
 *       | out.position     | DrvlibMotorSetRef 的 ref（loop_type 含 ANGLE）| 位置设定值 |
 *       | out.velocity     | angle_feedforward_ptr                       | ANGLE 环输出 = SPEED 环设定值，rad/s |
 *       | out.acceleration | speed_feedforward_ptr（须先 ×J÷Kt 换成电流）  | SPEED 环输出 = 电流(A)；加速度前馈 = J·α÷Kt |
 *       注意：
 *       - 前馈指针是 float*，指向调用方持有的变量；angle_feedforward_ptr 可直接
 *         指向 out.velocity，但加速度前馈要先自己算一个
 *         `float ff_i = out.acceleration * J / Kt;` 再取址。
 *       - position_mode 用 DRVLIB_MOTOR_POS_CONTINUOUS（轨迹是多圈绝对角，不要 WRAP）。
 *       - dt 单位是**秒**（APP_TASK_DEF 给的是 ms，须 ×0.001f）。
 *       - drvlib_motor 最内环是**电流环**，要"力矩"语义由 app 自己 ÷Kt/×Kt。
 *
 * @note 编译开关：固件在 app_cfg.h 定义 LIB_TRAJ_USED（不开则 .c 是空翻译单元）。
 *       本模块只依赖 <stdint.h>/<string.h>/<math.h>，不依赖 bsp/drv/lib_math。
 *
 * @note 旧接口 LibTrajInit/Seed/SetTarget/Move/Update（每拍重算刹车曲线的梯形规划器）
 *       已随执行模型变更**整体移除**：新模型是"预计算整条轨迹 + 按时间采样"。
 */

#ifndef __LIB_TRAJ_H
#define __LIB_TRAJ_H

#include <stdint.h>

/*============================================
 *              容量与容差
 *============================================*/

#ifndef LIB_TRAJ_DEG_MAX
#define LIB_TRAJ_DEG_MAX 4 /* 位置最高次：p(τ)=Σ c[k]·τ^k，k=0..4 */
#endif

/* 段容量：jerk 受限规划器 7 段、"先停再走"兜底 13 段、以后的 snap 受限规划器 15 段，
   故取 16 留余量。RAM 紧的 app 可 -DLIB_TRAJ_SEG_MAX=7 覆盖。（注释放这里而不是行尾，
   否则 clang-format 对 #define 行尾的多行块注释会反复重排、不收敛。） */
#ifndef LIB_TRAJ_SEG_MAX
#define LIB_TRAJ_SEG_MAX 16
#endif

#define LIB_TRAJ_EPS_TIME 1e-6f  /* 段时长下限 (s)，短于此的段视为空段 */
#define LIB_TRAJ_EPS_STATE 1e-5f /* 状态比较阈值（仓库无全局 EPS，各模块自带） */

/*============================================
 *              段
 *============================================*/

/**
 * @brief 一段轨迹：p(τ) = Σ c[k]·τ^k，τ = t − 段起点 ∈ [0, duration]
 * @note c[degree+1 .. LIB_TRAJ_DEG_MAX] 恒为 0（构造函数负责清零），
 *       因此求值时不必按 degree 分支，高阶导数自然退化为 0。
 *       degree 只作自描述/调试用。
 */
typedef struct
{
    float c[LIB_TRAJ_DEG_MAX + 1]; /* 位置多项式系数，c[k] 是 τ^k 的系数 */
    float duration;                /* 段时长 T (s) */
    uint8_t degree;                /* 实际次数 0..LIB_TRAJ_DEG_MAX */
} LibTraj_Seg_s;

/*============================================
 *              轨迹实例
 *============================================*/

/**
 * @brief 计划 + 运行状态一体的实例
 * @note 分段：seg[]/seg_num/total 是"预计算的计划"（规划后运行期只读），
 *       t_now/seg_start/seg_hint 是 Step 的运行游标。
 */
typedef struct
{
    LibTraj_Seg_s seg[LIB_TRAJ_SEG_MAX]; /* 整条轨迹（规划后运行期只读） */
    uint16_t seg_num;                    /* 已追加段数 */
    float total;                         /* 总时长 (s)，缓存 */

    /* ---- 运行游标（Reset 清零；规划器不直接碰） ---- */
    float t_now;       /* Step 累计时间 */
    float seg_start;   /* seg_hint 段的起始时刻 */
    uint16_t seg_hint; /* Step 的段查找缓存 */
} LibTraj_Traj_s;

/*============================================
 *              输出（每拍设定值）
 *============================================*/

typedef struct
{
    float position;     /* x  (rad)    */
    float velocity;     /* v  (rad/s)  */
    float acceleration; /* a  (rad/s²) */
    float jerk;         /* j  (rad/s³) */
    uint8_t finished;   /* 1 = 采样点已到/超过总时长（空轨迹恒为 1） */
} LibTraj_Output_s;

/**
 * @brief 定义轨迹实例
 * @example LIB_TRAJ_INSTANCE_DEF(feeder_traj);
 */
#define LIB_TRAJ_INSTANCE_DEF(name) static LibTraj_Traj_s name = {0}

/*============================================
 *              计划管理
 *============================================*/

/**
 * @brief 清空轨迹（段数/总时长/运行游标全部归零）
 */
void LibTrajReset(LibTraj_Traj_s *traj);

/**
 * @brief 是否为空轨迹
 * @return 1 = 空（无段），0 = 非空
 */
uint8_t LibTrajIsEmpty(const LibTraj_Traj_s *traj);

/**
 * @brief 追加一段已构造好的段（**只追加，不重算该段初值**，见头文件契约）
 * @param traj 轨迹
 * @param seg  段（须已用 LibTrajSegMake* 构造）
 * @return 0 成功；-1 失败（指针为空 / 段已满 / 段时长非法）
 */
int8_t LibTrajAppendSeg(LibTraj_Traj_s *traj, const LibTraj_Seg_s *seg);

/**
 * @brief 追加一段"常值 jerk"段，初值取当前轨迹末端状态（**按构造即 p/v/a 连续**）
 * @param traj     轨迹
 * @param jerk     jerk (rad/s³)
 * @param duration 段时长 (s)；< LIB_TRAJ_EPS_TIME 时视为空段直接跳过
 * @return 0 成功（含跳过空段）；-1 失败（traj 为空指针 / 段已满）
 * @note 空轨迹时以 (0,0,0) 为初值 —— 需要真实初值时请先 LibTrajAppendSeg 一个
 *       用 LibTrajSegMakeConstJerk(p0,v0,a0,...) 构造的首段（规划器就是这么做的）。
 */
int8_t LibTrajAppendJerk(LibTraj_Traj_s *traj, float jerk, float duration);

/**
 * @brief 取轨迹起点状态（p/v/a/j）。空轨迹输出全 0、finished=1
 */
void LibTrajStartState(const LibTraj_Traj_s *traj, LibTraj_Output_s *out);

/**
 * @brief 取轨迹终点状态（p/v/a/j）。空轨迹输出全 0、finished=1
 */
void LibTrajEndState(const LibTraj_Traj_s *traj, LibTraj_Output_s *out);

/*============================================
 *              段构造
 *============================================*/

/**
 * @brief 常值 jerk 段（前向 Taylor）：c0=p0, c1=v0, c2=a0/2, c3=jerk/6
 * @param seg      输出段
 * @param p0,v0,a0 段起点状态 (rad, rad/s, rad/s²)
 * @param jerk     段内常值 jerk (rad/s³)
 * @param duration 段时长 (s)
 * @note 位置 3 次、a 线性。因为初值就是调用方给的段起点状态，所以逐段首尾相接时
 *       **p/v/a 天然连续**（这正是规划器与 LibTrajAppendJerk 的基础）。
 */
void LibTrajSegMakeConstJerk(LibTraj_Seg_s *seg, float p0, float v0, float a0, float jerk, float duration);

/**
 * @brief 常值 snap 段（位置 4 次）：c0=p0, c1=v0, c2=a0/2, c3=j0/6, c4=snap/24
 * @param seg           输出段
 * @param p0,v0,a0      段起点状态
 * @param j0            段起点 jerk (rad/s³)
 * @param snap          段内常值 snap (rad/s⁴)
 * @param duration      段时长 (s)
 * @note 位置 4 次、a 二次、j 线性 —— 逐段相接时 p/v/a/j 全连续（snap 受限规划器的素材）。
 */
void LibTrajSegMakeConstSnap(LibTraj_Seg_s *seg, float p0, float v0, float a0, float j0, float snap, float duration);

/**
 * @brief 三次 Hermite 段：钉住两端位置与速度（位置 3 次，a 由边界条件决定）
 * @param seg      输出段
 * @param p0,v0    起点 位置/速度
 * @param p1,v1    终点 位置/速度
 * @param duration 段时长 h (s)，必须 > 0（否则段非法，系数会溢出）
 * @note dp = p1−p0−v0·h，dv = v1−v0；c2=(3dp−h·dv)/h²，c3=(h·dv−2dp)/h³。
 */
void LibTrajSegMakeCubicHermite(LibTraj_Seg_s *seg, float p0, float v0, float p1, float v1, float duration);

/**
 * @brief 四次 Hermite 段：钉住起点 p/v/a 与终点 p/v（位置 4 次，**终点 a 不受约束**）
 * @param seg      输出段
 * @param p0,v0,a0 起点 位置/速度/加速度
 * @param p1,v1    终点 位置/速度
 * @param duration 段时长 h (s)，必须 > 0
 * @note dp = p1−p0−v0·h−(a0/2)h²，dv = v1−v0−a0·h；
 *       c3=(4dp−h·dv)/h³，c4=(h·dv−3dp)/h⁴。终点加速度是自由量（四次的能力边界）。
 */
void LibTrajSegMakeQuarticHermite(LibTraj_Seg_s *seg, float p0, float v0, float a0, float p1, float v1, float duration);

/**
 * @brief 直接给系数构造段（万能入口；c[0..degree] 被拷入，更高阶清零）
 * @param seg      输出段
 * @param coeff    系数数组，至少 degree+1 项
 * @param degree   次数，须 ≤ LIB_TRAJ_DEG_MAX
 * @param duration 段时长 (s)，须 ≥ 0
 * @return 0 成功；-1 非法（指针为空 / degree 超限 / duration 为负或 NaN）
 */
int8_t LibTrajSegMakeRaw(LibTraj_Seg_s *seg, const float *coeff, uint8_t degree, float duration);

/*============================================
 *              采样
 *============================================*/

/**
 * @brief 段内求值：给定段内时间 τ，算 p/v/a/j
 * @param seg 段（空指针 → 输出全 0、finished=1）
 * @param tau 段内时间 (s)，会被钳到 [0, duration]
 * @param out 输出
 */
void LibTrajSegEval(const LibTraj_Seg_s *seg, float tau, LibTraj_Output_s *out);

/**
 * @brief 轨迹求值（**纯函数，不改实例状态**）：给定绝对时间 t 算 p/v/a/j
 * @param traj 轨迹（空 ⇒ 输出全 0、finished=1）
 * @param t    绝对时间 (s)。t<0 按 0 取；t ≥ total 按 total 取（finished=1）
 * @param out  输出
 * @note 段查找是从头线性累加定位（≤ LIB_TRAJ_SEG_MAX 次比较），保证 const 正确性。
 */
void LibTrajEval(const LibTraj_Traj_s *traj, float t, LibTraj_Output_s *out);

/**
 * @brief 推进一拍并采样（每拍调用）：t_now += dt 后求值
 * @param traj 轨迹（空指针 ⇒ 输出全 0、finished=1）
 * @param dt   步长 (s)，负值按 0 取
 * @param out  输出
 * @note 与 LibTrajEval 是**两条独立代码路径**（这里走 seg_hint 顺序推进），
 *       同一时刻的结果应一致（PC 端测试会逐点比对）。
 */
void LibTrajStep(LibTraj_Traj_s *traj, float dt, LibTraj_Output_s *out);

#endif /* __LIB_TRAJ_H */

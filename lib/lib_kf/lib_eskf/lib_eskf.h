/**
 * @file lib_eskf.h
 * @brief 通用误差状态卡尔曼滤波(ESKF)库：名义状态外置，滤波器内只跑误差态
 *
 * @note 纯算法模块，无硬件 / 时间依赖、无日志、无动态内存；使用单精度 float
 * @note 运行期任意 n 维误差态 / m 维量测 / 可选 l 维控制输入，矩阵与工作区由
 *       ESKF_INSTANCE_DEF 按上限维数静态分配
 * @note 协方差代数复用 lib_kf_core.h（与 lib_lkf / lib_ekf 同一份实现）
 * @note dt 由调用方原样透传给回调，本模块不感知时间
 *
 * ═══════════════════ 为什么要有 ESKF（与 lib_ekf 的区别） ═══════════════════
 * 单位四元数这类状态**不能用减法修正**（q ← q + Δ 毫无意义，且归一化会破坏协方差
 * 的线性性）。ESKF 的做法是把状态拆成两层：
 *   - **名义状态(nominal)**：非线性、大范围、由用户持有并传播（通常在 ctx 里）。
 *     例如四元数 q_nom、零偏 b_nom。它**不在滤波器内部**，也不需要协方差描述。
 *   - **误差状态(error/δ)**：小量、线性、进滤波器。例如 δθ、δb。
 *   真值由 δ 修正名义值得到： x_true = x_nom ⊞ δx
 *   滤波器只估 δx（小量 ⇒ 线性化精度高、协方差小、数值条件好），估出来后**注入**
 *   (inject) 名义状态并清零 δx，如此往复。
 * 本模块就是这套骨架；具体"⊞ 怎么加、F/H 怎么算"全部由用户回调给出。
 *
 * ═══════════════════ 标准调用时序（硬约束） ═══════════════════
 * @code
 *     Predict(δ 必须为 0) → Update(δ 变非 0) → Inject(δ 清 0) → Predict → …
 * @endcode
 * - Predict / Update 入口检查 δ 必须为 0，否则返回 ESKF_ERR_CFG；
 * - Update 成功后才置位"有未注入的 δ"，故**每次 Update 后必须紧跟一次 Inject**；
 * - Inject 幂等：没有待注入的 δ 时直接返回 ESKF_OK，不重复动 P。
 *
 * @note 四元数误差约定见各回调注释处：本库不规定 q 是机体系→世界系还是反过来，
 *       但要求用户全文自洽。仓库内 `drvlib_bmi088_ist8310_eskf` 采用
 *       q: 机体系→世界系、δq 右乘（q_true = q_nom ⊗ δq，δθ 在机体系）——这是
 *       姿态 ESKF 最常用的约定，其线性化结论为：
 *         F_c = [[ -[ω]× , -I₃ ],[ 0₃ , 0₃ ]]    （ω = 陀螺标定值 - b_nom）
 *         H   = [ [h]× , 0₃ ]                    （h = R(q_nom)ᵀ·参考方向）
 *
 * @note 使用示例（一维位置 + 速度，名义值放在 ctx 里；此处只是骨架演示）：
 * @code
 *     typedef struct { float p, v; } NavCtx;
 *
 *     static void propagate(void *ctx, const float *u, float dt, float *F, float *Q)
 *     {
 *         NavCtx *c = ctx; (void)u;
 *         c->p += c->v * dt;                       // 名义状态传播（非线性由用户负责）
 *         F[0]=1; F[1]=dt; F[2]=0; F[3]=1;
 *         Q[0]=1e-6f*dt; Q[1]=0; Q[2]=0; Q[3]=1e-4f*dt;
 *     }
 *     static void measure(void *ctx, const float *z, int m_now, float *y, float *H, float *R)
 *     {
 *         NavCtx *c = ctx; (void)m_now;
 *         y[0] = z[0] - c->p;                      // 残差 = 量测 - h(名义)
 *         H[0] = 1.0f; H[1] = 0.0f;
 *         R[0] = 1e-4f;
 *     }
 *     static void inject(void *ctx, const float *dx)
 *     {
 *         NavCtx *c = ctx;
 *         c->p += dx[0]; c->v += dx[1];            // δx 注入名义状态
 *     }
 *
 *     ESKF_INSTANCE_DEF(nav, 2, 1, 0);
 *     Eskf_Init_Config_s cfg = { .n = 2, .m = 1, .ctx = &nav_ctx,
 *                                .propagate = propagate, .measure = measure, .inject = inject };
 *     EskfInit(&nav, &cfg);
 *     EskfPredict(&nav, NULL, 0.01f);
 *     if (EskfUpdate(&nav, z) == ESKF_OK) { EskfInject(&nav); }
 * @endcode
 */

#ifndef __LIB_ESKF_H
#define __LIB_ESKF_H

#include <stddef.h>
#include <stdint.h>

/*============================ 选项 / 返回状态 ============================*/

/**
 * @brief ESKF 选项位标志（写入 EskfInstance.opt）
 */
typedef enum
{
    ESKF_OPT_JOSEPH = (1u << 0), /* 置位=使用 Joseph 协方差更新（数值最稳）；
                                    未置位=经典式 P=(I-KH)·P            */
} Eskf_Opt_e;

/**
 * @brief ESKF 返回状态
 */
typedef enum
{
    ESKF_OK = 0,       /* 成功                                            */
    ESKF_ERR_NULL,     /* 传入空指针                                      */
    ESKF_ERR_DIM,      /* 维度非法（n/m/l 越界或为 0 等）                  */
    ESKF_ERR_CFG,      /* 回调缺失 / 调用时序违规（如 Update 后未 Inject）  */
    ESKF_ERR_SINGULAR, /* S 矩阵奇异，本次量测更新被跳过（δ/P 未改动）      */
} Eskf_Status_e;

/*============================ 用户回调 ============================*/

/**
 * @brief 传播名义状态并给出误差态线性化：x_nom ← f(x_nom, u, dt)，同时写出 F、Q
 * @param ctx 上下文指针（名义状态通常就放在 ctx 里，由本回调就地更新）
 * @param u   控制输入（长度 l；未用控制输入时为 NULL）
 * @param dt  距上次预测的时间间隔 [s]
 * @param F   输出：n×n 误差态状态转移矩阵（行主序紧凑，行距 n）
 * @param Q   输出：n×n 过程噪声协方差（行主序紧凑，行距 n）
 *
 * @note 本回调**只负责名义状态与 F/Q**；滤波器的 δ 恒为 0（入口已校验），
 *       故 F 只需在名义值处线性化
 * @note 姿态场景常用形式（δθ 在机体系、δq 右乘）：
 *       F = I₆ + [[ -[ω]× , -I₃ ],[ 0₃ , 0₃ ]]·dt，Q = diag(q_att·dt·I₃, q_bias·dt·I₃)
 * @note δ 的量纲由用户定（角误差用 rad、零偏用 rad/s 等），本库不做任何假设
 */
typedef void (*Eskf_Propagate_fn)(void *ctx, const float *u, float dt, float *F, float *Q);

/**
 * @brief 量测：给出残差 y、量测雅可比 H、量测噪声 R（**必填**）
 * @param ctx   上下文指针（读名义状态算 h）
 * @param z     本次量测（长度 m_now）
 * @param m_now 本次使用的量测通道数（1 ~ m_max）
 * @param y     输出：残差 y = z - h(x_nom)（长度 m_now）
 * @param H     输出：m_now×n 量测雅可比（只写前 m_now 行，**行距恒为 n**）
 * @param R     输出：m_now×m_now 量测噪声协方差（**紧凑，行距 m_now**）
 *
 * @note 残差**必须**写成 z - h(名义)，不是 h - z：ESKF 的 δx 定义为
 *       x_true = x_nom ⊞ δx，故 H·δx = h(x_true) - h(x_nom)，y = z - h(x_nom) = H·δx + v
 * @note 姿态场景（归一化方向量测）：h = R(q_nom)ᵀ·ĝ_w，H = [ [h]× , 0₃ ]
 * @note 本回调内自行做门限/拒帧判断：拒帧时可直接令 R 极大（等价于跳过）或由调用方
 *       决定本次不调用 EskfUpdateM
 */
typedef void (*Eskf_Measure_fn)(void *ctx, const float *z, int m_now, float *y, float *H, float *R);

/**
 * @brief 把误差态注入名义状态：x_nom ← x_nom ⊞ δx（**必填**）
 * @param ctx   上下文指针
 * @param delta 本次要注入的 δx（长度 n，只读）
 *
 * @note 姿态场景：δq = normalize([1, δθ/2])（δθ 在机体系）、q_nom ← normalize(q_nom ⊗ δq)、
 *       b_nom += δb。**δq 的左/右乘必须与 Measure 的 H 符号、Propagate 的 F 一致**
 * @note 仅在本回调中修改名义状态；本库其他任何地方都不触碰名义值
 */
typedef void (*Eskf_Inject_fn)(void *ctx, const float *delta);

/**
 * @brief 注入后对 P 做重置的雅可比 G（可选，NULL 表示 G = I₃⊕…即不重置）
 * @param ctx   上下文指针
 * @param delta 刚注入的 δx（只读）
 * @param G     输出：n×n 重置雅可比（行主序紧凑，行距 n）
 *
 * @note 注入后 P ← G·P·Gᵀ。理论上 G ≠ I（源于注入用精确四元数、而误差态是线性
 *       近似所引入的二阶项）；姿态场景取 G = diag(I₃ - ½[δθ]× , I₃)。
 *       正常工况 δθ<0.01 rad 且 P 姿态块近各向同性时，G=I 的量化影响 <1e-3，
 *       故本参数留空（NULL）即可，需要严格一致时再挂上
 */
typedef void (*Eskf_ResetJac_fn)(void *ctx, const float *delta, float *G);

/*============================ 配置结构体 ============================*/

/**
 * @brief ESKF 初始化配置结构体
 *
 * @note propagate / measure / inject **三者必须给出**，缺一返回 ESKF_ERR_CFG
 * @note P0 为 δx 的初始协方差（n×n）；δx 初值恒为 0（误差态无"初值"概念），
 *       故没有 x0 字段——名义状态的初值请在 Init 之前直接写进 ctx
 */
typedef struct
{
    uint8_t n;   /* 误差态维数 (1~n_max)                        */
    uint8_t m;   /* 量测维数 (1~m_max)                          */
    uint8_t l;   /* 控制输入维数 (0~l_max, 0=无控制)             */
    uint8_t opt; /* Eskf_Opt_e 位标志                           */
    void *ctx;   /* 上下文指针，原样回传给各回调                  */

    Eskf_Propagate_fn propagate; /* 名义状态传播 + F/Q（必填）      */
    Eskf_Measure_fn measure;     /* 残差 + H + R（必填）           */
    Eskf_Inject_fn inject;       /* δx 注入名义状态（必填）         */
    Eskf_ResetJac_fn reset_jac;  /* 注入后 P 重置雅可比（可空→G=I）  */

    const float *P0; /* δx 协方差初值 n*n，NULL→单位阵              */
    const float *Q;  /* 过程噪声初值 n*n（propagate 每步会重写），NULL→全零 */
    const float *R;  /* 量测噪声初值 m*m（measure 每次会重写），NULL→全零   */
} Eskf_Init_Config_s;

/*============================ 实例结构体 ============================*/

/**
 * @brief ESKF 实例结构体
 *
 * @note 所有缓冲区（矩阵 + 工作区）由 ESKF_INSTANCE_DEF 按上限维数静态分配并
 *       通过指针绑定到本实例，使用期不得 re-init / 换绑
 * @note δ 与 P 公开可读写，便于调试（RTT/IDE 观察窗口）与外部监视
 */
typedef struct EskfInstance
{
    /* 活跃维数（运行时 ≤ 预留上限） */
    uint8_t n;   /* 误差态维数                                */
    uint8_t m;   /* 量测维数                                  */
    uint8_t l;   /* 控制输入维数                               */
    uint8_t opt; /* Eskf_Opt_e 位标志                         */

    /* 预留上限（编译期由 ESKF_INSTANCE_DEF 写定，const 只读，供越界校验） */
    const uint8_t n_max;
    const uint8_t m_max;
    const uint8_t l_max;

    void *ctx;                   /* 上下文指针                  */
    Eskf_Propagate_fn propagate; /* 名义状态传播回调             */
    Eskf_Measure_fn measure;     /* 量测回调                    */
    Eskf_Inject_fn inject;       /* 注入回调                    */
    Eskf_ResetJac_fn reset_jac;  /* P 重置雅可比回调（可空）      */

    /* 状态 / 模型矩阵（行主序、按活跃维紧凑，由宏绑定） */
    float *delta; /* n   δx 误差态                              */
    float *P;     /* n*n δx 协方差                              */
    float *P0;    /* n*n δx 协方差初值（供 EskfReset 复原）        */
    float *F;     /* n*n 误差态状态转移矩阵（propagate 写）         */
    float *Q;     /* n*n 过程噪声（propagate 写）                  */
    float *H;     /* m*n 量测雅可比（measure 写，行距 n）           */
    float *R;     /* m*m 量测噪声（measure 写，紧凑行距 m_now）      */

    /* 内部工作区（由宏绑定，勿直接使用） */
    float *G;  /* n*n 重置雅可比工作区              */
    float *wA; /* n*n */
    float *wB; /* n*n */
    float *wC; /* n*m */
    float *wG; /* n*m */
    float *wD; /* 2*(m*m)，[S|I] 增广求逆用 */
    float *wF; /* m  （量测残差 y） */

    uint8_t delta_dirty; /* 1 = 已有未注入的 δ，禁止 Predict/Update */
} EskfInstance;

/*============================ 实例声明宏 ============================*/

/**
 * @brief 定义并静态连接一个 ESKF 实例
 * @param name 实例变量名
 * @param N    误差态维上限（整数常量表达式，≥1）
 * @param M    量测维上限（整数常量表达式，≥1）
 * @param L    控制输入维上限（整数常量表达式，0 表示无控制）
 *
 * @note RAM 占用 ≈ (7N² + 3NM + 3M² + N + M) floats
 *       （n=6、m=3、l=0 时 336 floats ≈ 1.31 KB）
 * @note 例：ESKF_INSTANCE_DEF(eskf_att, 6, 3, 0);
 */
#define ESKF_INSTANCE_DEF(name, N, M, L)                                                                                                                       \
    static float name##_delta[N];                                                                                                                              \
    static float name##_P[(N) * (N)];                                                                                                                          \
    static float name##_P0[(N) * (N)];                                                                                                                         \
    static float name##_F[(N) * (N)];                                                                                                                          \
    static float name##_Q[(N) * (N)];                                                                                                                          \
    static float name##_H[(M) ? (M) * (N) : 1];                                                                                                                \
    static float name##_R[(M) ? (M) * (M) : 1];                                                                                                                \
    static float name##_G[(N) * (N)];                                                                                                                          \
    static float name##_wA[(N) * (N)];                                                                                                                         \
    static float name##_wB[(N) * (N)];                                                                                                                         \
    static float name##_wC[(M) ? (N) * (M) : 1];                                                                                                               \
    static float name##_wG[(M) ? (N) * (M) : 1];                                                                                                               \
    static float name##_wD[(M) ? 2 * (M) * (M) : 1];                                                                                                           \
    static float name##_wF[(M) ? (M) : 1];                                                                                                                     \
    static EskfInstance name = {.n_max = (N),                                                                                                                  \
                                .m_max = (M),                                                                                                                  \
                                .l_max = (L),                                                                                                                  \
                                .delta = name##_delta,                                                                                                         \
                                .P = name##_P,                                                                                                                 \
                                .P0 = name##_P0,                                                                                                               \
                                .F = name##_F,                                                                                                                 \
                                .Q = name##_Q,                                                                                                                 \
                                .H = name##_H,                                                                                                                 \
                                .R = name##_R,                                                                                                                 \
                                .G = name##_G,                                                                                                                 \
                                .wA = name##_wA,                                                                                                               \
                                .wB = name##_wB,                                                                                                               \
                                .wC = name##_wC,                                                                                                               \
                                .wG = name##_wG,                                                                                                               \
                                .wD = name##_wD,                                                                                                               \
                                .wF = name##_wF}

/*============================ 矩阵元素索引宏 ============================*/

/* 以活跃维为行距直读/改写实例矩阵元素（行主序） */
#define ESKF_DELTA(eskf, i) ((eskf)->delta[(i)])              /* δx(i)        */
#define ESKF_P(eskf, i, j) ((eskf)->P[(i) * (eskf)->n + (j)]) /* P(i,j) n×n   */
#define ESKF_F(eskf, i, j) ((eskf)->F[(i) * (eskf)->n + (j)]) /* F(i,j) n×n   */
#define ESKF_Q(eskf, i, j) ((eskf)->Q[(i) * (eskf)->n + (j)]) /* Q(i,j) n×n   */
#define ESKF_H(eskf, i, j) ((eskf)->H[(i) * (eskf)->n + (j)]) /* H(i,j) m×n   */
#define ESKF_R(eskf, i, j) ((eskf)->R[(i) * (eskf)->m + (j)]) /* R(i,j) m×m   */

/*============================ 公开接口声明 ============================*/

/**
 * @brief 初始化 ESKF
 * @param eskf 实例指针（ESKF_INSTANCE_DEF 声明）
 * @param cfg  初始化配置结构体指针
 * @return 状态码（ESKF_OK / ESKF_ERR_NULL / ESKF_ERR_DIM / ESKF_ERR_CFG）
 *
 * @note 校验维度 ≤ 预留上限并写入活跃维；装载 P0/Q/R 与四个回调；δ 置 0、清除脏标志
 * @note 名义状态初值请在调用本函数**之前**直接写进 ctx（本库不持有名义状态）
 */
Eskf_Status_e EskfInit(EskfInstance *eskf, const Eskf_Init_Config_s *cfg);

/**
 * @brief 重置滤波器：δ = 0、P = P0、清除脏标志（**不动名义状态**）
 * @param eskf 实例指针
 * @return 状态码
 *
 * @note 用于上电播种后重新起滤；名义状态仍需调用方自行设置（如重播四元数）
 */
Eskf_Status_e EskfReset(EskfInstance *eskf);

/**
 * @brief 时间更新（预测）：传播名义状态并 P ← F·P·Fᵀ + Q
 * @param eskf 实例指针
 * @param u    控制输入向量（长度 l），无控制 / 不需要时传 NULL
 * @param dt   时间间隔 [s]，原样透传给 propagate 回调
 * @return 状态码（δ 非 0 时返回 ESKF_ERR_CFG）
 *
 * @note 名义状态的传播在 propagate 回调内完成，本函数不触碰名义状态
 */
Eskf_Status_e EskfPredict(EskfInstance *eskf, const float *u, float dt);

/**
 * @brief 量测更新（校正），使用实例活跃量测维 m = eskf->m
 * @param eskf 实例指针
 * @param z    量测向量（长度 m）
 * @return 状态码（ESKF_ERR_SINGULAR 表示 S 奇异，本次被跳过，δ/P 未改动）
 *
 * @note 等价于 EskfUpdateM(eskf, eskf->m, z, NULL)；**成功后必须调用 EskfInject**
 */
Eskf_Status_e EskfUpdate(EskfInstance *eskf, const float *z);

/**
 * @brief 量测更新（通用形式，支持多速率 / 部分量测 / 同维不同量测源）
 * @param eskf    实例指针
 * @param m_now   本次使用的量测通道数（1 ~ eskf->m_max）
 * @param z       本次量测向量（长度 m_now）
 * @param override 本次临时使用的量测回调（NULL = 用注册时的 eskf->measure）
 * @return 状态码
 *
 * @note `override` 用于"同一次滤波、多个同维量测源"（如加速度计与磁力计都是 3 维：
 *       注册时给 measure_acc，磁量测时传 measure_mag），无需为每种源建实例
 * @note **成功后必须调用 EskfInject** 才能继续 Predict/Update
 */
Eskf_Status_e EskfUpdateM(EskfInstance *eskf, uint8_t m_now, const float *z, Eskf_Measure_fn override);

/**
 * @brief 把 δx 注入名义状态并清零：inject(ctx, δ) → P ← G·P·Gᵀ（G 由 reset_jac 给，
 *        缺省为单位阵）→ δ = 0、清除脏标志
 * @param eskf 实例指针
 * @return 状态码
 *
 * @note 幂等：没有待注入的 δ 时直接返回 ESKF_OK，不重复动 P
 * @note 无需在量测更新失败（ESKF_ERR_SINGULAR）时调用：失败时 δ 保持 0
 */
Eskf_Status_e EskfInject(EskfInstance *eskf);

#endif /* __LIB_ESKF_H */

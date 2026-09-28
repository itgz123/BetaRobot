/**
 * @file lib_ekf.h
 * @brief 通用非线性扩展卡尔曼滤波(EKF)库：状态内置，f/F/h/H 由回调提供
 *
 * @note 纯算法模块，无硬件 / 时间依赖、无日志、无动态内存；使用单精度 float
 * @note 运行期任意 n 维状态 / m 维量测 / 可选 l 维控制输入，矩阵与工作区由
 *       EKF_INSTANCE_DEF 按上限维数静态分配
 * @note 协方差代数复用 lib_kf_core.h（与 lib_lkf / lib_eskf 同一份实现），
 *       本模块只负责"调用回调算 x⁺ / F / z_pred / H，再算残差 y = z - h(x)"
 * @note dt 由调用方原样透传给回调（f/F 内自行折进状态转移与过程噪声），
 *       本模块不感知时间
 *
 * 算法流程（非线性 EKF）：
 *   时间更新（预测）： x⁺ = f(x, u, dt)              （f 为 NULL 时退化为 F·x + B·u）
 *                      F  = ∂f/∂x |x                   （F 回调为 NULL 时用实例内存储阵）
 *                      P  = F·P·Fᵀ + Q
 *   量测更新（校正）： z_pred = h(x)                  （h 回调必填）
 *                      H  = ∂h/∂x |x                  （H 回调为 NULL 时用实例内存储阵）
 *                      y  = z - z_pred
 *                      S  = H·P·Hᵀ + R
 *                      K  = P·Hᵀ·S⁻¹
 *                      x  = x + K·y
 *                      P  = (I - K·H)·P            （经典式）
 *                      P  = (I-KH)·P·(I-KH)ᵀ + K·R·Kᵀ （Joseph 式，置 EKF_OPT_JOSEPH）
 *   协方差更新后均做对称化以抑制长期数值漂移。
 *
 * @note 与 lib_lkf 的分工：能写成 x⁺ = F·x、z_pred = H·x 的线性系统直接用 lib_lkf；
 *       状态传播或量测映射为非线性的用本模块。二者可混用（同一工程内并存）
 * @note 与 lib_eskf 的分工：本模块的**误差 = 状态本身**（状态直接进滤波器）；
 *       若状态含单位四元数等"不能用减法修正"的量，应用 lib_eskf（误差状态在
 *       滤波器内、名义状态在滤波器外）
 *
 * @note 使用示例（2D 雷达测距测角，状态 x=[px, py, vx, vy]）：
 * @code
 *     static void radar_f(void *ctx, const float *x, const float *u, float dt, float *x_next)
 *     {
 *         (void)ctx; (void)u;
 *         x_next[0] = x[0] + x[2] * dt;
 *         x_next[1] = x[1] + x[3] * dt;
 *         x_next[2] = x[2];
 *         x_next[3] = x[3];
 *     }
 *     static void radar_h(void *ctx, const float *x, int m_now, float *z_pred)
 *     {
 *         (void)ctx; (void)m_now;
 *         z_pred[0] = sqrtf(x[0]*x[0] + x[1]*x[1]);          // range
 *         z_pred[1] = atan2f(x[1], x[0]);                     // bearing
 *     }
 *
 *     EKF_INSTANCE_DEF(radar, 4, 2, 0);
 *     float Q[16] = {...}, R[4] = {...}, P0[16] = {...};
 *     Ekf_Init_Config_s cfg = {
 *         .n = 4, .m = 2, .l = 0, .opt = EKF_OPT_JOSEPH,
 *         .f_fn = radar_f, .h_fn = radar_h,      // F/H 回调留 NULL：线性化由 H 回调缺省
 *         .Q = Q, .R = R, .P0 = P0,
 *     };
 *     EkfInit(&radar, &cfg);
 *     EkfPredict(&radar, NULL, 0.01f);
 *     EkfUpdate(&radar, z);
 * @endcode
 */

#ifndef __LIB_EKF_H
#define __LIB_EKF_H

#include <stddef.h>
#include <stdint.h>

/*============================ 选项 / 返回状态 ============================*/

/**
 * @brief EKF 选项位标志（写入 EkfInstance.opt）
 */
typedef enum
{
    EKF_OPT_JOSEPH = (1u << 0), /* 置位=使用 Joseph 协方差更新（数值最稳）；
                                   未置位=经典式 P=(I-KH)·P            */
} Ekf_Opt_e;

/**
 * @brief EKF 返回状态
 */
typedef enum
{
    EKF_OK = 0,       /* 成功                                           */
    EKF_ERR_NULL,     /* 传入空指针                                     */
    EKF_ERR_DIM,      /* 维度非法（n/m/l 越界或为 0 等）                 */
    EKF_ERR_CFG,      /* 回调缺失（量测更新必须有 h_fn）                 */
    EKF_ERR_SINGULAR, /* S 矩阵奇异，本次量测更新被跳过（x/P 未改动）     */
} Ekf_Status_e;

/*============================ 用户回调 ============================*/

/**
 * @brief 状态传播 x⁺ = f(x, u, dt)（非线性模型；为 NULL 时退化为线性 x⁺ = F·x + B·u）
 * @param ctx    注册时给定的上下文指针（原样回传，可为 NULL）
 * @param x      当前状态（长度 n，只读）
 * @param u      控制输入（长度 l；未用控制输入时为 NULL）
 * @param dt     距上次预测的时间间隔 [s]（由调用方给，可能为 0）
 * @param x_next 输出：传播后的状态（长度 n，**不得与 x 别名**）
 */
typedef void (*Ekf_f_fn)(void *ctx, const float *x, const float *u, float dt, float *x_next);

/**
 * @brief 状态雅可比 F = ∂f/∂x（为 NULL 时使用实例内由配置装载的常值 F 矩阵）
 * @param ctx 上下文指针
 * @param x   线性化点：**传播前的状态** x_{k-1}（长度 n，只读）
 * @param u   控制输入（长度 l；可为 NULL）
 * @param dt  时间间隔 [s]
 * @param F   输出：n×n 行主序紧凑矩阵（行距 n）
 *
 * @note 本回调在状态传播**之前**被调用，故 x 是上一时刻的估计，符合 EKF 惯例
 */
typedef void (*Ekf_F_fn)(void *ctx, const float *x, const float *u, float dt, float *F);

/**
 * @brief 量测预测 z_pred = h(x)（**必填**）
 * @param ctx    上下文指针
 * @param x      当前状态（长度 n，只读）
 * @param m_now  本次使用的量测通道数（1 ~ m_max）
 * @param z_pred 输出：预测量测（长度 m_now）
 */
typedef void (*Ekf_h_fn)(void *ctx, const float *x, int m_now, float *z_pred);

/**
 * @brief 量测雅可比 H = ∂h/∂x（为 NULL 时使用实例内由配置装载的常值 H 矩阵）
 * @param ctx   上下文指针
 * @param x     线性化点：当前状态（长度 n，只读）
 * @param m_now 本次使用的量测通道数
 * @param H     输出：m_now×n 行主序紧凑矩阵（**行距为实例的 n**，只需写前 m_now 行）
 */
typedef void (*Ekf_H_fn)(void *ctx, const float *x, int m_now, float *H);

/*============================ 配置结构体 ============================*/

/**
 * @brief EKF 初始化配置结构体
 *
 * @note 所有矩阵均行主序、按活跃维紧凑排列（F/Q/P 为 n×n，H 为 m×n，
 *       R 为 m×m，B 为 n×l）；传 NULL 的模型矩阵将被清零，P0 默认单位阵，
 *       x0 默认全零
 * @note 回调可只填需要的：纯线性系统给 f_fn=NULL、F_fn=NULL、H_fn=NULL
 *       （等价于 lib_lkf）；只非线性传播则给 f_fn/F_fn；只非线性量测则给 h_fn/H_fn
 * @note h_fn 为 NULL 时 EkfUpdate 返回 EKF_ERR_CFG
 */
typedef struct
{
    uint8_t n;   /* 状态维数 (1~n_max)                          */
    uint8_t m;   /* 量测维数 (1~m_max)                          */
    uint8_t l;   /* 控制输入维数 (0~l_max, 0=无控制)             */
    uint8_t opt; /* Ekf_Opt_e 位标志                            */
    void *ctx;   /* 上下文指针，原样回传给各回调                  */

    Ekf_f_fn f_fn; /* 状态传播（NULL→线性 F·x+B·u）               */
    Ekf_F_fn F_fn; /* 状态雅可比（NULL→用实例常值 F 阵）           */
    Ekf_h_fn h_fn; /* 量测预测（必填）                            */
    Ekf_H_fn H_fn; /* 量测雅可比（NULL→用实例常值 H 阵）           */

    const float *x0; /* 状态初值 n，NULL→全零                        */
    const float *P0; /* 协方差初值 n*n，NULL→单位阵                  */
    const float *F;  /* 状态转移矩阵初值 n*n，NULL→全零               */
    const float *Q;  /* 过程噪声协方差 n*n，NULL→全零                 */
    const float *H;  /* 量测映射矩阵初值 m*n，NULL→全零               */
    const float *R;  /* 量测噪声协方差 m*m，NULL→全零                 */
    const float *B;  /* 控制输入矩阵 n*l，NULL→无（或全零）            */
} Ekf_Init_Config_s;

/*============================ 实例结构体 ============================*/

/**
 * @brief EKF 实例结构体
 *
 * @note 所有缓冲区（矩阵 + 工作区）由 EKF_INSTANCE_DEF 按上限维数静态分配并
 *       通过指针绑定到本实例，使用期不得 re-init / 换绑
 * @note 模型/状态矩阵字段公开可读写，便于调试（RTT/IDE 观察窗口）
 */
typedef struct EkfInstance
{
    /* 活跃维数（运行时 ≤ 预留上限） */
    uint8_t n;   /* 状态维数                                  */
    uint8_t m;   /* 量测维数                                   */
    uint8_t l;   /* 控制输入维数                               */
    uint8_t opt; /* Ekf_Opt_e 位标志                           */

    /* 预留上限（编译期由 EKF_INSTANCE_DEF 写定，const 只读，供越界校验） */
    const uint8_t n_max;
    const uint8_t m_max;
    const uint8_t l_max;

    void *ctx;     /* 上下文指针                     */
    Ekf_f_fn f_fn; /* 状态传播回调                    */
    Ekf_F_fn F_fn; /* 状态雅可比回调                  */
    Ekf_h_fn h_fn; /* 量测预测回调                    */
    Ekf_H_fn H_fn; /* 量测雅可比回调                  */

    /* 模型 / 状态矩阵（行主序、按活跃维紧凑，由宏绑定） */
    float *x; /* n   状态向量      */
    float *P; /* n*n 误差协方差    */
    float *F; /* n*n 状态转移矩阵  */
    float *Q; /* n*n 过程噪声      */
    float *H; /* m*n 量测映射矩阵  */
    float *R; /* m*m 量测噪声      */
    float *B; /* n*l 控制矩阵      */

    /* 内部工作区（由宏绑定，勿直接使用） */
    float *wA; /* n*n */
    float *wB; /* n*n */
    float *wC; /* n*m */
    float *wG; /* n*m */
    float *wD; /* 2*(m*m)，[S|I] 增广求逆用 */
    float *wE; /* n   */
    float *wF; /* m   */
    float *wR; /* m*m 量测噪声紧凑拷贝（UpdateM 的 m_now < m 时用） */
} EkfInstance;

/*============================ 实例声明宏 ============================*/

/**
 * @brief 定义并静态连接一个 EKF 实例
 * @param name 实例变量名
 * @param N    状态维上限（整数常量表达式，≥1）
 * @param M    量测维上限（整数常量表达式，≥1）
 * @param L    控制输入维上限（整数常量表达式，0 表示无控制）
 *
 * @note RAM 占用 ≈ (5N² + 3NM + 4M² + N·L + 2N + M) floats
 *       （n=6、m=3、l=0 时 285 floats ≈ 1.14 KB）
 * @note 例：EKF_INSTANCE_DEF(ekf_radar, 4, 2, 0);
 */
#define EKF_INSTANCE_DEF(name, N, M, L)                                                                                \
    static float name##_x[N];                                                                                          \
    static float name##_P[(N) * (N)];                                                                                  \
    static float name##_F[(N) * (N)];                                                                                  \
    static float name##_Q[(N) * (N)];                                                                                  \
    static float name##_H[(M) ? (M) * (N) : 1];                                                                        \
    static float name##_R[(M) ? (M) * (M) : 1];                                                                        \
    static float name##_B[(L) ? (N) * (L) : 1];                                                                        \
    static float name##_wA[(N) * (N)];                                                                                 \
    static float name##_wB[(N) * (N)];                                                                                 \
    static float name##_wC[(M) ? (N) * (M) : 1];                                                                       \
    static float name##_wG[(M) ? (N) * (M) : 1];                                                                       \
    static float name##_wD[(M) ? 2 * (M) * (M) : 1];                                                                   \
    static float name##_wE[N];                                                                                         \
    static float name##_wF[(M) ? (M) : 1];                                                                             \
    static float name##_wR[(M) ? (M) * (M) : 1];                                                                       \
    static EkfInstance name = {.n_max = (N),                                                                           \
                               .m_max = (M),                                                                           \
                               .l_max = (L),                                                                           \
                               .x = name##_x,                                                                          \
                               .P = name##_P,                                                                          \
                               .F = name##_F,                                                                          \
                               .Q = name##_Q,                                                                          \
                               .H = name##_H,                                                                          \
                               .R = name##_R,                                                                          \
                               .B = name##_B,                                                                          \
                               .wA = name##_wA,                                                                        \
                               .wB = name##_wB,                                                                        \
                               .wC = name##_wC,                                                                        \
                               .wG = name##_wG,                                                                        \
                               .wD = name##_wD,                                                                        \
                               .wE = name##_wE,                                                                        \
                               .wF = name##_wF,                                                                        \
                               .wR = name##_wR}

/*============================ 矩阵元素索引宏 ============================*/

/* 以活跃维为行距直读/改写实例矩阵元素（行主序） */
#define EKF_X(ekf, i) ((ekf)->x[(i)])                       /* x(i)          */
#define EKF_P(ekf, i, j) ((ekf)->P[(i) * (ekf)->n + (j)])   /* P(i,j) n×n   */
#define EKF_F(ekf, i, j) ((ekf)->F[(i) * (ekf)->n + (j)])   /* F(i,j) n×n   */
#define EKF_Q(ekf, i, j) ((ekf)->Q[(i) * (ekf)->n + (j)])   /* Q(i,j) n×n   */
#define EKF_H(ekf, i, j) ((ekf)->H[(i) * (ekf)->n + (j)])   /* H(i,j) m×n   */
#define EKF_R(ekf, i, j) ((ekf)->R[(i) * (ekf)->m + (j)])   /* R(i,j) m×m   */
#define EKF_B(ekf, i, j) ((ekf)->B[(i) * (ekf)->l + (j)])   /* B(i,j) n×l   */

/*============================ 公开接口声明 ============================*/

/**
 * @brief 初始化 EKF
 * @param ekf 实例指针（EKF_INSTANCE_DEF 声明）
 * @param cfg 初始化配置结构体指针
 * @return 状态码（EKF_OK / EKF_ERR_NULL / EKF_ERR_DIM）
 *
 * @note 校验维度 ≤ 预留上限并写入活跃维；按配置装载 x/P/F/Q/H/R/B 与四个回调
 * @note 未在配置中给出的模型矩阵将被清零，P0 缺省为单位阵
 */
Ekf_Status_e EkfInit(EkfInstance *ekf, const Ekf_Init_Config_s *cfg);

/**
 * @brief 重置滤波器状态：x = 0、P = 单位阵
 * @param ekf 实例指针
 * @return 状态码
 *
 * @note 保留活跃维度、模型矩阵、回调与 opt 不变
 */
Ekf_Status_e EkfReset(EkfInstance *ekf);

/**
 * @brief 直接设定状态与协方差（用于外部播种 / 重初始化）
 * @param ekf 实例指针
 * @param x   新状态（长度 n，不可为 NULL）
 * @param P   新协方差（n×n，可为 NULL 表示不修改）
 * @return 状态码
 */
Ekf_Status_e EkfSetState(EkfInstance *ekf, const float *x, const float *P);

/**
 * @brief 时间更新（预测）：x⁺ = f(x,u,dt)（或 F·x+B·u），P = F·P·Fᵀ + Q
 * @param ekf 实例指针
 * @param u   控制输入向量（长度 l），无控制 / 不需要时传 NULL
 * @param dt  距上次预测的时间间隔 [s]，原样透传给 f_fn / F_fn
 * @return 状态码
 *
 * @note F_fn 回调在状态传播**之前**调用（线性化点 = 传播前状态）
 * @note dt 不参与本模块任何计算，只透传；是否用 dt、怎么用由回调决定
 */
Ekf_Status_e EkfPredict(EkfInstance *ekf, const float *u, float dt);

/**
 * @brief 量测更新（校正），使用实例活跃量测维 m = ekf->m
 * @param ekf 实例指针
 * @param z   量测向量（长度 m）
 * @return 状态码（EKF_ERR_SINGULAR 表示 S 奇异，本次更新被跳过，x/P 未改动）
 *
 * @note 等价于 EkfUpdateM(ekf, ekf->m, z)
 */
Ekf_Status_e EkfUpdate(EkfInstance *ekf, const float *z);

/**
 * @brief 量测更新（通用形式，支持多速率 / 部分量测 / 多传感器顺序更新）
 * @param ekf   实例指针
 * @param m_now 本次使用的量测通道数（1 ~ ekf->m_max）
 * @param z     本次量测向量（长度 m_now）
 * @return 状态码
 *
 * @note 回调 h_fn/H_fn 收到同一个 m_now，只需给出前 m_now 个通道；H 的行距恒为
 *       实例的 n，R 在实例内以**活跃维 m** 为行距存储，本函数会自动取其前
 *       m_now×m_now 子块并转成紧凑布局（m_now==m 时为恒等拷贝）后交给内核
 * @note 一个预测周期内可对多个传感器各调用一次（共享 x/P）
 */
Ekf_Status_e EkfUpdateM(EkfInstance *ekf, uint8_t m_now, const float *z);

#endif /* __LIB_EKF_H */

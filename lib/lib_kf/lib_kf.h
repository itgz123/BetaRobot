/**
 * @file lib_kf.h
 * @brief 通用离散线性卡尔曼滤波(LKF)库
 *
 * @note 纯算法模块，无硬件 / 时间依赖
 * @note 支持运行时任意状态维 n、量测维 m（以及可选控制输入维 l），
 *       维度运行时给定，物理存储上限由 KALMAN_INSTANCE_DEF 预留
 * @note 使用单精度 float；通用任意维矩阵工具（增广 Gauss-Jordan 求逆、块置
 *       单位/对角）由 lib_math（lib_math_linalg.h）提供，本模块只含卡尔曼流程
 * @note dt 的处理：由调用方折进状态转移矩阵 F 与过程噪声 Q 后传入，
 *       本模块不感知时间（卡尔曼为纯代数流程）
 *
 * @note 使用示例 1（标量位置滤波）：
 * @code
 *     KALMAN_INSTANCE_DEF(kf_1d, 1, 1, 0);   // 状态1维 / 量测1维 / 无控制
 *
 *     static const float F1[] = {1.0f};
 *     static const float Q1[] = {1e-6f};
 *     static const float H1[] = {1.0f};
 *     static const float R1[] = {1e-3f};
 *     static const float P01[] = {1.0f};
 *
 *     Kalman_Init_Config_s cfg = {
 *         .n = 1, .m = 1,
 *         .F = F1, .Q = Q1, .H = H1, .R = R1, .P0 = P01,
 *     };
 *     KalmanInit(&kf_1d, &cfg);
 *
 *     // 周期调用：先预测（每步按实际时间差改写 kf_1d.F/Q），再用量测更新
 *     KalmanPredict(&kf_1d, NULL);
 *     KalmanUpdate(&kf_1d, &measure);
 *     // 状态估计 = kf_1d.x[0]
 * @endcode
 *
 * @note 使用示例 2（多传感器 / 多速率顺序更新，2 维匀速 + 位置量测）：
 * @code
 *     KALMAN_INSTANCE_DEF(kf_2d, 2, 1, 0);
 *
 *     // 时刻 k：状态 x = [pos; vel]，量测只观测位置分量
 *     KalmanPredict(&kf_2d, NULL);
 *
 *     // 传感器 A（高频，只测位置）：H_A = [1, 0]
 *     static const float HA[] = {1.0f, 0.0f};
 *     static const float RA[] = {0.1f};
 *     KalmanUpdateM(&kf_2d, 1, &zA, HA, RA);
 *
 *     // 传感器 B（低频，测速度）到达时再做一次部分量测更新
 *     // static const float HB[] = {0.0f, 1.0f};
 *     // KalmanUpdateM(&kf_2d, 1, &zB, HB, RB);
 *
 *     // 也可通过索引宏直接读写/改写模型（时变 F/H/R）：
 *     // KF_P(&kf_2d, 0, 0) = ...; KF_F(&kf_2d, 0, 1) = dt;
 * @endcode
 */

#ifndef __LIB_KF_H
#define __LIB_KF_H

#include <stddef.h>
#include <stdint.h>

/*============================ 选项 / 返回状态 ============================*/

/**
 * @brief 卡尔曼滤波选项位标志（写入 KalmanInstance.opt）
 */
typedef enum
{
    KALMAN_OPT_JOSEPH = (1u << 0), /* 置位=使用 Joseph 协方差更新（数值最稳）；
                                      未置位=经典式 P=(I-KH)·P            */
} Kalman_Opt_e;

/**
 * @brief 卡尔曼滤波返回状态
 */
typedef enum
{
    KALMAN_OK = 0,       /* 成功                                        */
    KALMAN_ERR_NULL,     /* 传入空指针                                  */
    KALMAN_ERR_DIM,      /* 维度非法（n/m/l 越界或为 0 等）              */
    KALMAN_ERR_SINGULAR, /* S 矩阵奇异，本次量测更新被跳过（x/P 未改动） */
} Kalman_Status_e;

/*============================ 配置结构体 ============================*/

/**
 * @brief 卡尔曼滤波器初始化配置结构体
 *
 * @note 所有矩阵均行主序、按活跃维紧凑排列（F/Q/P 为 n×n，H 为 m×n，
 *       R 为 m×m，B 为 n×l）；传 NULL 的模型矩阵将被清零，P0 默认单位阵，
 *       x0 默认全零
 * @note 非 NULL 指针指向的数组必须含完整元素数（即便只有对角线的 Q/R 也
 *       须给出全部 n²/m² 个元素）；对角噪声阵可直接用 lib_math 的
 *       Lib_Math_MatSetDiag / Lib_Math_MatSetDiagVal 经索引宏写入实例矩阵
 */
typedef struct
{
    uint8_t n;       /* 状态维数 (1~n_max)                  */
    uint8_t m;       /* 量测维数 (1~m_max)                  */
    uint8_t l;       /* 控制输入维数 (0~l_max, 0=无控制)     */
    uint8_t opt;     /* Kalman_Opt_e 位标志                  */
    const float *x0; /* 状态初值 n，NULL→全零                 */
    const float *P0; /* 协方差初值 n*n，NULL→单位阵           */
    const float *F;  /* 状态转移矩阵 n*n，NULL→全零           */
    const float *Q;  /* 过程噪声协方差 n*n，NULL→全零         */
    const float *H;  /* 量测映射矩阵 m*n，NULL→全零           */
    const float *R;  /* 量测噪声协方差 m*m，NULL→全零         */
    const float *B;  /* 控制输入矩阵 n*l，NULL→无（或全零）    */
} Kalman_Init_Config_s;

/*============================ 实例结构体 ============================*/

/**
 * @brief 卡尔曼滤波器实例结构体
 *
 * @note 所有缓冲区（矩阵 + 工作区）由 KALMAN_INSTANCE_DEF 按上限维数
 *       静态分配并通过指针绑定到本实例，使用期不得 re-init / 换绑
 * @note 模型/状态矩阵字段公开可读写，便于调试（RTT/IDE 观察窗口）与
 *       实现时变 F/H/Q/R
 */
typedef struct KalmanInstance
{
    /* 活跃维数（运行时 ≤ 预留上限） */
    uint8_t n;   /* 状态维数               */
    uint8_t m;   /* 量测维数（每步可更小的 m_now 由 UpdateM 指定） */
    uint8_t l;   /* 控制输入维数           */
    uint8_t opt; /* Kalman_Opt_e 位标志     */

    /* 预留上限（编译期由 KALMAN_INSTANCE_DEF 写定，const 只读，供越界校验） */
    const uint8_t n_max;
    const uint8_t m_max;
    const uint8_t l_max;

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
} KalmanInstance;

/*============================ 实例声明宏 ============================*/

/**
 * @brief 定义并静态连接一个卡尔曼实例
 * @param name 实例变量名
 * @param N    状态维上限（整数常量表达式，≥1）
 * @param M    量测维上限（整数常量表达式，≥1）
 * @param L    控制输入维上限（整数常量表达式，0 表示无控制）
 *
 * @note 按上限分配所需矩阵与工作区，RAM 占用 ≈ (5N² + 3NM + 3M² + N·L + 2N + M) floats
 * @note 例：KALMAN_INSTANCE_DEF(kf_imu, 9, 6, 0);
 */
#define KALMAN_INSTANCE_DEF(name, N, M, L)           \
    static float name##_x[N];                        \
    static float name##_P[(N) * (N)];                \
    static float name##_F[(N) * (N)];                \
    static float name##_Q[(N) * (N)];                \
    static float name##_H[(M) ? (M) * (N) : 1];      \
    static float name##_R[(M) ? (M) * (M) : 1];      \
    static float name##_B[(L) ? (N) * (L) : 1];      \
    static float name##_wA[(N) * (N)];               \
    static float name##_wB[(N) * (N)];               \
    static float name##_wC[(M) ? (N) * (M) : 1];     \
    static float name##_wG[(M) ? (N) * (M) : 1];     \
    static float name##_wD[(M) ? 2 * (M) * (M) : 1]; \
    static float name##_wE[N];                       \
    static float name##_wF[(M) ? (M) : 1];           \
    static KalmanInstance name = {                   \
        .n_max = (N), .m_max = (M), .l_max = (L), .x = name##_x, .P = name##_P, .F = name##_F, .Q = name##_Q, .H = name##_H, .R = name##_R, .B = name##_B, .wA = name##_wA, .wB = name##_wB, .wC = name##_wC, .wG = name##_wG, .wD = name##_wD, .wE = name##_wE, .wF = name##_wF}

/*============================ 矩阵元素索引宏 ============================*/

/* 以活跃维为行距直读/改写实例矩阵元素（行主序） */
#define KF_X(kf, i) ((kf)->x[(i)])                    /* x(i)          */
#define KF_P(kf, i, j) ((kf)->P[(i) * (kf)->n + (j)]) /* P(i,j) n×n   */
#define KF_F(kf, i, j) ((kf)->F[(i) * (kf)->n + (j)]) /* F(i,j) n×n   */
#define KF_Q(kf, i, j) ((kf)->Q[(i) * (kf)->n + (j)]) /* Q(i,j) n×n   */
#define KF_H(kf, i, j) ((kf)->H[(i) * (kf)->n + (j)]) /* H(i,j) m×n   */
#define KF_R(kf, i, j) ((kf)->R[(i) * (kf)->m + (j)]) /* R(i,j) m×m   */
#define KF_B(kf, i, j) ((kf)->B[(i) * (kf)->l + (j)]) /* B(i,j) n×l   */

/*============================ 公开接口声明 ============================*/

/**
 * @brief 初始化卡尔曼滤波器
 * @param kf  实例指针（KALMAN_INSTANCE_DEF 声明）
 * @param cfg 初始化配置结构体指针
 * @return 状态码（KALMAN_OK / KALMAN_ERR_NULL / KALMAN_ERR_DIM）
 *
 * @note 校验维度 ≤ 预留上限并写入活跃维；按配置装载 x/P/F/Q/H/R/B
 * @note 未在配置中给出的模型矩阵将被清零，P0 缺省为单位阵
 */
Kalman_Status_e KalmanInit(KalmanInstance *kf, const Kalman_Init_Config_s *cfg);

/**
 * @brief 重置卡尔曼滤波器状态
 * @param kf 实例指针
 * @return 状态码
 *
 * @note 仅清状态（x=0）并令 P=单位阵；保留活跃维度与模型矩阵 F/H/Q/R/B，
 *       不清除 opt
 */
Kalman_Status_e KalmanReset(KalmanInstance *kf);

/**
 * @brief 时间更新（预测）：x = F·x (+ B·u)，P = F·P·Fᵀ + Q
 * @param kf 实例指针
 * @param u  控制输入向量（长度 l），无控制 / 不需要时传 NULL
 * @return 状态码
 *
 * @note dt 与过程噪声的变化由调用方改写 kf->F / kf->Q 后体现（时变系统直接
 *       修改实例矩阵再调用即可）
 */
Kalman_Status_e KalmanPredict(KalmanInstance *kf, const float *u);

/**
 * @brief 量测更新（校正），使用实例内置模型 H/R，量测通道数 m = kf->m
 * @param kf 实例指针
 * @param z  量测向量（长度 m）
 * @return 状态码（KALMAN_ERR_SINGULAR 表示 S 奇异，本次更新被跳过，x/P 未改动）
 *
 * @note 等价于 KalmanUpdateM(kf, kf->m, z, kf->H, kf->R)
 */
Kalman_Status_e KalmanUpdate(KalmanInstance *kf, const float *z);

/**
 * @brief 量测更新（通用形式，支持多速率 / 部分量测 / 多传感器顺序更新）
 * @param kf    实例指针
 * @param m_now 本次使用的量测通道数（1 ~ kf->m_max，可小于 kf->m）
 * @param z     本次量测向量（长度 m_now）
 * @param H     本次量测映射矩阵（m_now×n，行主序，行距 n）
 * @param R     本次量测噪声协方差（m_now×m_now，行主序）
 * @return 状态码
 *
 * @note 在一个预测周期内可对多个传感器各调用一次（共享 x/P）；
 *       "只测位置"、"只测速度" 等部分状态可观测场景直接用本接口
 */
Kalman_Status_e KalmanUpdateM(KalmanInstance *kf, uint8_t m_now, const float *z, const float *H, const float *R);

#endif /* __LIB_KF_H */

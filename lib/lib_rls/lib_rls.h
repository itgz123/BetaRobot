/**
 * @file lib_rls.h
 * @brief 通用递推最小二乘（RLS）：在线估计线性模型 y = φᵀθ 的参数 θ
 *
 * @note 模型与递推（遗忘因子 λ）：
 *         K = P·φ / (λ + φᵀ·P·φ)
 *         θ ← θ + K·(y − φᵀθ)
 *         P ← (P − K·φᵀ·P) / λ
 *       λ = 1 为不遗忘（累积拟合，适合"一次辨识、合格后冻结"）；
 *       0 < λ < 1 为指数遗忘（适合慢时变参数的在线跟踪）。
 *
 * @note 职责边界（纯算法库）：
 *       - 不分配内存、不依赖 FreeRTOS/DWT/CMSIS-DSP/任何 bsp/drv；
 *       - 不管采样门控、回归量归一化、物理区间/激励判据 —— 这些属于调用方；
 *       - 每次调用只做一次递推，dt/采样周期由调用方决定；
 *       - 维数在编译期由 LIB_RLS_INSTANCE_DEF 确定，实例由调用方静态分配。
 *
 * @note 数值保护：
 *       - 输入或中间量非有限 → 按 fail_policy 冻结或复位；
 *       - 分母 ≤ LIB_RLS_DENOM_EPS → 同上；
 *       - 每次更新对称化 P，并把 P 元素/对角钳在 [p_min, p_max]；
 *       - 参数可钳在 [param_min, param_max]；
 *       - 可选 innovation 限幅，抑制离群样本；
 *       - **更新是原子的**：任何检查失败都不会留下"算了一半"的 θ/P。
 *
 * @note 与三份参考实现的关系：
 *       - RM2024-PowerModule/Utils/RLS.hpp：保留标准递推、λ/δ/初值语义；
 *         去掉 Matrix 模板与 FreeRTOS 时间戳；修正其 validate() 里
 *         `lambda >= 0 || lambda <= 1`（恒真）应为 `&&` 的问题。
 *       - RM_YunQian/modules/RLS：保留 λ、P_init 的用法；
 *         去掉 malloc 与 CMSIS-DSP 依赖 —— 标量输出的分母本来就是一维，
 *         不需要矩阵求逆。
 *       - ignore/轮阻力rls拟合：补上其缺失的 rls.hpp，并吸收
 *         "数值失败冻结、不自动重试"的策略；归一化/物理区间/四工况计数
 *         仍由调用方实现。
 *
 * @note 编译开关：固件在 app_cfg.h 定义 LIB_RLS_USED；不定义则 .c 是空翻译单元。
 *       PC 端测试用 -DLIB_RLS_USED -DLIB_RLS_STANDALONE 跳过 app_cfg.h。
 *
 * @note 本模块只依赖 <stdint.h>/<math.h> 与 lib_math，可在 PC 上单测。
 */

#ifndef __LIB_RLS_H
#define __LIB_RLS_H

#include <stdint.h>

/* 分母下限：λ + φᵀPφ 理论上恒正；低于该值视为数值失效。
 * 取 1e-9 是为了在 float 下仍能容纳很小的激励，同时挡住 0/负值。 */
#define LIB_RLS_DENOM_EPS (1.0e-9f)

/*============================================
 *              状态码 / 失败策略
 *============================================*/

/**
 * @brief RLS 返回值 / 最近一次状态
 */
typedef enum : int8_t
{
    LIB_RLS_OK = 0,              /* 更新成功 */
    LIB_RLS_ERR_ARG = -1,        /* 空指针 / 配置非法 / 实例未初始化 */
    LIB_RLS_ERR_NOT_FINITE = -2, /* 输入或中间量出现 NaN/Inf */
    LIB_RLS_ERR_DENOM = -3,      /* λ + φᵀPφ 非有限或过小 */
    LIB_RLS_ERR_FROZEN = -4,     /* 实例已冻结，本次未更新 */
} LibRls_Status_e;

/**
 * @brief 数值失效后的处理策略
 */
typedef enum : uint8_t
{
    LIB_RLS_FAIL_FREEZE = 0, /* 冻结：保留最后一次有效 θ/P，后续 Update 返回 ERR_FROZEN */
    LIB_RLS_FAIL_RESET = 1,  /* 复位：θ 恢复初值、P 恢复 δI，后续 Update 可继续 */
} LibRls_FailPolicy_e;

/*============================================
 *              配置 / 实例
 *============================================*/

/**
 * @brief RLS 初始化配置
 *
 * @note 推荐做法（与参考实现一致）：
 *       - 轮阻力/功率模型这类"固定参数"辨识：lambda = 1，合格后由调用方冻结；
 *       - 慢时变跟踪：lambda = 0.999 ~ 0.9999，并配合参数上下限；
 *       - p_init 取 100~1000（回归量做过归一化时），p_min 取 0 或 1e-9，
 *         p_max 取 1e6（或 FLT_MAX 关闭上限）。
 */
typedef struct
{
    float lambda;                    /* 遗忘因子，0 < lambda <= 1（1 = 不遗忘） */
    float p_init;                    /* 协方差初值：P(0) = p_init·I，须落在 [p_min, p_max] */
    float p_min;                     /* P 对角下限，>= 0（0 = 不设下限） */
    float p_max;                     /* P 元素上限，>= p_min（FLT_MAX = 不设上限） */
    float param_min;                 /* 参数下限（-FLT_MAX = 不设下限） */
    float param_max;                 /* 参数上限（+FLT_MAX = 不设上限） */
    float innovation_limit;          /* |y − φᵀθ| 限幅，0 = 关闭；>0 时按符号钳位 */
    LibRls_FailPolicy_e fail_policy; /* 数值失效后的处理方式 */
    const float *theta_init;         /* 初始参数，长度 = 实例维数；NULL = 全 0 */
} LibRls_Config_s;

/**
 * @brief RLS 实例（由 LIB_RLS_INSTANCE_DEF 静态分配并绑定缓冲区）
 *
 * @note 维数 dim 在编译期固定；theta/P/gain/p_phi 为**行主序紧凑数组**：
 *       P(i,j) = P[i * dim + j]，读写可用 LIB_RLS_THETA / LIB_RLS_P 宏。
 *
 * @note 除内部工作区（w_*）外，其余字段均可直接读取：
 *       - theta[0..dim-1]：当前参数估计
 *       - last_prediction / last_innovation：最近一次的预测值与原始残差
 *       - last_denominator：最近一次的分母 λ + φᵀPφ
 *       - update_count / status / frozen：更新计数与状态
 */
typedef struct
{
    const uint8_t dim;   /* 回归量维数（编译期由 LIB_RLS_INSTANCE_DEF 写定） */
    uint8_t initialized; /* Init 成功置 1 */
    uint8_t frozen;      /* 失败冻结标志 */
    LibRls_FailPolicy_e fail_policy;
    LibRls_Status_e status; /* 最近一次更新状态（冻结后保留根因） */

    float lambda;
    float p_init;
    float p_min;
    float p_max;
    float param_min;
    float param_max;
    float innovation_limit;

    float *theta;      /* dim      当前参数 */
    float *theta_init; /* dim      复位基准 */
    float *P;          /* dim*dim  协方差（行主序） */
    float *gain;       /* dim      最近一次的增益向量（诊断） */
    float *p_phi;      /* dim      最近一次的 P·φ（诊断） */

    /* 内部工作区（由宏绑定，勿直接使用；供原子更新使用） */
    float *w_phi;   /* dim     */
    float *w_theta; /* dim     */
    float *w_gain;  /* dim     */
    float *w_p_phi; /* dim     */
    float *w_P;     /* dim*dim */

    uint32_t update_count;  /* 成功更新次数 */
    float last_prediction;  /* 最近一次的 φᵀθ */
    float last_innovation;  /* 最近一次的 y − φᵀθ（原始值，未限幅） */
    float last_denominator; /* 最近一次的 λ + φᵀPφ */
} LibRls_Instance_s;

/*============================================
 *              实例声明 / 索引宏
 *============================================*/

/**
 * @brief 定义并静态连接一个 RLS 实例
 * @param name 实例变量名
 * @param N    回归量维数（整数常量表达式，1..255）
 *
 * @note 在 .c 的函数内或文件作用域使用；按 N 精确分配矩阵与工作区
 *       （约 4·(N² + 4N) 字节，不含诊断字段）。
 * @note 例：LIB_RLS_INSTANCE_DEF(rls_wheel, 4);
 */
#define LIB_RLS_INSTANCE_DEF(name, N)                                                                                                                          \
    _Static_assert((N) >= 1 && (N) <= 255, "LIB_RLS_INSTANCE_DEF: 1 <= N <= 255");                                                                             \
    static float name##_theta[(N)];                                                                                                                            \
    static float name##_theta_init[(N)];                                                                                                                       \
    static float name##_P[(N) * (N)];                                                                                                                          \
    static float name##_gain[(N)];                                                                                                                             \
    static float name##_p_phi[(N)];                                                                                                                            \
    static float name##_w_phi[(N)];                                                                                                                            \
    static float name##_w_theta[(N)];                                                                                                                          \
    static float name##_w_gain[(N)];                                                                                                                           \
    static float name##_w_p_phi[(N)];                                                                                                                          \
    static float name##_w_P[(N) * (N)];                                                                                                                        \
    static LibRls_Instance_s name = {.dim = (N),                                                                                                               \
                                     .theta = name##_theta,                                                                                                    \
                                     .theta_init = name##_theta_init,                                                                                          \
                                     .P = name##_P,                                                                                                            \
                                     .gain = name##_gain,                                                                                                      \
                                     .p_phi = name##_p_phi,                                                                                                    \
                                     .w_phi = name##_w_phi,                                                                                                    \
                                     .w_theta = name##_w_theta,                                                                                                \
                                     .w_gain = name##_w_gain,                                                                                                  \
                                     .w_p_phi = name##_w_p_phi,                                                                                                \
                                     .w_P = name##_w_P}

/* 以 dim 为行距直读/改写实例参数与协方差元素 */
#define LIB_RLS_THETA(rls, i) ((rls)->theta[(i)])               /* θ(i)       */
#define LIB_RLS_P(rls, i, j) ((rls)->P[(i) * (rls)->dim + (j)]) /* P(i,j) dim×dim */

/*============================================
 *              接口
 *============================================*/

/**
 * @brief 初始化 RLS 实例（实例须由 LIB_RLS_INSTANCE_DEF 声明）
 * @param inst 实例指针
 * @param cfg  配置指针
 * @return LIB_RLS_OK / LIB_RLS_ERR_ARG
 *
 * @note 配置/实例校验失败时**不改动 inst**（便于调用方保留旧实例）。
 * @note 成功后 θ = theta_init（或 0），P = p_init·I，计数/状态清零。
 */
LibRls_Status_e LibRlsInit(LibRls_Instance_s *inst, const LibRls_Config_s *cfg);

/**
 * @brief 复位：θ 恢复 theta_init，P 恢复 p_init·I，清冻结/计数/诊断量
 * @return LIB_RLS_OK / LIB_RLS_ERR_ARG（未初始化）
 */
LibRls_Status_e LibRlsReset(LibRls_Instance_s *inst);

/**
 * @brief 同时设置当前参数和复位基准
 * @param theta 长度 = inst->dim，非有限值返回 ERR_ARG
 * @return LIB_RLS_OK / LIB_RLS_ERR_ARG
 *
 * @note 有限值会被钳到 [param_min, param_max]。设置后 update_count 不变。
 */
LibRls_Status_e LibRlsSetParams(LibRls_Instance_s *inst, const float *theta);

/**
 * @brief 运行期修改遗忘因子
 * @param lambda (0, 1]
 * @return LIB_RLS_OK / LIB_RLS_ERR_ARG
 */
LibRls_Status_e LibRlsSetLambda(LibRls_Instance_s *inst, float lambda);

/**
 * @brief 用当前参数预测输出 φᵀθ
 * @return 预测值；inst/phi 非法时返回 0
 */
float LibRlsPredict(const LibRls_Instance_s *inst, const float *phi);

/**
 * @brief 执行一次 RLS 更新
 * @param inst 实例
 * @param phi  回归量，长度 = dim
 * @param y    本次观测输出
 * @return 本次状态
 *
 * @note 失败时按 fail_policy 处理：FREEZE 保留旧 θ/P；RESET 恢复初值。
 * @note 成功时同时更新 last_prediction/last_innovation/last_denominator。
 */
LibRls_Status_e LibRlsUpdate(LibRls_Instance_s *inst, const float *phi, float y);

#endif /* __LIB_RLS_H */

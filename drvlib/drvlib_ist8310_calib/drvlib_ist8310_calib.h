/**
 * @file drvlib_ist8310_calib.h
 * @brief IST8310 磁力计标定参数与修正：drvlib_bmi088_ist8310_eskf 使用
 *
 * ═══════════════════ 为什么单独抽出来 ═══════════════════
 * 与 drvlib_bmi088_calib.h 对称：标定是**器件属性**，不是估计算法的属性。同一颗
 * IST8310、同一套装安装，无论后面接 ESKF 还是别的融合算法，用的都应该是同一份
 * 标定数据。所以结构体与修正函数放这里，融合模块只负责"怎么用修正后的方向"。
 *
 * ═══════════════════ 标定策略：全部在 PC 端离线完成 ═══════════════════
 * 本模块**不做任何运行期标定**。上场前用 PC（python，见同目录 ist8310_calib.py）
 * 做椭球拟合，结果写进 app 里的 static const，随固件落进 .rodata（即"写死到固件"）。
 * 换磁计 / 拆装 / 改安装方式 / 云台上电大电流走线变了，都必须重标并重新烧录。
 *
 * ═══════════════════ 为什么 mount_rot 不并进软铁 ═══════════════════
 * 椭球拟合对**纯旋转不可观测**：把整条磁测链乘任意旋转矩阵，椭球还是椭球，拟合残差
 * 一模一样。所以"磁计 → IMU"的安装旋转**必须另标**（用已知姿态实验），混进软铁矩阵
 * 只会让两者都失去物理含义：软铁是器件的线性畸变（对角占优、可解释），
 * 而安装旋转是刚体变换（正交、可解释），性质完全不同。
 *
 * ═══════════════════ 修正模型 ═══════════════════
 *   m_c = ( R_mount · S_soft · (m_raw - h_hard) ) ⊘ S_scale
 *
 *   h_hard   硬铁偏置 (µT)：磁计自身铁磁材料 + 板级恒定磁场，**先减**
 *   S_soft   软铁 3×3：器件尺度/非正交 + 附近可磁化材料的畸变，把椭球拉回球
 *   R_mount  磁计 → IMU 的安装旋转（另一个 IMU/机体系下的量需要这个才能和 acc 对齐）
 *   S_scale  逐轴尺度，**最后除**；椭球拟合已自带归一化，这里只留给"事后微调"
 *
 * 顺序不是随意的：硬铁/软铁都在**磁计自身坐标系**里测出并生效，安装旋转把它转到
 * IMU 机体系，两件事分属不同系，谁前谁后不能混。而 `S_scale` 是输出系里的最后一道
 * 修剪（拟合的绝对模长由参考场强钉死，通常无需再动，故默认全 0）。
 *
 * ═══════════════════ 零初始化 == 完全不修正 ═══════════════════
 * 全 0 的结构体与"不修正"完全等价：hard_iron 全 0 即不减，soft_iron / mount_rot
 * 全 0 视为**单位阵**，scale ≤ 0 视为 1。于是 app 里只写了硬铁、没写软铁时不必
 * 补 `.soft_iron = {{1,0,0},{0,1,0},{0,0,1}}` 这种噪音，未标定的默认态也天然安全。
 * 这条保证由 `IST8310MagCorrect` 自己维持（而不是只靠 Load 归一），
 * 所以把零初始化的 `IST8310Calib_s` 直接喂进去也得到原样输出。
 *
 * ═══════════════════ 绝对模长的一个提醒 ═══════════════════
 * 融合算法用的是**归一化方向**，故 h_hard / S_soft 的绝对尺度不影响姿态；但模块的
 * "模长偏离门限"（判是否吃到电机磁场）直接看 |m_c| 与 `mag_ref_uT` 的差，
 * 所以 app 里 `.mag_ref_uT` 必须与标定时钉死的场强（脚本会打印）保持一致。
 *
 * @note 头文件即全部（无 .c）：两个函数都很小，static inline 进各自的 TU，不引入
 *       新符号。只依赖 ist8310_reg_def.h（纯器件事实、无 HAL/工程依赖），
 *       故 PC 端也能直接编译，标定脚本与小工具可复用同一份逻辑。
 */

#ifndef __DRVLIB_IST8310_CALIB_H
#define __DRVLIB_IST8310_CALIB_H

#include <stdint.h> /* ist8310_reg_def.h 自身用到 uint8_t 但不带 <stdint.h>（固件里靠 main.h 先引入）*/
#include "ist8310_reg_def.h" /* IST8310_AXIS_NUM（纯器件事实，不含 HAL 依赖） */

/*============================ 标定结构体 ============================*/

/**
 * @brief IST8310 标定参数（全部来自 PC 端离线椭球拟合 + 已知姿态实验）
 *
 * @note 除 scale 外，零值即"不修正"；矩阵（soft_iron / mount_rot）全 0 视为单位阵，
 *       scale 填 0 也按 1 处理。零初始化的本结构体与"完全不修正"完全等价。
 * @note 矩阵一律**行主序**（`m[行][列]`），与 C 数组下标一致，避免转置歧义。
 * @note 建议在 app 里定义为 static const（落在 .rodata = 编进 flash 的数值）。
 */
typedef struct
{
    float hard_iron[IST8310_AXIS_NUM];                  /* 硬铁偏置 (µT)，先减 */
    float soft_iron[IST8310_AXIS_NUM][IST8310_AXIS_NUM]; /* 软铁 3×3 行主序；全 0 → 单位阵 */
    float mount_rot[IST8310_AXIS_NUM][IST8310_AXIS_NUM]; /* 磁计→IMU 安装旋转，行主序；全 0 → 单位阵 */
    float scale[IST8310_AXIS_NUM];                      /* 每轴尺度，**最后除**；0 或 1 = 不修正 */
} IST8310Calib_s;

/*============================ 私有助手 ============================*/

/** @brief 3×3 是否全 0（全 0 视为单位阵的判据） */
static inline int IST8310Calib_MatIsZero(const float m[IST8310_AXIS_NUM][IST8310_AXIS_NUM])
{
    for (int i = 0; i < IST8310_AXIS_NUM; i++)
    {
        for (int j = 0; j < IST8310_AXIS_NUM; j++)
        {
            if (m[i][j] != 0.0f)
                return 0;
        }
    }
    return 1;
}

/** @brief out = M·v（3×3 行主序乘 3×1），out 可与 v 同一数组 */
static inline void IST8310Calib_MatVec(const float m[IST8310_AXIS_NUM][IST8310_AXIS_NUM], const float v[IST8310_AXIS_NUM],
                                       float out[IST8310_AXIS_NUM])
{
    float r[IST8310_AXIS_NUM];
    for (int i = 0; i < IST8310_AXIS_NUM; i++)
    {
        r[i] = m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2];
    }
    for (int i = 0; i < IST8310_AXIS_NUM; i++)
    {
        out[i] = r[i];
    }
}

/*============================ 公开接口 ============================*/

/**
 * @brief 装载标定参数：拷贝 + 合法性归一（全 0 矩阵 → 单位阵，scale ≤ 0 → 1）
 * @param dst 目标（通常是实例里的一份拷贝）
 * @param src 源（app 里那份 static const）；NULL = 完全不修正
 *
 * @note 拷贝而非存指针：调用方不必保证指针长期有效；源头仍在 flash 里。
 * @note 归一后结构体是"规范形式"（矩阵要么是真值要么是单位阵、scale 恒 > 0），
 *       便于在调试器里直读、也便于做相等比较；但**修正函数并不依赖它**（见文件头）。
 */
static inline void IST8310CalibLoad(IST8310Calib_s *dst, const IST8310Calib_s *src)
{
    const IST8310Calib_s zero = {0};

    *dst = (src != NULL) ? *src : zero;

    if (IST8310Calib_MatIsZero(dst->soft_iron))
    {
        for (int i = 0; i < IST8310_AXIS_NUM; i++)
        {
            dst->soft_iron[i][i] = 1.0f;
        }
    }
    if (IST8310Calib_MatIsZero(dst->mount_rot))
    {
        for (int i = 0; i < IST8310_AXIS_NUM; i++)
        {
            dst->mount_rot[i][i] = 1.0f;
        }
    }
    for (int i = 0; i < IST8310_AXIS_NUM; i++)
    {
        if (dst->scale[i] <= 0.0f)
            dst->scale[i] = 1.0f;
    }
}

/**
 * @brief 按标定参数修正一帧磁数据：减硬铁 → 乘软铁 → 转安装 → 除尺度
 * @param raw drv 层输出的原始磁感应强度 (µT)
 * @param c 标定参数；NULL 或零初始化 = 完全不修正
 * @param out 修正结果（IMU 机体系，µT）；可与 raw 同一数组
 *
 * @note 零值语义在**本函数内**成立，不依赖调用方先跑 IST8310CalibLoad（见文件头
 *       「零初始化 == 完全不修正」）。代价是每次调用多 18 次浮点比较 —— 换帧率
 *       100~200Hz 下可忽略，换来的是"忘了 Load 就静默输出 0"这个坑不存在。
 */
static inline void IST8310MagCorrect(const float raw[IST8310_AXIS_NUM], const IST8310Calib_s *c,
                                     float out[IST8310_AXIS_NUM])
{
    float v[IST8310_AXIS_NUM];

    if (c == NULL)
    {
        for (int i = 0; i < IST8310_AXIS_NUM; i++)
        {
            out[i] = raw[i];
        }
        return;
    }

    /* 1. 硬铁：磁计系内先减 */
    for (int i = 0; i < IST8310_AXIS_NUM; i++)
    {
        v[i] = raw[i] - c->hard_iron[i];
    }

    /* 2. 软铁：磁计系内把椭球拉回球（全 0 → 单位阵，跳过） */
    if (!IST8310Calib_MatIsZero(c->soft_iron))
    {
        IST8310Calib_MatVec(c->soft_iron, v, v);
    }

    /* 3. 安装旋转：磁计系 → IMU 机体系（全 0 → 单位阵，跳过） */
    if (!IST8310Calib_MatIsZero(c->mount_rot))
    {
        IST8310Calib_MatVec(c->mount_rot, v, v);
    }

    /* 4. 尺度：输出系内最后除（≤0 视为 1，不修正） */
    for (int i = 0; i < IST8310_AXIS_NUM; i++)
    {
        out[i] = (c->scale[i] > 0.0f) ? (v[i] / c->scale[i]) : v[i];
    }
}

#endif /* __DRVLIB_IST8310_CALIB_H */

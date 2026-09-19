/**
 * @file drvlib_bmi088_calib.h
 * @brief BMI088 标定参数与修正：drvlib_bmi088_kalman / drvlib_bmi088_mahony 共用
 *
 * ═══════════════════ 为什么单独抽出来 ═══════════════════
 * 标定是**器件属性**，不是估计算法的属性：同一颗 BMI088、同一套装安装，无论后面
 * 接线性 KF 还是 Mahony，用的都应该是同一份标定数据。所以结构体与修正函数放在
 * 这里共用，两个算法模块只负责"怎么用修正后的数据"。
 * app 里只需一份 `static const BMI088_Calib_s`，两个模块都能喂。
 *
 * ═══════════════════ 标定策略：全部在 PC 端离线完成 ═══════════════════
 * 两个算法模块都**不做任何运行期标定**。上场前用 PC（python）标定，结果写进 app
 * 里的 static const，随固件落进 .rodata（即"写死到固件"）。换 IMU / 拆装 / 改安装
 * 方式后必须重新标定并重新烧录。
 *
 * 可标定项与"没有转台/温箱"时各自的可得性：
 *   ✅ 陀螺零偏          静止均值
 *   ✅ 陀螺零偏温漂斜率  自热扫温（上电静置数十分钟，记温度与零偏），不需温箱
 *   ⚠️ 陀螺尺度/非正交   需已知角速率参考：用云台 yaw 轴自转 + 编码器即可，不需转台
 *   ⚠️ acc 零偏/尺度/非正交  需六位置夹具。注意**绕竖直轴转动对 acc 完全无用**
 *      （重力在 IMU 系的投影不随绕 Z 旋转改变），要标 acc 必须改变它的姿态
 *   ❌ 非线性/迟滞、g 敏感、饱和、杆臂、带宽/延迟 —— 需转台/温箱/离心机，暂不做
 *
 * ═══════════════════ 修正模型 ═══════════════════
 *   m = M·(S⊙t) + b   ⇒   t = S⁻¹ ⊙ (M⁻¹·(m - b))
 *   M（非正交/轴间失准）取对角为 1 的上三角，一阶近似 M⁻¹ ≈ I - offdiag。
 *   顺序：先扣零偏 → 再去非正交 → 最后除尺度。
 *
 * @note 头文件即全部（无 .c）：两个函数都很小，static inline 进各自的 TU，不引入
 *       新符号；未标定时（全 0 / 全 1）是恒等变换，只多 6 次乘法 3 次除法。
 */

#ifndef __DRVLIB_BMI088_CALIB_H
#define __DRVLIB_BMI088_CALIB_H

#include "drv_bmi088.h" /* BMI088_AXIS_NUM */

/*============================ 标定结构体 ============================*/

/**
 * @brief 单个三轴传感器的标定参数（全部来自 PC 端离线标定）
 *
 * @note 除 scale 外，零值即"不修正"。scale 填 0 也按 1 处理（见 BMI088CalibLoad），
 *       于是零初始化的标定结构体与"完全不修正"完全等价 —— app 只填 bias 时不必
 *       写出 `.scale = {1,1,1}` 这种噪音。
 */
typedef struct
{
    float bias[BMI088_AXIS_NUM];        /* 零偏：陀螺 rad/s，acc m/s² */
    float scale[BMI088_AXIS_NUM];       /* 尺度因子真值：修正时**除以**它；0 或 1 = 不修正 */
    float misalign[BMI088_AXIS_NUM];    /* 非正交/轴间失准（上三角交叉项）：[0]=xy [1]=xz [2]=yz */
    float bias_tempco[BMI088_AXIS_NUM]; /* 零偏温度系数 (1/℃)；0 = 不补偿 */
} BMI088_AxisCalib_s;

/**
 * @brief PC 端标定结果总表
 * @note 建议在 app 里定义为 static const（落在 .rodata = 编进 flash 的数值）
 */
typedef struct
{
    BMI088_AxisCalib_s gyro; /* 陀螺：单位 rad/s、rad/s/℃ */
    BMI088_AxisCalib_s acc;  /* 加速度计：单位 m/s²、m/s²/℃ */
    float temp_ref;          /* 温漂参考温度 (℃)：bias 是在该温度下测的 */
} BMI088_Calib_s;

/*============================ 共用函数 ============================*/

/**
 * @brief 装载单个三轴传感器的标定参数：拷贝 + 合法性归一（scale ≤ 0 → 1）
 * @param dst 目标（通常是实例里的一份拷贝）
 * @param src 源（取 BMI088_Calib_s 的 gyro 或 acc 成员）；NULL = 完全不修正
 *
 * @note 拷贝而非存指针：调用方不必保证指针长期有效；源头仍在 flash 里
 */
static inline void BMI088CalibLoad(BMI088_AxisCalib_s *dst, const BMI088_AxisCalib_s *src)
{
    const BMI088_AxisCalib_s zero = {0};

    *dst = (src != NULL) ? *src : zero;

    for (uint8_t i = 0; i < BMI088_AXIS_NUM; i++)
    {
        if (dst->scale[i] <= 0.0f)
            dst->scale[i] = 1.0f;
    }
}

/**
 * @brief 按标定参数修正一个三轴传感器：扣零偏 → 去非正交 → 除尺度
 * @param raw drv 层输出的原始物理量
 * @param bias 本帧生效零偏（标定值 + 温度系数·ΔT）
 * @param c 标定参数（scale 已由 BMI088CalibLoad 归一，除法安全）
 * @param out 修正结果（可与 raw 同一数组）
 */
static inline void BMI088AxisCorrect(const float raw[BMI088_AXIS_NUM], const float bias[BMI088_AXIS_NUM],
                                     const BMI088_AxisCalib_s *c, float out[BMI088_AXIS_NUM])
{
    float x = raw[0] - bias[0];
    float y = raw[1] - bias[1];
    float z = raw[2] - bias[2];

    float x1 = x - c->misalign[0] * y - c->misalign[1] * z;
    float y1 = y - c->misalign[2] * z;

    out[0] = x1 / c->scale[0];
    out[1] = y1 / c->scale[1];
    out[2] = z / c->scale[2];
}

#endif /* __DRVLIB_BMI088_CALIB_H */

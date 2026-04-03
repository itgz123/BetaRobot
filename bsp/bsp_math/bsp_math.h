/**
 * @file bsp_math.h
 * @brief BSP层数学加速封装：FPU/DSP/CORDIC/CRC/FMAC/MPU/RAMECC统一接口
 *
 * @note 本文件职责：
 *       1. 根据硬件特性提供条件编译接口
 *       2. 封装硬件加速功能（CORDIC三角函数、CRC计算等）
 *       3. 提供软件回退实现（硬件不可用时）
 *
 * @note 硬件特性由 bsp_cfg.h 中的宏定义控制：
 *       - HAS_FPU:     浮点运算单元
 *       - HAS_DSP:     DSP指令集
 *       - HAS_CORDIC:  坐标旋转数字计算协处理器
 *       - HAS_CRC:     硬件CRC
 *       - HAS_FMAC:    滤波数学加速器
 *       - HAS_MPU:     内存保护单元
 *       - HAS_RAMECC:  RAM错误校正
 */

#ifndef __BSP_MATH_H
#define __BSP_MATH_H

#include "bsp_cfg.h"
#include <stdint.h>

/*============================================
 *              硬件特性查询接口
 *============================================*/

/**
 * @brief 检查FPU是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasFPU(void)
{
#if HAS_FPU
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief 检查DSP指令集是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasDSP(void)
{
#if HAS_DSP
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief 检查CORDIC协处理器是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasCORDIC(void)
{
#if HAS_CORDIC
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief 检查硬件CRC是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasCRC(void)
{
#if HAS_CRC
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief 检查FMAC是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasFMAC(void)
{
#if HAS_FMAC
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief 检查MPU是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasMPU(void)
{
#if HAS_MPU
    return 1;
#else
    return 0;
#endif
}

/**
 * @brief 检查RAMECC是否可用
 * @return 1-可用, 0-不可用
 */
static inline uint8_t BSP_Math_HasRAMECC(void)
{
#if HAS_RAMECC
    return 1;
#else
    return 0;
#endif
}

/*============================================
 *              初始化接口
 *============================================*/

/**
 * @brief 初始化数学加速模块
 * @note 根据硬件特性自动初始化：
 *       - FPU: 启用浮点运算
 *       - CORDIC: 初始化CORDIC外设
 *       - CRC: 初始化CRC外设
 *       - FMAC: 初始化FMAC外设
 *       - MPU: 配置内存保护区域（可选）
 *       - RAMECC: 启用RAM错误检测
 */
void BSP_Math_Init(void);

/*============================================
 *              三角函数接口
 *============================================*/

/**
 * @brief 计算sin(theta)
 * @param theta 角度（弧度）
 * @return sin(theta)
 * @note 优先级：CORDIC硬件 > CMSIS-DSP > 标准库
 */
float BSP_Math_Sin(float theta);

/**
 * @brief 计算cos(theta)
 * @param theta 角度（弧度）
 * @return cos(theta)
 * @note 优先级：CORDIC硬件 > CMSIS-DSP > 标准库
 */
float BSP_Math_Cos(float theta);

/**
 * @brief 同时计算sin和cos
 * @param theta 角度（弧度）
 * @param[out] p_sin sin(theta)输出
 * @param[out] p_cos cos(theta)输出
 * @note 优先级：CORDIC硬件 > CMSIS-DSP > 标准库，CORDIC模式下更高效
 */
void BSP_Math_SinCos(float theta, float *p_sin, float *p_cos);

/**
 * @brief 计算atan2(y, x)
 * @param y Y坐标
 * @param x X坐标
 * @return atan2(y, x)（弧度）
 * @note 优先级：CORDIC硬件 > CMSIS-DSP > 标准库
 */
float BSP_Math_Atan2(float y, float x);

/**
 * @brief 计算sqrt(x)
 * @param x 输入值（必须>=0）
 * @return sqrt(x)
 * @note 优先级：CORDIC硬件 > CMSIS-DSP > 标准库
 */
float BSP_Math_Sqrt(float x);

/*============================================
 *              CRC 计算接口
 *============================================*/

#if HAS_CRC
/**
 * @brief CRC计算配置结构体
 */
typedef struct
{
    uint32_t init_value; // 初始值
    uint32_t poly_size;  // 多项式宽度: 7, 8, 16, 32
    uint32_t poly;       // 生成多项式
    uint8_t reverse_in;  // 输入反转
    uint8_t reverse_out; // 输出反转
} BSP_CRC_Config_t;

/**
 * @brief CRC7计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC7校验值
 */
uint8_t BSP_Math_CRC7(const uint8_t *data, uint32_t len, uint8_t init_val);

/**
 * @brief CRC8计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC8校验值
 */
uint8_t BSP_Math_CRC8(const uint8_t *data, uint32_t len, uint8_t init_val);

/**
 * @brief CRC16计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC16校验值
 */
uint16_t BSP_Math_CRC16(const uint8_t *data, uint32_t len, uint16_t init_val);

/**
 * @brief CRC32计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC32校验值
 */
uint32_t BSP_Math_CRC32(const uint8_t *data, uint32_t len, uint32_t init_val);

/**
 * @brief 自定义CRC计算
 * @param config CRC配置
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC校验值
 */
uint32_t BSP_Math_CRC_Custom(const BSP_CRC_Config_t *config,
                             const uint8_t *data, uint32_t len);
#endif // HAS_CRC

/*============================================
 *              FMAC 滤波器接口
 *============================================*/

#if HAS_FMAC
/**
 * @brief FMAC滤波器类型枚举
 */
typedef enum
{
    FMAC_FILTER_FIR = 0, // FIR滤波器
    FMAC_FILTER_IIR,     // IIR滤波器
} BSP_FMAC_FilterType_e;

/**
 * @brief FMAC滤波器配置结构体
 */
typedef struct
{
    BSP_FMAC_FilterType_e type; // 滤波器类型
    int16_t *p_coeffs;          // 系数数组
    uint16_t coeff_num;         // 系数数量
    uint8_t input_shift;        // 输入移位
    uint8_t output_shift;       // 输出移位
} BSP_FMAC_FilterConfig_t;

/**
 * @brief 初始化FMAC滤波器
 * @param config 滤波器配置
 * @return 0-成功, 负数-失败
 */
int8_t BSP_Math_FMAC_Init(const BSP_FMAC_FilterConfig_t *config);

/**
 * @brief FMAC滤波处理
 * @param input 输入数据
 * @param output 输出数据
 * @param len 数据长度
 * @return 实际处理的样本数
 */
uint16_t BSP_Math_FMAC_Filter(const int16_t *input, int16_t *output, uint16_t len);
#endif // HAS_FMAC

/*============================================
 *              通用数学宏定义
 *============================================*/

/* 常用数学常量 */
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923f
#endif

#ifndef M_E
#define M_E 2.71828182845904523536f
#endif

/* 角度弧度转换 */
#define DEG_TO_RAD(deg) ((deg) * M_PI / 180.0f)
#define RAD_TO_DEG(rad) ((rad) * 180.0f / M_PI)

/* 限制范围 */
#define MATH_CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

/* 符号函数 */
#define MATH_SIGN(x) ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

/* 最大最小值 */
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* 绝对值 */
#ifndef ABS
#define ABS(x) ((x) > 0 ? (x) : -(x))
#endif

/* 平方 */
#define SQ(x) ((x) * (x))

#endif // __BSP_MATH_H

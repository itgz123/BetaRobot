/**
 * @file bsp_math.c
 * @brief BSP层数学加速封装实现
 *
 * @note 三层回退机制：
 *       1. 硬件加速（CORDIC）- 仅部分STM32有（H7/G4系列）
 *       2. CMSIS-DSP库 - 有DSP指令集时可用
 *       3. 标准库math.h - 兜底实现
 */

#include "bsp_math.h"
#include "bsp_log.h"
#include <math.h>
#include <string.h>

/*============================================
 *              条件包含
 *============================================*/

#if HAS_CRC
#include "stm32f4xx_hal_crc.h"
#endif

#if HAS_CORDIC
#include "stm32f4xx_hal_cordic.h"
#endif

#if HAS_FMAC
#include "stm32f4xx_hal_fmac.h"
#endif

#if HAS_CMSIS_DSP
#include "arm_math.h"
#endif

/*============================================
 *              私有变量
 *============================================*/

#if HAS_CRC
static CRC_HandleTypeDef *s_hcrc = NULL;
#endif

#if HAS_CORDIC
static CORDIC_HandleTypeDef *s_hcordic = NULL;
#endif

#if HAS_FMAC
static FMAC_HandleTypeDef *s_hfmac = NULL;
static BSP_FMAC_FilterConfig_t s_fmac_config;
#endif

/*============================================
 *              私有辅助函数
 *============================================*/

#if HAS_CORDIC
/**
 * @brief 角度归一化到[-PI, PI]
 */
static float normalize_angle(float theta)
{
    while (theta > M_PI)
        theta -= 2.0f * M_PI;
    while (theta < -M_PI)
        theta += 2.0f * M_PI;
    return theta;
}
#endif

/*============================================
 *              初始化接口实现
 *============================================*/

void BSP_Math_Init(void)
{
#if HAS_FPU
    /* FPU已在启动代码中由SystemInit启用，此处无需操作 */
    LOGINFO("[bsp_math] FPU enabled");
#endif

#if HAS_CMSIS_DSP
    LOGINFO("[bsp_math] CMSIS-DSP available");
#endif

#if HAS_CORDIC
    if (s_hcordic != NULL)
    {
        /* CORDIC默认配置：三角函数模式 */
        CORDIC_ConfigTypeDef cordic_config = {0};
        cordic_config.Function = CORDIC_FUNCTION_SINE;
        cordic_config.Precision = CORDIC_PRECISION_6CYCLES;
        cordic_config.Scale = CORDIC_SCALE_0;
        cordic_config.InSize = CORDIC_INSIZE_32BITS;
        cordic_config.OutSize = CORDIC_OUTSIZE_32BITS;
        cordic_config.NbWrite = CORDIC_NBWRITE_1;
        cordic_config.NbRead = CORDIC_NBREAD_1;

        if (HAL_CORDIC_Configure(s_hcordic, &cordic_config) != HAL_OK)
        {
            LOGERROR("[bsp_math] CORDIC config failed");
        }
        else
        {
            LOGINFO("[bsp_math] CORDIC initialized");
        }
    }
#endif

#if HAS_CRC
    if (s_hcrc != NULL)
    {
        LOGINFO("[bsp_math] CRC initialized");
    }
#endif

#if HAS_FMAC
    if (s_hfmac != NULL)
    {
        LOGINFO("[bsp_math] FMAC initialized");
    }
#endif

#if HAS_MPU
    /* MPU配置通常在启动代码中完成，此处仅记录 */
    LOGINFO("[bsp_math] MPU available");
#endif

#if HAS_RAMECC
    LOGINFO("[bsp_math] RAMECC available");
#endif
}

/*============================================
 *              三角函数接口实现
 *          三层回退：CORDIC -> CMSIS-DSP -> 标准库
 *============================================*/

float BSP_Math_Sin(float theta)
{
#if HAS_CORDIC
    /* 第一层：CORDIC硬件加速 */
    if (s_hcordic != NULL)
    {
        theta = normalize_angle(theta);

        CORDIC_ConfigTypeDef config = {0};
        config.Function = CORDIC_FUNCTION_SINE;
        config.Precision = CORDIC_PRECISION_6CYCLES;
        config.Scale = CORDIC_SCALE_0;
        config.InSize = CORDIC_INSIZE_32BITS;
        config.OutSize = CORDIC_OUTSIZE_32BITS;
        config.NbWrite = CORDIC_NBWRITE_1;
        config.NbRead = CORDIC_NBREAD_1;

        HAL_CORDIC_Configure(s_hcordic, &config);

        int32_t input = (int32_t)(theta / M_PI * 2147483648.0f);
        int32_t output = 0;

        HAL_CORDIC_Calculate(s_hcordic, &input, &output, 1, HAL_MAX_DELAY);

        return (float)output / 2147483648.0f;
    }
#endif

#if HAS_CMSIS_DSP
    /* 第二层：CMSIS-DSP快速逼近 */
    return arm_sin_f32(theta);
#endif

    /* 第三层：标准库 */
    return sinf(theta);
}

float BSP_Math_Cos(float theta)
{
#if HAS_CORDIC
    /* 第一层：CORDIC硬件加速 */
    if (s_hcordic != NULL)
    {
        theta = normalize_angle(theta);

        CORDIC_ConfigTypeDef config = {0};
        config.Function = CORDIC_FUNCTION_COSINE;
        config.Precision = CORDIC_PRECISION_6CYCLES;
        config.Scale = CORDIC_SCALE_0;
        config.InSize = CORDIC_INSIZE_32BITS;
        config.OutSize = CORDIC_OUTSIZE_32BITS;
        config.NbWrite = CORDIC_NBWRITE_1;
        config.NbRead = CORDIC_NBREAD_1;

        HAL_CORDIC_Configure(s_hcordic, &config);

        int32_t input = (int32_t)(theta / M_PI * 2147483648.0f);
        int32_t output = 0;

        HAL_CORDIC_Calculate(s_hcordic, &input, &output, 1, HAL_MAX_DELAY);

        return (float)output / 2147483648.0f;
    }
#endif

#if HAS_CMSIS_DSP
    /* 第二层：CMSIS-DSP快速逼近 */
    return arm_cos_f32(theta);
#endif

    /* 第三层：标准库 */
    return cosf(theta);
}

void BSP_Math_SinCos(float theta, float *p_sin, float *p_cos)
{
    if (p_sin == NULL || p_cos == NULL)
    {
        return;
    }

#if HAS_CORDIC
    /* 第一层：CORDIC硬件加速（同时计算sin和cos，效率最高） */
    if (s_hcordic != NULL)
    {
        theta = normalize_angle(theta);

        CORDIC_ConfigTypeDef config = {0};
        config.Function = CORDIC_FUNCTION_SINE;
        config.Precision = CORDIC_PRECISION_6CYCLES;
        config.Scale = CORDIC_SCALE_0;
        config.InSize = CORDIC_INSIZE_32BITS;
        config.OutSize = CORDIC_OUTSIZE_32BITS;
        config.NbWrite = CORDIC_NBWRITE_1;
        config.NbRead = CORDIC_NBREAD_2; /* 同时读取sin和cos */

        HAL_CORDIC_Configure(s_hcordic, &config);

        int32_t input = (int32_t)(theta / M_PI * 2147483648.0f);
        int32_t output[2] = {0};

        HAL_CORDIC_Calculate(s_hcordic, &input, output, 1, HAL_MAX_DELAY);

        *p_sin = (float)output[0] / 2147483648.0f;
        *p_cos = (float)output[1] / 2147483648.0f;
        return;
    }
#endif

#if HAS_CMSIS_DSP
    /* 第二层：CMSIS-DSP快速逼近 */
    *p_sin = arm_sin_f32(theta);
    *p_cos = arm_cos_f32(theta);
    return;
#endif

    /* 第三层：标准库 */
    *p_sin = sinf(theta);
    *p_cos = cosf(theta);
}

float BSP_Math_Atan2(float y, float x)
{
#if HAS_CORDIC
    /* 第一层：CORDIC硬件加速 */
    if (s_hcordic != NULL)
    {
        CORDIC_ConfigTypeDef config = {0};
        config.Function = CORDIC_FUNCTION_PHASE;
        config.Precision = CORDIC_PRECISION_6CYCLES;
        config.Scale = CORDIC_SCALE_0;
        config.InSize = CORDIC_INSIZE_32BITS;
        config.OutSize = CORDIC_OUTSIZE_32BITS;
        config.NbWrite = CORDIC_NBWRITE_2;
        config.NbRead = CORDIC_NBREAD_1;

        HAL_CORDIC_Configure(s_hcordic, &config);

        float mag = sqrtf(x * x + y * y);
        if (mag < 1e-10f)
        {
            return 0.0f;
        }

        int32_t input[2];
        input[0] = (int32_t)(x / mag * 2147483647.0f);
        input[1] = (int32_t)(y / mag * 2147483647.0f);
        int32_t output = 0;

        HAL_CORDIC_Calculate(s_hcordic, input, &output, 1, HAL_MAX_DELAY);

        return (float)output / 2147483648.0f;
    }
#endif

#if HAS_CMSIS_DSP
    /* 第二层：CMSIS-DSP */
    float result;
    arm_atan2_f32(y, x, &result);
    return result;
#endif

    /* 第三层：标准库 */
    return atan2f(y, x);
}

float BSP_Math_Sqrt(float x)
{
    if (x < 0.0f)
    {
        return 0.0f;
    }

#if HAS_CORDIC
    /* 第一层：CORDIC硬件加速 */
    if (s_hcordic != NULL)
    {
        CORDIC_ConfigTypeDef config = {0};
        config.Function = CORDIC_FUNCTION_SQRT;
        config.Precision = CORDIC_PRECISION_6CYCLES;
        config.Scale = CORDIC_SCALE_0;
        config.InSize = CORDIC_INSIZE_32BITS;
        config.OutSize = CORDIC_OUTSIZE_32BITS;
        config.NbWrite = CORDIC_NBWRITE_1;
        config.NbRead = CORDIC_NBREAD_1;

        HAL_CORDIC_Configure(s_hcordic, &config);

        float scale = 1.0f;
        while (x >= 1.0f)
        {
            x *= 0.25f;
            scale *= 2.0f;
        }
        while (x < 0.5f && x > 0.0f)
        {
            x *= 4.0f;
            scale *= 0.5f;
        }

        int32_t input = (int32_t)(x * 2147483647.0f);
        int32_t output = 0;

        HAL_CORDIC_Calculate(s_hcordic, &input, &output, 1, HAL_MAX_DELAY);

        return (float)output / 2147483647.0f * scale;
    }
#endif

#if HAS_CMSIS_DSP
    /* 第二层：CMSIS-DSP（使用VSQRT指令或优化实现） */
    float result;
    arm_sqrt_f32(x, &result);
    return result;
#endif

    /* 第三层：标准库 */
    return sqrtf(x);
}

/*============================================
 *              CRC 接口实现
 *============================================*/

#if HAS_CRC

uint8_t BSP_Math_CRC7(const uint8_t *data, uint32_t len, uint8_t init_val)
{
    if (data == NULL || len == 0)
    {
        return init_val;
    }

    if (s_hcrc == NULL)
    {
        /* 软件回退实现：CRC-7/MMC */
        uint8_t crc = init_val;
        for (uint32_t i = 0; i < len; i++)
        {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 0x80)
                    crc = (crc << 1) ^ 0x12; /* x^7 + x^3 + 1 */
                else
                    crc <<= 1;
            }
        }
        return crc & 0x7F;
    }

    /* 硬件CRC实现 */
    /* 注意：STM32硬件CRC通常不支持CRC-7，此处仍使用软件实现 */
    uint8_t crc = init_val;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x12;
            else
                crc <<= 1;
        }
    }
    return crc & 0x7F;
}

uint8_t BSP_Math_CRC8(const uint8_t *data, uint32_t len, uint8_t init_val)
{
    if (data == NULL || len == 0)
    {
        return init_val;
    }

    if (s_hcrc == NULL)
    {
        /* 软件回退实现：CRC-8/MAXIM */
        uint8_t crc = init_val;
        for (uint32_t i = 0; i < len; i++)
        {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 0x80)
                    crc = (crc << 1) ^ 0x31; /* x^8 + x^5 + x^4 + 1 */
                else
                    crc <<= 1;
            }
        }
        return crc;
    }

    /* 硬件CRC实现 */
    /* STM32 CRC默认为CRC-32，需要重新配置 */
    __HAL_CRC_DR_RESET(s_hcrc);

    /* 使用8位写入 */
    for (uint32_t i = 0; i < len; i++)
    {
        *(volatile uint8_t *)&s_hcrc->Instance->DR = data[i];
    }

    return (uint8_t)s_hcrc->Instance->DR;
}

uint16_t BSP_Math_CRC16(const uint8_t *data, uint32_t len, uint16_t init_val)
{
    if (data == NULL || len == 0)
    {
        return init_val;
    }

    if (s_hcrc == NULL)
    {
        /* 软件回退实现：CRC-16/CCITT */
        uint16_t crc = init_val;
        for (uint32_t i = 0; i < len; i++)
        {
            crc ^= (uint16_t)data[i] << 8;
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 0x8000)
                    crc = (crc << 1) ^ 0x1021; /* x^16 + x^12 + x^5 + 1 */
                else
                    crc <<= 1;
            }
        }
        return crc;
    }

    /* 硬件CRC实现 */
    __HAL_CRC_DR_RESET(s_hcrc);

    /* 16位写入需要处理字节对齐 */
    for (uint32_t i = 0; i + 1 < len; i += 2)
    {
        uint16_t word = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
        *(volatile uint16_t *)&s_hcrc->Instance->DR = word;
    }

    /* 处理剩余字节 */
    if (len & 1)
    {
        *(volatile uint8_t *)&s_hcrc->Instance->DR = data[len - 1];
    }

    return (uint16_t)s_hcrc->Instance->DR;
}

uint32_t BSP_Math_CRC32(const uint8_t *data, uint32_t len, uint32_t init_val)
{
    if (data == NULL || len == 0)
    {
        return init_val;
    }

    if (s_hcrc == NULL)
    {
        /* 软件回退实现：CRC-32 */
        uint32_t crc = init_val;
        for (uint32_t i = 0; i < len; i++)
        {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320; /* CRC-32多项式 */
                else
                    crc >>= 1;
            }
        }
        return crc ^ 0xFFFFFFFF;
    }

    /* 硬件CRC实现 */
    __HAL_CRC_DR_RESET(s_hcrc);

    /* 32位写入 */
    for (uint32_t i = 0; i + 3 < len; i += 4)
    {
        uint32_t word = (uint32_t)data[i] |
                        ((uint32_t)data[i + 1] << 8) |
                        ((uint32_t)data[i + 2] << 16) |
                        ((uint32_t)data[i + 3] << 24);
        s_hcrc->Instance->DR = word;
    }

    /* 处理剩余字节 */
    uint32_t remaining = len & 3;
    if (remaining > 0)
    {
        uint32_t last = 0;
        uint32_t offset = len - remaining;
        for (uint32_t i = 0; i < remaining; i++)
        {
            last |= (uint32_t)data[offset + i] << (i * 8);
        }
        s_hcrc->Instance->DR = last;
    }

    return s_hcrc->Instance->DR;
}

uint32_t BSP_Math_CRC_Custom(const BSP_CRC_Config_t *config,
                             const uint8_t *data, uint32_t len)
{
    if (config == NULL || data == NULL || len == 0)
    {
        return 0;
    }

    /* 自定义CRC通常使用软件实现 */
    uint32_t crc = config->init_value;
    uint8_t poly_size = config->poly_size;

    for (uint32_t i = 0; i < len; i++)
    {
        uint8_t byte = data[i];

        if (config->reverse_in)
        {
            byte = ((byte & 0xF0) >> 4) | ((byte & 0x0F) << 4);
            byte = ((byte & 0xCC) >> 2) | ((byte & 0x33) << 2);
            byte = ((byte & 0xAA) >> 1) | ((byte & 0x55) << 1);
        }

        switch (poly_size)
        {
        case 7:
            crc ^= (uint32_t)byte;
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 0x40)
                    crc = (crc << 1) ^ config->poly;
                else
                    crc <<= 1;
            }
            crc &= 0x7F;
            break;

        case 8:
            crc ^= (uint32_t)byte;
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 0x80)
                    crc = (crc << 1) ^ config->poly;
                else
                    crc <<= 1;
            }
            crc &= 0xFF;
            break;

        case 16:
            crc ^= (uint32_t)byte << 8;
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 0x8000)
                    crc = (crc << 1) ^ config->poly;
                else
                    crc <<= 1;
            }
            crc &= 0xFFFF;
            break;

        case 32:
            crc ^= (uint32_t)byte << 24;
            for (uint8_t j = 0; j < 8; j++)
            {
                if (crc & 0x80000000)
                    crc = (crc << 1) ^ config->poly;
                else
                    crc <<= 1;
            }
            break;

        default:
            break;
        }
    }

    if (config->reverse_out)
    {
        if (poly_size <= 8)
        {
            uint8_t tmp = (uint8_t)crc;
            tmp = ((tmp & 0xF0) >> 4) | ((tmp & 0x0F) << 4);
            tmp = ((tmp & 0xCC) >> 2) | ((tmp & 0x33) << 2);
            tmp = ((tmp & 0xAA) >> 1) | ((tmp & 0x55) << 1);
            crc = tmp;
        }
        else if (poly_size <= 16)
        {
            uint16_t tmp = (uint16_t)crc;
            tmp = ((tmp & 0xFF00) >> 8) | ((tmp & 0x00FF) << 8);
            tmp = ((tmp & 0xF0F0) >> 4) | ((tmp & 0x0F0F) << 4);
            tmp = ((tmp & 0xCCCC) >> 2) | ((tmp & 0x3333) << 2);
            tmp = ((tmp & 0xAAAA) >> 1) | ((tmp & 0x5555) << 1);
            crc = tmp;
        }
    }

    return crc;
}

#endif /* HAS_CRC */

/*============================================
 *              FMAC 接口实现
 *============================================*/

#if HAS_FMAC

int8_t BSP_Math_FMAC_Init(const BSP_FMAC_FilterConfig_t *config)
{
    if (config == NULL || s_hfmac == NULL)
    {
        return -1;
    }

    memcpy(&s_fmac_config, config, sizeof(BSP_FMAC_FilterConfig_t));

    FMAC_FilterConfigTypeDef fmac_conf = {0};

    if (config->type == FMAC_FILTER_FIR)
    {
        fmac_conf.InputBaseAddress = 0;
        fmac_conf.InputBufferSize = 64; /* 输入缓冲区大小 */
        fmac_conf.InputThreshold = FMAC_THRESHOLD_1;
        fmac_conf.CoeffBaseAddress = 64;
        fmac_conf.CoeffBufferSize = config->coeff_num;
        fmac_conf.OutputBaseAddress = 64 + config->coeff_num;
        fmac_conf.OutputBufferSize = 64;
        fmac_conf.OutputThreshold = FMAC_THRESHOLD_1;
        fmac_conf.pCoeffA = config->p_coeffs;
        fmac_conf.CoeffASize = config->coeff_num;
        fmac_conf.Filter = FMAC_FUNC_LOAD;
    }
    else /* FMAC_FILTER_IIR */
    {
        fmac_conf.InputBaseAddress = 0;
        fmac_conf.InputBufferSize = 64;
        fmac_conf.InputThreshold = FMAC_THRESHOLD_1;
        fmac_conf.CoeffBaseAddress = 64;
        fmac_conf.CoeffBufferSize = config->coeff_num;
        fmac_conf.OutputBaseAddress = 64 + config->coeff_num;
        fmac_conf.OutputBufferSize = 64;
        fmac_conf.OutputThreshold = FMAC_THRESHOLD_1;
        fmac_conf.pCoeffA = config->p_coeffs;
        fmac_conf.CoeffASize = config->coeff_num / 2;
        fmac_conf.pCoeffB = config->p_coeffs + config->coeff_num / 2;
        fmac_conf.CoeffBSize = config->coeff_num / 2;
        fmac_conf.Filter = FMAC_FUNC_LOAD;
    }

    fmac_conf.InputAccess = FMAC_BUFFER_ACCESS_NONE;
    fmac_conf.OutputAccess = FMAC_BUFFER_ACCESS_NONE;
    fmac_conf.CoeffAFormat = FMAC_FORMAT_Q1_15;
    fmac_conf.CoeffBFormat = FMAC_FORMAT_Q1_15;
    fmac_conf.InputShift = config->input_shift;
    fmac_conf.OutputShift = config->output_shift;
    fmac_conf.Accumulator = 0;
    fmac_conf.P = config->coeff_num;
    fmac_conf.Q = 0;
    fmac_conf.R = 0;

    if (HAL_FMAC_FilterConfig(s_hfmac, &fmac_conf) != HAL_OK)
    {
        LOGERROR("[bsp_math] FMAC config failed");
        return -2;
    }

    LOGINFO("[bsp_math] FMAC filter initialized");
    return 0;
}

uint16_t BSP_Math_FMAC_Filter(const int16_t *input, int16_t *output, uint16_t len)
{
    if (input == NULL || output == NULL || len == 0 || s_hfmac == NULL)
    {
        return 0;
    }

    if (HAL_FMAC_FilterStart(s_hfmac, output, NULL) != HAL_OK)
    {
        LOGERROR("[bsp_math] FMAC start failed");
        return 0;
    }

    uint16_t processed = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        if (HAL_FMAC_AppendFilterData(s_hfmac, &input[i], 1) != HAL_OK)
        {
            break;
        }
        processed++;
    }

    /* 等待处理完成 */
    HAL_FMAC_PollFilterData(s_hfmac, len, HAL_MAX_DELAY);

    HAL_FMAC_FilterStop(s_hfmac);

    return processed;
}

#endif /* HAS_FMAC */

/*============================================
 *              句柄注册接口
 *============================================*/

#if HAS_CRC
/**
 * @brief 注册CRC句柄
 * @param hcrc CRC外设句柄
 */
void BSP_Math_RegisterCRC(CRC_HandleTypeDef *hcrc)
{
    s_hcrc = hcrc;
}
#endif

#if HAS_CORDIC
/**
 * @brief 注册CORDIC句柄
 * @param hcordic CORDIC外设句柄
 */
void BSP_Math_RegisterCORDIC(CORDIC_HandleTypeDef *hcordic)
{
    s_hcordic = hcordic;
}
#endif

#if HAS_FMAC
/**
 * @brief 注册FMAC句柄
 * @param hfmac FMAC外设句柄
 */
void BSP_Math_RegisterFMAC(FMAC_HandleTypeDef *hfmac)
{
    s_hfmac = hfmac;
}
#endif
/**
 * @file bsp_math.c
 * @brief BSP层数学函数封装实现
 *
 * @note 三角函数已在 bsp_math.h 中使用 static inline 实现
 *       本文件仅包含初始化和 CRC 相关函数
 */

#include "bsp_math.h"
#include "bsp_log.h"
#include <string.h>

/*============================================
 *              条件包含
 *============================================*/

#ifdef HAL_CRC_MODULE_ENABLED
#if DEVELOPMENT_BOARD == DM_MC02
#include "stm32h7xx_hal_crc.h"
#elif DEVELOPMENT_BOARD == DJI_A
#include "stm32f4xx_hal_crc.h"
#else
#include "stm32f4xx_hal_crc.h"
#endif
#endif

/*============================================
 *              私有变量
 *============================================*/

#ifdef HAL_CRC_MODULE_ENABLED
static CRC_HandleTypeDef *s_hcrc = NULL;
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

#if HAS_DSP
    LOGINFO("[bsp_math] CMSIS-DSP available");
#endif

#ifdef HAL_CRC_MODULE_ENABLED
    if (s_hcrc != NULL)
    {
        LOGINFO("[bsp_math] CRC initialized");
    }
#endif
}

/*============================================
 *              CRC 接口实现
 *============================================*/

#ifdef HAL_CRC_MODULE_ENABLED

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
        uint32_t word = (uint32_t)data[i] | ((uint32_t)data[i + 1] << 8) | ((uint32_t)data[i + 2] << 16) | ((uint32_t)data[i + 3] << 24);
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

uint32_t BSP_Math_CRC_Custom(const BSP_CRC_Config_t *config, const uint8_t *data, uint32_t len)
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

#endif /* HAL_CRC_MODULE_ENABLED */

/*============================================
 *              句柄注册接口
 *============================================*/

#ifdef HAL_CRC_MODULE_ENABLED
/**
 * @brief 注册CRC句柄
 * @param hcrc CRC外设句柄
 */
void BSP_Math_RegisterCRC(CRC_HandleTypeDef *hcrc)
{
    s_hcrc = hcrc;
}
#endif
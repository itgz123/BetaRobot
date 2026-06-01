/**
 * @file bsp_crc.c
 * @brief BSP层CRC计算封装实现（纯软件实现）
 */

#include "bsp_crc.h"
#include <string.h>

/*============================================
 *              CRC 接口实现
 *============================================*/

uint8_t BSP_CRC7(const uint8_t *data, uint32_t len, uint8_t init_val)
{
    if (data == NULL || len == 0)
    {
        return init_val;
    }

    /* CRC-7/MMC */
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

uint8_t BSP_CRC8(const uint8_t *data, uint32_t len, uint8_t init_val)
{
    if (data == NULL || len == 0)
    {
        return init_val;
    }

    /* CRC-8/MAXIM */
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

uint16_t BSP_CRC16(const uint8_t *data, uint32_t len, uint16_t init_val)
{
    if (data == NULL || len == 0)
    {
        return init_val;
    }

    /* CRC-16/CCITT */
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

uint32_t BSP_CRC32(const uint8_t *data, uint32_t len, uint32_t init_val)
{
    if (data == NULL || len == 0)
    {
        return init_val;
    }

    /* CRC-32 */
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

uint32_t BSP_CRC_Custom(const BSP_CRC_Config_t *config, const uint8_t *data, uint32_t len)
{
    if (config == NULL || data == NULL || len == 0)
    {
        return 0;
    }

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

/**
 * @file bsp_crc.h
 * @brief BSP层CRC计算封装
 *
 * @note 提供常用CRC算法的纯软件实现：
 *       - CRC-7/MMC
 *       - CRC-8/MAXIM
 *       - CRC-16/CCITT
 *       - CRC-32
 *       - 自定义CRC（支持7/8/16/32位多项式）
 */

#ifndef __BSP_CRC_H
#define __BSP_CRC_H

#include <stdint.h>

/*============================================
 *              CRC 计算接口（纯软件实现）
 *============================================*/

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
uint8_t BSP_CRC7(const uint8_t *data, uint32_t len, uint8_t init_val);

/**
 * @brief CRC8计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC8校验值
 */
uint8_t BSP_CRC8(const uint8_t *data, uint32_t len, uint8_t init_val);

/**
 * @brief CRC16计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC16校验值
 */
uint16_t BSP_CRC16(const uint8_t *data, uint32_t len, uint16_t init_val);

/**
 * @brief CRC32计算
 * @param data 数据指针
 * @param len 数据长度
 * @param init_val 初始值
 * @return CRC32校验值
 */
uint32_t BSP_CRC32(const uint8_t *data, uint32_t len, uint32_t init_val);

/**
 * @brief 自定义CRC计算
 * @param config CRC配置
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC校验值
 */
uint32_t BSP_CRC_Custom(const BSP_CRC_Config_t *config, const uint8_t *data, uint32_t len);

#endif // __BSP_CRC_H

/**
 * @file lib_crc.c
 * @brief lib层CRC计算封装实现（逐位直接计算 / 查表驱动）
 *
 * 算法语义（crcmod 对齐，经标准 check 向量验证）：
 *   - 直接计算 LIB_CRC_Direct：逐位，支持 7/8/16/32 位
 *       * 7 位（CRC-7/MMC）：width+1 位（8 位）寄存器逐位，多项式左移 1 位，结果取高 7 位
 *       * 8/16/32 位：reverse_in=0 用非反射逐位（MSB-first，输入放寄存器高位左移），
 *         =1 用反射逐位（LSB-first 右移，内部取反射多项式）
 *   - 查表 LIB_CRC_TableCalc：每字节 1 次查表替代 8 次位循环，表可为 Flash const 或 RAM 生成
 *   - reverse_in != reverse_out 时结果按位反转（RefOut），始终与 xor_out 异或后返回
 *
 * Flash 内置表见 lib_crc_tables.c（lib_crc_tables_gen.py 生成），与本文件的 LIB_CRC_GenTable
 * 逐位逻辑完全一致，二者可混用。
 */

#include "lib_crc.h"
#include "app_cfg.h"

#ifdef LIB_CRC_USED

/* 位反转：把 value 的低 width 位整体反转（RefIn 反射多项式 / RefOut 反射结果用） */
static uint32_t CRC_BitReflect(uint32_t value, uint8_t width)
{
    uint32_t reflected = 0;

    for (uint8_t i = 0; i < width; i++)
    {
        reflected = (reflected << 1) | (value & 1u);
        value >>= 1;
    }
    return reflected;
}

/**
 * @brief 逐位直接计算（可算任意算法，兜底层，无需表）
 * @see LIB_CRC_Direct
 */
uint32_t LIB_CRC_Direct(const LIB_CRC_Algo_t *algo, const uint8_t *data, uint32_t len)
{
    uint32_t width;
    uint32_t mask;
    uint32_t crc;
    uint32_t poly;

    if (algo == NULL || data == NULL)
        return 0;

    width = algo->poly_size;
    if (width != 7 && width != 8 && width != 16 && width != 32)
        return 0; /* 非法宽度 */
    mask = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);

    if (width == 7)
    {
        /* CRC-7：width+1 位（8 位）寄存器逐位，多项式左移 1 位参与运算，结果取高 7 位 */
        crc = algo->init_value & 0x7F;
        poly = (algo->poly & 0x7F) << 1;
        for (uint32_t i = 0; i < len; i++)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; bit++)
                crc = (crc & 0x80) ? (uint32_t)(((crc << 1) ^ poly) & 0xFF) : (uint32_t)((crc << 1) & 0xFF);
        }
        crc = (crc >> 1) & 0x7F;
    }
    else if (algo->reverse_in)
    {
        /* 反射逐位：LSB-first，寄存器右移，使用反射多项式 */
        poly = CRC_BitReflect(algo->poly & mask, (uint8_t)width);
        crc = algo->init_value & mask;
        for (uint32_t i = 0; i < len; i++)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; bit++)
                crc = (crc & 1u) ? (uint32_t)((crc >> 1) ^ poly) : (crc >> 1);
            crc &= mask;
        }
    }
    else
    {
        /* 非反射逐位：MSB-first，输入字节放寄存器高位，左移 */
        poly = algo->poly & mask;
        crc = algo->init_value & mask;
        for (uint32_t i = 0; i < len; i++)
        {
            crc ^= data[i] << (width - 8);
            for (uint8_t bit = 0; bit < 8; bit++)
                crc = (crc & (1u << (width - 1))) ? (uint32_t)(((crc << 1) ^ poly) & mask) : (uint32_t)((crc << 1) & mask);
        }
    }

    /* 输出反转（RefOut）：refin 与 refout 不同向时结果按位反转（crcmod 语义） */
    if (algo->reverse_in != algo->reverse_out)
        crc = CRC_BitReflect(crc, (uint8_t)width);

    /* 结果异或（XorOut） */
    return (crc ^ algo->xor_out) & mask;
}

/**
 * @brief 生成 256 项查表到 table[256]（RAM）
 * @see LIB_CRC_GenTable
 */
int8_t LIB_CRC_GenTable(const LIB_CRC_Algo_t *algo, uint32_t table[256])
{
    uint32_t width;
    uint32_t poly;

    /* 校验表与算法匹配：仅 8/16/32 位支持逐字节查表（7 位须走 LIB_CRC_Direct） */
    if (algo == NULL || table == NULL)
        return -1;
    width = algo->poly_size;
    if (width != 8 && width != 16 && width != 32)
        return -1;

    poly = algo->poly & ((width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u));
    if (algo->reverse_in)
        poly = CRC_BitReflect(poly, (uint8_t)width);

    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t crc;

        if (algo->reverse_in)
        {
            /* 反射表：i 作为 8 位输入，LSB-first 右移 8 次 */
            crc = i;
            for (uint8_t bit = 0; bit < 8; bit++)
                crc = (crc & 1u) ? (uint32_t)((crc >> 1) ^ poly) : (crc >> 1);
        }
        else
        {
            /* 非反射表：i 放寄存器高位，MSB-first 左移 8 次 */
            crc = i << (width - 8);
            for (uint8_t bit = 0; bit < 8; bit++)
                crc = (crc & (1u << (width - 1))) ? (uint32_t)(((crc << 1) ^ poly)) : (uint32_t)(crc << 1);
        }
        table[i] = crc;
    }
    return 0;
}

/**
 * @brief 通用查表计算（Flash 表或 RAM 生成表通用）
 * @see LIB_CRC_TableCalc
 */
uint32_t LIB_CRC_TableCalc(const LIB_CRC_Table_t *tbl, const uint8_t *data, uint32_t len)
{
    const LIB_CRC_Algo_t *algo;
    const uint32_t *table;
    uint32_t width;
    uint32_t mask;
    uint32_t crc;

    if (tbl == NULL || tbl->algo == NULL || tbl->table == NULL || data == NULL)
        return 0;

    algo = tbl->algo;
    width = algo->poly_size;

    /* 7 位/非法宽度不支持查表：自动降级为逐位直接计算 */
    if (width == 7 || (width != 8 && width != 16 && width != 32))
        return LIB_CRC_Direct(algo, data, len);

    table = tbl->table;
    mask = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);

    crc = algo->init_value & mask;
    if (algo->reverse_in)
    {
        /* 反射表：每字节 1 次查表（LSB-first） */
        for (uint32_t i = 0; i < len; i++)
            crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xFFu];
    }
    else
    {
        /* 非反射表：每字节 1 次查表（MSB-first） */
        for (uint32_t i = 0; i < len; i++)
            crc = (crc << 8) ^ table[((crc >> (width - 8)) ^ data[i]) & 0xFFu];
    }
    crc &= mask;

    /* 输出反转（RefOut）：refin 与 refout 不同向时结果按位反转（crcmod 语义） */
    if (algo->reverse_in != algo->reverse_out)
        crc = CRC_BitReflect(crc, (uint8_t)width);

    /* 结果异或（XorOut） */
    return (crc ^ algo->xor_out) & mask;
}

#endif /* LIB_CRC_USED */

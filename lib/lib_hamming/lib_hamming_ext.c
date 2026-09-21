/**
 * @file lib_hamming_ext.c
 * @brief 扩展缩短汉明码（SECDED）实现：复用标准码块函数 + 1 位总校验位
 *
 * 复用点：块编码调 LIB_Hamming_EncodeBlock、块解码调 LIB_Hamming_DecodeBlock（取 syndrome），
 * 位置反查调 LIB_Hamming_PosFromSyndrome —— 本文件不含任何校验矩阵/列值逻辑。
 */

#include "lib_hamming_ext.h"

#ifndef LIB_HAMMING_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_HAMMING_USED

#include <string.h>

/* 单块位缓冲最大字节数：m ≤ 7、k_s ≤ 120 ⇒ 扩展码字 ≤ 128 bit = 16 B */
#define HAMMING_EXT_BLK_BYTES 16u

/* 参数校验（与标准层一致：m ∈ [2,7]，k_s ∈ [1, k]） */
static int Hamming_ExtCfgValid(const LIB_Hamming_Cfg_t *cfg)
{
    if (cfg == NULL)
        return 0;
    if (cfg->m < 2u || cfg->m > LIB_HAMMING_M_MAX)
        return 0;
    if (cfg->k_s < 1u || cfg->k_s > (uint16_t)(((1u << cfg->m) - 1u) - cfg->m))
        return 0;
    return 1;
}

/**
 * @brief 编码任意 bit 长度数据（扩展缩短汉明码）
 * @see LIB_Hamming_ExtEncode
 */
int8_t LIB_Hamming_ExtEncode(const uint8_t *data, uint32_t data_bits, const LIB_Hamming_Cfg_t *cfg,
                             uint8_t *code, uint32_t code_cap_bits, uint32_t *code_bits)
{
    uint32_t blocks;
    uint32_t required;
    uint32_t n_s;
    uint32_t n_e;
    uint32_t b;

    if (data == NULL || code == NULL || code_bits == NULL || !Hamming_ExtCfgValid(cfg))
        return -1;
    if (data_bits == 0u)
        return -1;

    n_s = (uint32_t)cfg->k_s + cfg->m; /* 标准码字位数 */
    n_e = n_s + 1u;                    /* 扩展码字位数（+总校验位） */
    blocks = (data_bits + cfg->k_s - 1u) / cfg->k_s;
    if (blocks > (0xFFFFFFFFu / n_e))
        return -1; /* 长度溢出 */
    required = blocks * n_e;
    if (code_cap_bits < required)
        return -2; /* 缓冲不足：不写任何数据 */

    memset(code, 0, (size_t)((required + 7u) / 8u));

    for (b = 0; b < blocks; b++)
    {
        uint8_t blk_data[HAMMING_EXT_BLK_BYTES];
        uint8_t blk_code[HAMMING_EXT_BLK_BYTES];
        uint8_t parity = 0;
        uint32_t base = b * cfg->k_s;
        uint32_t have = (data_bits > base) ? (data_bits - base) : 0u;
        uint32_t i;

        if (have > cfg->k_s)
            have = cfg->k_s;

        memset(blk_data, 0, sizeof(blk_data));
        for (i = 0; i < have; i++)
        {
            LIB_Hamming_BitSet(blk_data, i, LIB_Hamming_BitGet(data, base + i));
        }

        /* 复用标准码块编码 */
        if (LIB_Hamming_EncodeBlock(blk_data, cfg, blk_code) != LIB_HAMMING_OK)
            return -1;

        for (i = 0; i < n_s; i++)
        {
            uint8_t bit = LIB_Hamming_BitGet(blk_code, i);

            LIB_Hamming_BitSet(code, b * n_e + i, bit);
            parity ^= bit; /* 总校验位 = 标准码字全部位的异或 */
        }
        LIB_Hamming_BitSet(code, b * n_e + n_s, parity);
    }

    *code_bits = required;
    return 0;
}

/**
 * @brief 解码任意 bit 长度数据（扩展缩短汉明码，纠 1 检 2）
 * @see LIB_Hamming_ExtDecode
 */
int8_t LIB_Hamming_ExtDecode(const uint8_t *code, uint32_t code_bits, uint32_t data_bits,
                             const LIB_Hamming_Cfg_t *cfg, uint8_t *data, LIB_Hamming_Stat_t *stat)
{
    uint32_t blocks;
    uint32_t required;
    uint32_t n_s;
    uint32_t n_e;
    uint32_t b;

    if (code == NULL || data == NULL || !Hamming_ExtCfgValid(cfg))
        return -1;
    if (data_bits == 0u)
        return -1;

    n_s = (uint32_t)cfg->k_s + cfg->m;
    n_e = n_s + 1u;
    blocks = (data_bits + cfg->k_s - 1u) / cfg->k_s;
    if (blocks > (0xFFFFFFFFu / n_e))
        return -1;
    required = blocks * n_e;
    if (code_bits != required)
        return -1; /* 防错位解码 */

    memset(data, 0, (size_t)((data_bits + 7u) / 8u));
    if (stat != NULL)
    {
        stat->blocks = blocks;
        stat->corrected = 0;
        stat->detected = 0;
        stat->first_err_pos = -1;
    }

    for (b = 0; b < blocks; b++)
    {
        uint8_t blk_code[HAMMING_EXT_BLK_BYTES];
        uint8_t blk_data[HAMMING_EXT_BLK_BYTES];
        uint8_t syn = 0;
        uint8_t parity = 0;
        int16_t pos = -1;
        LIB_Hamming_Status_t block_st;
        uint32_t base = b * cfg->k_s;
        uint32_t have = (data_bits > base) ? (data_bits - base) : 0u;
        uint32_t i;

        if (have > cfg->k_s)
            have = cfg->k_s;

        /* 取标准部分 + 总校验位 */
        memset(blk_code, 0, sizeof(blk_code));
        for (i = 0; i < n_s; i++)
        {
            uint8_t bit = LIB_Hamming_BitGet(code, b * n_e + i);

            LIB_Hamming_BitSet(blk_code, i, bit);
            parity ^= bit;
        }
        parity ^= LIB_Hamming_BitGet(code, b * n_e + n_s);

        /* 复用标准码块解码（syndrome + 位置反查） */
        {
            LIB_Hamming_Status_t st = LIB_Hamming_DecodeBlock(blk_code, cfg, blk_data, &syn, &pos);

            if (syn == 0u && parity == 0u)
            {
                block_st = LIB_HAMMING_OK; /* 无错 */
            }
            else if (syn != 0u && parity != 0u)
            {
                /* 单错（有效列 ⇒ CORRECTED）或 ≥3 位奇数错（无效列 ⇒ DETECTED） */
                block_st = st;
            }
            else if (syn != 0u)
            {
                block_st = LIB_HAMMING_DETECTED; /* ≥2 位错：不可纠，不采信标准层的位置 */
            }
            else
            {
                block_st = LIB_HAMMING_OK; /* 总校验位自身错：数据无错 */
            }
        }

        /* 仅确认为单错并纠正时采用纠错后的数据，否则输出原始位（不改数据） */
        for (i = 0; i < have; i++)
        {
            uint8_t bit = (block_st == LIB_HAMMING_CORRECTED) ? LIB_Hamming_BitGet(blk_data, i)
                                                              : LIB_Hamming_BitGet(blk_code, i);
            LIB_Hamming_BitSet(data, base + i, bit);
        }

        if (stat != NULL)
        {
            if (block_st == LIB_HAMMING_CORRECTED)
            {
                stat->corrected++;
                if (stat->first_err_pos < 0)
                    stat->first_err_pos = (int32_t)(b * n_e) + (int32_t)pos;
            }
            else if (block_st == LIB_HAMMING_DETECTED)
            {
                stat->detected++;
            }
        }
    }
    return 0;
}

#endif /* LIB_HAMMING_USED */

/**
 * @file lib_hamming.c
 * @brief lib层汉明码实现（标准 (2^m-1, 2^m-1-m) 族，系统式，缩短复用同一套块函数）
 *
 * 编码理论（系统式，码字前 k_s 位为数据、后 m 位为校验）：
 *   - 码字位置 i 对应校验矩阵 H 的一列 ColumnOf(i)：
 *       i <  k_s（数据位）→ 所有非零 m 位值里去掉 2 的幂后的第 (i+1) 个（升序 3,5,6,7,9,...）
 *       i >= k_s（校验位 j）→ 1<<j（单位向量 e_j）
 *     所有列互异且非零 ⇒ 最小距离 3 ⇒ 可纠 1 位错。
 *     数据列在实现上预生成为 s_hamming_col 表（m 无关，见该表注释），不再现场穷举。
 *   - 缩短：k_s < k 即只用 H 的前 k_s 个数据列，标准满码是 k_s == k 的特例。
 *   - 编码：acc = ⊕ ColumnOf(i)（取值为 1 的数据位），校验位 parity_j = (acc >> j) & 1
 *     （校验列是 e_j，只出现在自身方程中）。
 *   - 解码：recon = ⊕ ColumnOf(i)（码字全部位置）即 syndrome；为 0 无错，否则由
 *     LIB_Hamming_PosFromSyndrome 反查位置。落在"被缩短掉的数据列"的 syndrome
 *     只可能由 ≥2 位错产生 ⇒ 判不可纠。
 */

#include "lib_hamming.h"

#ifndef LIB_HAMMING_STANDALONE
#include "app_cfg.h"
#endif

#ifdef LIB_HAMMING_USED

#include <string.h>

/* 单块位缓冲最大字节数：m ≤ 7、k_s ≤ 120 ⇒ 码字 ≤ 127 bit = 16 B */
#define HAMMING_BLK_BYTES 16u

/* 满码数据位数 k = 2^m - 1 - m */
static uint16_t Hamming_FullK(uint8_t m)
{
    return (uint16_t)(((1u << m) - 1u) - m);
}

/* 参数校验：m ∈ [2,7]，k_s ∈ [1, k] */
static int Hamming_CfgValid(const LIB_Hamming_Cfg_t *cfg)
{
    if (cfg == NULL)
        return 0;
    if (cfg->m < 2u || cfg->m > LIB_HAMMING_M_MAX)
        return 0;
    if (cfg->k_s < 1u || cfg->k_s > Hamming_FullK(cfg->m))
        return 0;
    return 1;
}

/* 是否 2 的幂（v != 0 前提由调用方保证） */
static uint8_t Hamming_IsPow2(uint8_t v)
{
    return (uint8_t)(v != 0u && (v & (uint8_t)(v - 1u)) == 0u);
}

/* floor(log2(v))，v != 0 */
static uint8_t Hamming_ILog2(uint8_t v)
{
    uint8_t r = 0;

    while (v >>= 1)
    {
        r++;
    }
    return r;
}

/* 尾部 0 的个数，v != 0 */
static uint8_t Hamming_Ctz(uint8_t v)
{
    uint8_t r = 0;

    while (!(v & 1u))
    {
        v >>= 1;
        r++;
    }
    return r;
}

/**
 * @brief H 矩阵的数据列（非 2 的幂的非零 m 位值，升序）
 *
 * @note 这张表对**所有 m** 通用：m 位列值 = "[1, 2^m-1] 内非 2 的幂的值升序"，
 *       而"是否 2 的幂"与 m 无关，故 m=2..7 的列序列互为**前缀**（m=7 最长为 120 项）。
 * @note 早期版本按定义现场穷举（每个位置都从 val=1 重新数一遍非 2 的幂），
 *       复杂度 O(k_s²)：k_s=96 时单次编/解码累计 5229 次内层迭代，在 F407@168MHz
 *       上约 490 µs —— comm 层 2ms 周期里不可接受。改成查表后降到约 32 µs。
 * @note 本表**只做加速、不改语义**：由 test_hamming.c 的穷举往返（全部 m、全部 k_s、
 *       全部单位置错）与旧的"现场穷举"逐项一致。
 */
static const uint8_t s_hamming_col[120] = {
    3,   5,   6,   7,   9,   10,  11,  12,  13,  14,  15,  17,
    18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    30,  31,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,
    43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,
    55,  56,  57,  58,  59,  60,  61,  62,  63,  65,  66,  67,
    68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
    80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,
    92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103,
    104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
    116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127};

/* 表长须正好是最大 m 的满码数据位数 k = 2^m-1-m：改 LIB_HAMMING_M_MAX 时
 * （比如扩到 8，k=247）这里会编译报错，提醒同步扩表，不会静默越界读。 */
_Static_assert(sizeof(s_hamming_col) == LIB_HAMMING_FULL_K(LIB_HAMMING_M_MAX),
               "lib_hamming: 列值表大小须 = 2^LIB_HAMMING_M_MAX - 1 - LIB_HAMMING_M_MAX");

/**
 * @brief 位置 pos 对应的 H 列值
 * @note 数据列直接查表（pos < k_s ≤ 120）；校验列 j 是单位向量 e_j，与表无关。
 *       两点均已由 Hamming_CfgValid 保证：k_s ≤ k ≤ 表长、pos < k_s ⇒ 索引不越界。
 */
static uint8_t Hamming_ColumnOf(uint16_t pos, const LIB_Hamming_Cfg_t *cfg)
{
    if (pos >= cfg->k_s)
    {
        return (uint8_t)(1u << (uint16_t)(pos - cfg->k_s)); /* 校验位 j：单位向量 e_j */
    }
    return s_hamming_col[pos];
}

/**
 * @brief 按数据位数选单块配置（k_s = min(data_bits, k)）
 * @see LIB_Hamming_Fit
 */
int8_t LIB_Hamming_Fit(uint32_t data_bits, uint8_t m, LIB_Hamming_Cfg_t *cfg)
{
    uint16_t k;

    if (cfg == NULL || data_bits == 0u || m < 2u || m > LIB_HAMMING_M_MAX)
        return -1;

    k = Hamming_FullK(m);
    cfg->m = m;
    cfg->k_s = (uint16_t)((data_bits < (uint32_t)k) ? data_bits : k);
    return 0;
}

/**
 * @brief 计算编成码字所需的总位数
 * @see LIB_Hamming_RequiredBits
 */
uint32_t LIB_Hamming_RequiredBits(uint32_t data_bits, const LIB_Hamming_Cfg_t *cfg, uint8_t ext)
{
    uint32_t blocks;
    uint32_t n_s;

    if (!Hamming_CfgValid(cfg) || data_bits == 0u)
        return 0;

    n_s = (uint32_t)cfg->k_s + cfg->m + (ext ? 1u : 0u);
    blocks = (data_bits + cfg->k_s - 1u) / cfg->k_s;

    /* 防溢出：blocks * n_s */
    if (blocks > (0xFFFFFFFFu / n_s))
        return 0;
    return blocks * n_s;
}

/**
 * @brief 对一块数据编码（标准码）
 * @see LIB_Hamming_EncodeBlock
 */
LIB_Hamming_Status_t LIB_Hamming_EncodeBlock(const uint8_t *data, const LIB_Hamming_Cfg_t *cfg,
                                             uint8_t *code)
{
    uint8_t acc = 0;
    uint16_t i;
    uint8_t j;

    if (data == NULL || code == NULL || !Hamming_CfgValid(cfg))
        return LIB_HAMMING_BAD_ARG;

    /* 先清零整块码字（含尾部非整字节位），避免调用方脏缓冲污染 syndrome */
    memset(code, 0, (size_t)(((uint32_t)cfg->k_s + cfg->m + 7u) / 8u));

    /* 数据位原样放到码字前 k_s 位，同时累计 syndrome 构造值 */
    for (i = 0; i < cfg->k_s; i++)
    {
        if (LIB_Hamming_BitGet(data, i))
        {
            LIB_Hamming_BitSet(code, i, 1u);
            acc ^= Hamming_ColumnOf(i, cfg);
        }
    }

    /* 校验位：校验列是 e_j，parity_j 只出现在自身方程 ⇒ 即 acc 的第 j 位 */
    for (j = 0; j < cfg->m; j++)
    {
        LIB_Hamming_BitSet(code, (uint32_t)cfg->k_s + j, (uint8_t)((acc >> j) & 1u));
    }

    return LIB_HAMMING_OK;
}

/**
 * @brief 由 syndrome 反查块内错误位置
 * @see LIB_Hamming_PosFromSyndrome
 */
LIB_Hamming_Status_t LIB_Hamming_PosFromSyndrome(uint8_t syndrome, const LIB_Hamming_Cfg_t *cfg,
                                                 int16_t *pos)
{
    uint16_t p;

    if (!Hamming_CfgValid(cfg) || pos == NULL || syndrome == 0u)
        return LIB_HAMMING_BAD_ARG;

    if (Hamming_IsPow2(syndrome))
    {
        *pos = (int16_t)((uint16_t)cfg->k_s + Hamming_Ctz(syndrome));
        return LIB_HAMMING_OK;
    }

    /* 数据列：0-based 秩 = syndrome - floor(log2 syndrome) - 2 */
    p = (uint16_t)((int)syndrome - (int)Hamming_ILog2(syndrome) - 2);
    if (p >= cfg->k_s)
    {
        *pos = -1; /* 落在被缩短掉的数据列：单错不可能命中 ⇒ 判不可纠 */
        return LIB_HAMMING_DETECTED;
    }
    *pos = (int16_t)p;
    return LIB_HAMMING_OK;
}

/**
 * @brief 对一块码字解码（标准码，单纠错）
 * @see LIB_Hamming_DecodeBlock
 */
LIB_Hamming_Status_t LIB_Hamming_DecodeBlock(const uint8_t *code, const LIB_Hamming_Cfg_t *cfg,
                                             uint8_t *data, uint8_t *syndrome, int16_t *err_pos)
{
    uint8_t recon = 0;
    uint16_t n_s;
    uint16_t i;
    int16_t pos = -1;
    LIB_Hamming_Status_t st = LIB_HAMMING_OK;

    if (code == NULL || !Hamming_CfgValid(cfg))
        return LIB_HAMMING_BAD_ARG;

    n_s = (uint16_t)(cfg->k_s + cfg->m);
    for (i = 0; i < n_s; i++)
    {
        if (LIB_Hamming_BitGet(code, i))
        {
            recon ^= Hamming_ColumnOf(i, cfg);
        }
    }
    if (syndrome != NULL)
        *syndrome = recon;

    if (recon != 0u)
    {
        /* 命中有效列 ⇒ 已定位单错待纠；落在缩短掉的列 ⇒ DETECTED（pos 置 -1） */
        st = LIB_Hamming_PosFromSyndrome(recon, cfg, &pos);
        if (st == LIB_HAMMING_OK)
            st = LIB_HAMMING_CORRECTED;
    }

    /* 输出数据位：仅当命中有效列（pos 落在数据位）时翻转纠正；检出不可纠则输出原始位 */
    if (data != NULL)
    {
        for (i = 0; i < cfg->k_s; i++)
        {
            uint8_t bit = LIB_Hamming_BitGet(code, i);

            if ((int16_t)i == pos)
                bit ^= 1u;
            LIB_Hamming_BitSet(data, i, bit);
        }
    }
    if (err_pos != NULL)
        *err_pos = pos;
    return st;
}

/**
 * @brief 编码任意 bit 长度数据（标准汉明码）
 * @see LIB_Hamming_Encode
 */
int8_t LIB_Hamming_Encode(const uint8_t *data, uint32_t data_bits, const LIB_Hamming_Cfg_t *cfg,
                          uint8_t *code, uint32_t code_cap_bits, uint32_t *code_bits)
{
    uint32_t blocks;
    uint32_t required;
    uint32_t n_s;
    uint32_t b;

    if (data == NULL || code == NULL || code_bits == NULL || !Hamming_CfgValid(cfg))
        return -1;
    if (data_bits == 0u)
        return -1;

    n_s = (uint32_t)cfg->k_s + cfg->m;
    blocks = (data_bits + cfg->k_s - 1u) / cfg->k_s;
    if (blocks > (0xFFFFFFFFu / n_s))
        return -1; /* 长度溢出 */
    required = blocks * n_s;
    if (code_cap_bits < required)
        return -2; /* 缓冲不足：不写任何数据 */

    memset(code, 0, (size_t)((required + 7u) / 8u));

    for (b = 0; b < blocks; b++)
    {
        uint8_t blk_data[HAMMING_BLK_BYTES];
        uint8_t blk_code[HAMMING_BLK_BYTES];
        uint32_t base = b * cfg->k_s;
        uint32_t have = (data_bits > base) ? (data_bits - base) : 0u;
        uint32_t i;

        if (have > cfg->k_s)
            have = cfg->k_s;

        /* 末块不足 k_s 位补零（补零位是真实发送位，参与编码与纠错） */
        memset(blk_data, 0, sizeof(blk_data));
        for (i = 0; i < have; i++)
        {
            LIB_Hamming_BitSet(blk_data, i, LIB_Hamming_BitGet(data, base + i));
        }

        if (LIB_Hamming_EncodeBlock(blk_data, cfg, blk_code) != LIB_HAMMING_OK)
            return -1;

        for (i = 0; i < n_s; i++)
        {
            LIB_Hamming_BitSet(code, b * n_s + i, LIB_Hamming_BitGet(blk_code, i));
        }
    }

    *code_bits = required;
    return 0;
}

/**
 * @brief 解码任意 bit 长度数据（标准汉明码）
 * @see LIB_Hamming_Decode
 */
int8_t LIB_Hamming_Decode(const uint8_t *code, uint32_t code_bits, uint32_t data_bits,
                          const LIB_Hamming_Cfg_t *cfg, uint8_t *data, LIB_Hamming_Stat_t *stat)
{
    uint32_t blocks;
    uint32_t required;
    uint32_t n_s;
    uint32_t b;

    if (code == NULL || data == NULL || !Hamming_CfgValid(cfg))
        return -1;
    if (data_bits == 0u)
        return -1;

    n_s = (uint32_t)cfg->k_s + cfg->m;
    blocks = (data_bits + cfg->k_s - 1u) / cfg->k_s;
    if (blocks > (0xFFFFFFFFu / n_s))
        return -1;
    required = blocks * n_s;
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
        uint8_t blk_code[HAMMING_BLK_BYTES];
        uint8_t blk_data[HAMMING_BLK_BYTES];
        uint8_t syn = 0;
        int16_t pos = -1;
        LIB_Hamming_Status_t st;
        uint32_t base = b * cfg->k_s;
        uint32_t have = (data_bits > base) ? (data_bits - base) : 0u;
        uint32_t i;

        if (have > cfg->k_s)
            have = cfg->k_s;

        memset(blk_code, 0, sizeof(blk_code));
        for (i = 0; i < n_s; i++)
        {
            LIB_Hamming_BitSet(blk_code, i, LIB_Hamming_BitGet(code, b * n_s + i));
        }

        st = LIB_Hamming_DecodeBlock(blk_code, cfg, blk_data, &syn, &pos);

        /* 末块补零位被纠回后在此截断丢弃 */
        for (i = 0; i < have; i++)
        {
            LIB_Hamming_BitSet(data, base + i, LIB_Hamming_BitGet(blk_data, i));
        }

        if (stat != NULL)
        {
            if (st == LIB_HAMMING_CORRECTED)
            {
                stat->corrected++;
                if (stat->first_err_pos < 0)
                    stat->first_err_pos = (int32_t)(b * n_s) + (int32_t)pos;
            }
            else if (st == LIB_HAMMING_DETECTED)
            {
                stat->detected++;
            }
        }
    }
    return 0;
}

#endif /* LIB_HAMMING_USED */

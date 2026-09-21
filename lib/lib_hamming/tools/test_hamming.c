/**
 * @file test_hamming.c
 * @brief lib_hamming PC 端穷举检验（标准码 + 扩展缩短码），退出码 0 = PASS
 *
 * @note 用 tools/test_hamming.sh 一键编译运行（gcc，-DLIB_HAMMING_STANDALONE 跳过 app_cfg.h）。
 * @note 检验项：
 *       1. 参数：m/k_s 越界拒绝、Fit 取 min(data_bits,k)、RequiredBits 长度
 *       2. 结构（独立于编解码器自洽性）：系统式、H·c = 0、最小距离 >= 3（小码字穷举两两）
 *       3. 单 bit 错穷举全位置 → 标准/扩展码必纠对且数据不变
 *       4. 双 bit 错：扩展码必检出，标准码仅要求不崩
 *       5. 无效列（被缩短掉的列）→ 必判检出
 *       5b. 列值表锚定：全部 m/k_s/位置经公共 API 取到的列 == 定义式现场穷举值，
 *           且每个列值都反查回自身位置（表的独立性参照，防表被写错/置换）
 *       6. 任意长度：非字节对齐、多块、末块补零、绝对错误位置
 *       7. 缓冲：不足不写、脏缓冲尾部位清零、code_bits 不匹配拒绝
 *       8. 随机 Monte-Carlo 往返（0/1 位错）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib_hamming.h"
#include "lib_hamming_ext.h"

#define BUFB 512 /* 测试位缓冲字节数 */

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, ...)                       \
    do                                         \
    {                                          \
        g_checks++;                            \
        if (!(cond))                           \
        {                                      \
            g_fails++;                         \
            if (g_fails <= 30)                 \
            {                                  \
                printf("FAIL %d: ", __LINE__); \
                printf(__VA_ARGS__);           \
                printf("\n");                  \
            }                                  \
        }                                      \
    } while (0)

static uint32_t g_rng = 0x12345678u;

/* xorshift32：确定性随机，保证可复现 */
static uint32_t rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

static uint16_t full_k(uint8_t m)
{
    return (uint16_t)(((1u << m) - 1u) - m);
}

static void flip_bit(uint8_t *buf, uint32_t i)
{
    LIB_Hamming_BitSet(buf, i, (uint8_t)(LIB_Hamming_BitGet(buf, i) ^ 1u));
}

static int bits_equal(const uint8_t *a, const uint8_t *b, uint32_t nbits)
{
    uint32_t i;

    for (i = 0; i < nbits; i++)
    {
        if (LIB_Hamming_BitGet(a, i) != LIB_Hamming_BitGet(b, i))
            return 0;
    }
    return 1;
}

static void fill_random(uint8_t *buf, uint32_t nbits)
{
    uint32_t i;

    memset(buf, 0, (size_t)((nbits + 7u) / 8u));
    for (i = 0; i < nbits; i++)
        LIB_Hamming_BitSet(buf, i, (uint8_t)(rnd() & 1u));
}

/* ---------- 1. 参数与工具 ---------- */
static void test_params(void)
{
    LIB_Hamming_Cfg_t cfg;
    uint8_t data[BUFB] = {0};
    uint8_t code[BUFB] = {0};
    uint32_t cb = 0;

    CHECK(LIB_Hamming_Fit(0, 4, &cfg) == -1, "Fit data_bits=0 应拒绝");
    CHECK(LIB_Hamming_Fit(10, 1, &cfg) == -1, "Fit m=1 应拒绝");
    CHECK(LIB_Hamming_Fit(10, 8, &cfg) == -1, "Fit m=8 应拒绝");
    CHECK(LIB_Hamming_Fit(10, 4, &cfg) == 0 && cfg.m == 4 && cfg.k_s == 10, "Fit(10,4)");
    CHECK(LIB_Hamming_Fit(100, 3, &cfg) == 0 && cfg.k_s == full_k(3), "Fit 取满 k");

    cfg.m = 3;
    cfg.k_s = 0;
    CHECK(LIB_Hamming_EncodeBlock(data, &cfg, code) == LIB_HAMMING_BAD_ARG, "k_s=0 拒绝");
    cfg.k_s = (uint16_t)(full_k(3) + 1u);
    CHECK(LIB_Hamming_EncodeBlock(data, &cfg, code) == LIB_HAMMING_BAD_ARG, "k_s=k+1 拒绝");
    cfg.m = 8;
    cfg.k_s = 1;
    CHECK(LIB_Hamming_EncodeBlock(data, &cfg, code) == LIB_HAMMING_BAD_ARG, "m=8 拒绝");
    CHECK(LIB_Hamming_DecodeBlock(NULL, &cfg, NULL, NULL, NULL) == LIB_HAMMING_BAD_ARG,
          "DecodeBlock NULL 拒绝");

    cfg.m = 4;
    cfg.k_s = 8;
    CHECK(LIB_Hamming_Encode(NULL, 8, &cfg, code, BUFB * 8u, &cb) == -1, "data=NULL 拒绝");
    CHECK(LIB_Hamming_Encode(data, 0, &cfg, code, BUFB * 8u, &cb) == -1, "data_bits=0 拒绝");
    CHECK(LIB_Hamming_RequiredBits(16, &cfg, 0) == 24u, "RequiredBits std");
    CHECK(LIB_Hamming_RequiredBits(16, &cfg, 1) == 26u, "RequiredBits ext");
    CHECK(LIB_Hamming_RequiredBits(0, &cfg, 0) == 0u, "RequiredBits data_bits=0");
}

/* ---------- 2. 结构：系统式 + H·c=0 + 最小距离 >= 3 ---------- */
static void test_structure(void)
{
    uint8_t m;

    for (m = 2; m <= 4; m++) /* 2^k 可穷举：m<=4 → k<=11 → 2048 码字 */
    {
        uint16_t k = full_k(m);
        uint16_t n_s = (uint16_t)(k + m);
        uint32_t total = 1u << k;
        uint32_t cwbytes = (n_s + 7u) / 8u;
        uint8_t *cws = (uint8_t *)calloc(total, cwbytes);
        uint8_t data[8];
        uint8_t code[8];
        LIB_Hamming_Cfg_t cfg = {m, k};
        uint32_t d;

        CHECK(cws != NULL, "calloc m=%u 失败", m);
        if (cws == NULL)
            continue;

        for (d = 0; d < total; d++)
        {
            uint16_t i;
            uint8_t syn = 0xFFu;
            int16_t pos = -2;
            uint8_t out[8];
            LIB_Hamming_Status_t st;

            memset(data, 0, sizeof(data));
            for (i = 0; i < k; i++)
                LIB_Hamming_BitSet(data, i, (uint8_t)((d >> i) & 1u));

            LIB_Hamming_EncodeBlock(data, &cfg, code);

            /* 系统式：码字前 k 位 = 数据 */
            CHECK(bits_equal(data, code, k), "m=%u 非系统式 d=%u", m, d);

            /* H·c = 0：syndrome 为 0，状态 OK，往返一致 */
            st = LIB_Hamming_DecodeBlock(code, &cfg, out, &syn, &pos);
            CHECK(st == LIB_HAMMING_OK && syn == 0 && pos == -1, "m=%u H·c!=0 d=%u", m, d);
            CHECK(bits_equal(out, data, k), "m=%u 往返不一致 d=%u", m, d);

            memcpy(&cws[(size_t)d * cwbytes], code, cwbytes);
        }

        /* 最小距离 >= 3（穷举两两码字） */
        {
            uint32_t a;
            uint32_t b;
            uint32_t mind = 0xFFFFFFFFu;

            for (a = 0; a < total; a++)
            {
                for (b = a + 1; b < total; b++)
                {
                    uint32_t w = 0;
                    uint32_t i;

                    for (i = 0; i < n_s; i++)
                    {
                        if (LIB_Hamming_BitGet(&cws[(size_t)a * cwbytes], i) !=
                            LIB_Hamming_BitGet(&cws[(size_t)b * cwbytes], i))
                            w++;
                    }
                    if (w < mind)
                        mind = w;
                }
            }
            CHECK(mind >= 3u, "m=%u 最小距离 %u < 3", m, mind);
        }
        free(cws);
    }
}

/* ---------- 3. 单 bit 错穷举（标准码块 + 扩展码缓冲层） ---------- */
static void test_single_error(void)
{
    uint8_t m;

    for (m = 2; m <= 7; m++)
    {
        uint16_t k = full_k(m);
        uint16_t ks_list[3];
        uint16_t nk = 0;
        uint16_t a;

        ks_list[nk++] = 1;
        if ((uint16_t)(k / 2) > 1u && (uint16_t)(k / 2) < k)
            ks_list[nk++] = (uint16_t)(k / 2);
        if (k > 1u)
            ks_list[nk++] = k;

        for (a = 0; a < nk; a++)
        {
            uint16_t ks = ks_list[a];
            LIB_Hamming_Cfg_t cfg = {m, ks};
            uint16_t n_s = (uint16_t)(ks + m);
            uint16_t n_e = (uint16_t)(n_s + 1u);
            uint32_t npat = (ks <= 10u) ? (1u << ks) : 100u;
            uint32_t p;

            for (p = 0; p < npat; p++)
            {
                uint8_t data[16];
                uint8_t code[16];
                uint8_t out[16];
                uint32_t i;

                if (ks <= 10u)
                {
                    memset(data, 0, sizeof(data));
                    for (i = 0; i < ks; i++)
                        LIB_Hamming_BitSet(data, i, (uint8_t)((p >> i) & 1u));
                }
                else
                {
                    fill_random(data, ks);
                }

                /* ---- 标准码：码字内任意单 bit 翻转都必须纠对 ---- */
                LIB_Hamming_EncodeBlock(data, &cfg, code);
                for (i = 0; i < n_s; i++)
                {
                    uint8_t tmp[16];
                    int16_t pos = -2;
                    LIB_Hamming_Status_t st;

                    memcpy(tmp, code, sizeof(tmp));
                    flip_bit(tmp, i);
                    st = LIB_Hamming_DecodeBlock(tmp, &cfg, out, NULL, &pos);
                    CHECK(st == LIB_HAMMING_CORRECTED && pos == (int16_t)i &&
                              bits_equal(out, data, ks),
                          "std 单错 m=%u ks=%u i=%u st=%d pos=%d", m, ks, i, (int)st, (int)pos);
                }

                /* ---- 扩展缩短码（缓冲层，data_bits = ks 即单块） ---- */
                {
                    uint8_t ecode[16];
                    uint8_t eout[16];
                    uint8_t tmp[16];
                    uint32_t ebits = 0;
                    LIB_Hamming_Stat_t stat;

                    CHECK(LIB_Hamming_ExtEncode(data, ks, &cfg, ecode, sizeof(ecode) * 8u, &ebits) == 0 &&
                              ebits == n_e,
                          "ext 编码 m=%u ks=%u", m, ks);

                    for (i = 0; i < n_e; i++)
                    {
                        memcpy(tmp, ecode, sizeof(tmp));
                        flip_bit(tmp, i);
                        CHECK(LIB_Hamming_ExtDecode(tmp, n_e, ks, &cfg, eout, &stat) == 0 &&
                                  bits_equal(eout, data, ks),
                              "ext 单错数据 m=%u ks=%u i=%u", m, ks, i);
                        if (i == n_s)
                        {
                            /* 翻的是总校验位本身：数据无错，不算纠正 */
                            CHECK(stat.corrected == 0u && stat.detected == 0u,
                                  "ext 总校验位错 m=%u ks=%u corr=%u det=%u", m, ks,
                                  stat.corrected, stat.detected);
                        }
                        else
                        {
                            CHECK(stat.corrected == 1u && stat.detected == 0u,
                                  "ext 单错 m=%u ks=%u i=%u corr=%u det=%u", m, ks, i,
                                  stat.corrected, stat.detected);
                        }
                    }
                }
            }
        }
    }
}

/* ---------- 4. 双 bit 错 + 5. 无效列 ---------- */
static void test_double_error(void)
{
    uint8_t m;

    for (m = 2; m <= 4; m++)
    {
        uint16_t k = full_k(m);
        LIB_Hamming_Cfg_t cfg = {m, k};
        uint16_t n_s = (uint16_t)(k + m);
        uint16_t n_e = (uint16_t)(n_s + 1u);
        uint32_t npat = 1u << k;
        uint32_t p;

        for (p = 0; p < npat; p++)
        {
            uint8_t data[8];
            uint8_t code[8];
            uint8_t ecode[8];
            uint8_t out[8];
            uint32_t i;
            uint32_t j;

            memset(data, 0, sizeof(data));
            for (i = 0; i < k; i++)
                LIB_Hamming_BitSet(data, i, (uint8_t)((p >> i) & 1u));
            LIB_Hamming_EncodeBlock(data, &cfg, code);

            /* 标准码：双错只要求不崩、状态在 {CORRECTED, DETECTED} */
            for (i = 0; i < n_s; i++)
            {
                for (j = i + 1; j < n_s; j++)
                {
                    uint8_t tmp[8];
                    int16_t pos = -2;
                    LIB_Hamming_Status_t st;

                    memcpy(tmp, code, sizeof(tmp));
                    flip_bit(tmp, i);
                    flip_bit(tmp, j);
                    st = LIB_Hamming_DecodeBlock(tmp, &cfg, out, NULL, &pos);
                    CHECK(st == LIB_HAMMING_CORRECTED || st == LIB_HAMMING_DETECTED,
                          "std 双错状态异常 m=%u", m);
                }
            }

            /* 扩展码：双错必须全部检出，绝不误纠 */
            {
                uint32_t ebits = 0;
                uint8_t eout[8];
                LIB_Hamming_Stat_t stat;

                LIB_Hamming_ExtEncode(data, k, &cfg, ecode, sizeof(ecode) * 8u, &ebits);
                for (i = 0; i < n_e; i++)
                {
                    for (j = i + 1; j < n_e; j++)
                    {
                        uint8_t tmp[8];

                        memcpy(tmp, ecode, sizeof(tmp));
                        flip_bit(tmp, i);
                        flip_bit(tmp, j);
                        CHECK(LIB_Hamming_ExtDecode(tmp, n_e, k, &cfg, eout, &stat) == 0 &&
                                  stat.detected == 1u && stat.corrected == 0u,
                              "ext 双错未检出 m=%u i=%u j=%u corr=%u det=%u", m, i, j,
                              stat.corrected, stat.detected);
                    }
                }
            }
        }
    }
}

static void test_invalid_column(void)
{
    /* m=3,k_s=3：数据列 {3,5,6}(pos0-2)、校验列 {1,2,4}(pos3-5)
       翻 pos0(列3) 与 pos5(列4) ⇒ syndrome = 3^4 = 7 = 被缩短掉的列 ⇒ 必判检出 */
    LIB_Hamming_Cfg_t cfg = {3, 3};
    uint8_t data[2] = {0};
    uint8_t code[2] = {0};
    uint8_t out[2] = {0};
    uint8_t syn = 0;
    int16_t pos = 0;
    LIB_Hamming_Status_t st;

    LIB_Hamming_EncodeBlock(data, &cfg, code);
    flip_bit(code, 0);
    flip_bit(code, 5);
    st = LIB_Hamming_DecodeBlock(code, &cfg, out, &syn, &pos);
    CHECK(syn == 7u, "无效列 syndrome 期望 7，得 %u", syn);
    CHECK(st == LIB_HAMMING_DETECTED && pos == -1, "无效列应判检出");
}

static void test_posfromsyndrome(void)
{
    static const struct
    {
        uint8_t sy;
        int16_t pos;
        LIB_Hamming_Status_t st;
    } cases[] = {
        {3, 0, LIB_HAMMING_OK}, {5, 1, LIB_HAMMING_OK}, {6, 2, LIB_HAMMING_OK},
        {1, 3, LIB_HAMMING_OK}, {2, 4, LIB_HAMMING_OK}, {4, 5, LIB_HAMMING_OK},
        {7, -1, LIB_HAMMING_DETECTED},
    };
    LIB_Hamming_Cfg_t cfg = {3, 3};
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int16_t pos = -2;
        LIB_Hamming_Status_t st = LIB_Hamming_PosFromSyndrome(cases[i].sy, &cfg, &pos);

        CHECK(st == cases[i].st && pos == cases[i].pos, "PosFromSyndrome(%u) st=%d pos=%d",
              cases[i].sy, (int)st, (int)pos);
    }
    {
        int16_t pos = -2;
        CHECK(LIB_Hamming_PosFromSyndrome(0, &cfg, &pos) == LIB_HAMMING_BAD_ARG, "sy=0 应拒绝");
    }
}

/* ---------- 5b. 列值表锚定：查表只许加速，不许改语义 ---------- */
/* 定义式现场穷举：第 (pos+1) 个"非 2 的幂"的正整数（升序 3,5,6,7,9,...）。
 * 与 lib_hamming.c 里 s_hamming_col 出自同一定义，但这里是独立复现的参照：
 * 表被写错/置换（编码解码仍自洽，往返测试抓不到）或扩 LIB_HAMMING_M_MAX 时
 * 忘记扩表，都会在这里被逐项抓出来。 */
static uint8_t ref_column_of(uint16_t pos)
{
    uint16_t rank = 0;
    uint16_t v;

    for (v = 1u; v <= 0xFFu; v++)
    {
        if ((v & (uint16_t)(v - 1u)) != 0u) /* 非 2 的幂 */
        {
            if (rank == pos)
                return (uint8_t)v;
            rank++;
        }
    }
    return 0;
}

/* 码字前 k_s 位是数据、后 m 位是校验 ⇒ "只置数据位 pos"的码字，其 m 个校验位
 * 拼起来正好是 ColumnOf(pos)（校验列是 e_j，只出现在自身方程）。 */
static uint8_t code_column(const uint8_t *code, const LIB_Hamming_Cfg_t *cfg)
{
    uint8_t acc = 0;
    uint8_t j;

    for (j = 0; j < cfg->m; j++)
    {
        acc |= (uint8_t)(LIB_Hamming_BitGet(code, (uint32_t)cfg->k_s + j) << j);
    }
    return acc;
}

static void test_column_table(void)
{
    uint8_t m;

    for (m = 2u; m <= LIB_HAMMING_M_MAX; m++)
    {
        uint16_t k = full_k(m);
        uint16_t k_s;

        for (k_s = 1u; k_s <= k; k_s++)
        {
            LIB_Hamming_Cfg_t cfg = {m, k_s};
            uint16_t pos;
            uint8_t j;

            for (pos = 0; pos < k_s; pos++)
            {
                uint8_t data[BUFB] = {0};
                uint8_t code[BUFB] = {0};
                int16_t back = -2;
                uint8_t want = ref_column_of(pos);
                uint8_t got;

                LIB_Hamming_BitSet(data, pos, 1u);
                LIB_Hamming_EncodeBlock(data, &cfg, code);
                got = code_column(code, &cfg);

                CHECK(got == want, "列值表 m=%u k_s=%u pos=%u got=%u want=%u", m, k_s, pos, got, want);
                /* 顺带锚定反查公式：表中每个值都必须反查回它自己的位置 */
                CHECK(LIB_Hamming_PosFromSyndrome(want, &cfg, &back) == LIB_HAMMING_OK &&
                          back == (int16_t)pos,
                      "列反查 m=%u k_s=%u pos=%u -> %d", m, k_s, pos, (int)back);
            }

            /* 校验列 j 是单位向量 e_j，不经表；反查必须回到 k_s+j */
            for (j = 0; j < cfg.m; j++)
            {
                int16_t back = -2;

                CHECK(LIB_Hamming_PosFromSyndrome((uint8_t)(1u << j), &cfg, &back) ==
                              LIB_HAMMING_OK &&
                          back == (int16_t)(cfg.k_s + j),
                      "校验列反查 m=%u k_s=%u j=%u -> %d", m, k_s, j, (int)back);
            }
        }
    }
}

/* ---------- 6. 任意长度：补零、多块、绝对错误位置 ---------- */
static void test_padding_and_lengths(void)
{
    LIB_Hamming_Cfg_t cfg = {3, 4}; /* k=4, n_s=7, n_e=8 */
    uint16_t n_s = 7;
    uint16_t n_e = 8;
    uint32_t data_bits = 5; /* 2 块：块0 满 4 位，块1 1 位 + 3 位补零 */
    uint8_t data[BUFB];
    uint8_t code[BUFB];
    uint8_t out[BUFB];
    uint32_t cb = 0;
    uint32_t i;

    fill_random(data, data_bits);
    CHECK(LIB_Hamming_Encode(data, data_bits, &cfg, code, BUFB * 8u, &cb) == 0 && cb == 2u * n_s,
          "补零用例编码长度");
    CHECK(LIB_Hamming_Decode(code, cb, data_bits, &cfg, out, NULL) == 0 &&
              bits_equal(out, data, data_bits),
          "补零往返");

    /* 标准码：翻块1 的补零位（绝对位置 n_s + 1..3）→ 纠回且输出数据不变 */
    for (i = 1; i < 4u; i++)
    {
        uint8_t tmp[BUFB];
        LIB_Hamming_Stat_t stat;

        memcpy(tmp, code, sizeof(tmp));
        flip_bit(tmp, n_s + i);
        CHECK(LIB_Hamming_Decode(tmp, cb, data_bits, &cfg, out, &stat) == 0 &&
                  stat.corrected == 1u && stat.detected == 0u &&
                  bits_equal(out, data, data_bits),
              "补零位单错 %u", i);
    }

    /* 扩展码：补零位同样可纠；补零位 + 数据位 = 双错必检出 */
    {
        uint8_t ecode[BUFB];
        uint8_t eout[BUFB];
        uint32_t ebits = 0;
        LIB_Hamming_Stat_t stat;

        CHECK(LIB_Hamming_ExtEncode(data, data_bits, &cfg, ecode, BUFB * 8u, &ebits) == 0 &&
                  ebits == 2u * n_e,
              "ext 补零编码长度");

        for (i = 1; i < 4u; i++)
        {
            uint8_t tmp[BUFB];

            memcpy(tmp, ecode, sizeof(tmp));
            flip_bit(tmp, n_e + i); /* 块1 补零位绝对位置 */
            CHECK(LIB_Hamming_ExtDecode(tmp, ebits, data_bits, &cfg, eout, &stat) == 0 &&
                      stat.corrected == 1u && stat.detected == 0u &&
                      bits_equal(eout, data, data_bits),
                  "ext 补零位单错 %u", i);
        }

        /* 同一块内翻两位 = 双错 → 必检出（块1 数据位0 + 块1 补零位1） */
        {
            uint8_t tmp[BUFB];

            memcpy(tmp, ecode, sizeof(tmp));
            flip_bit(tmp, n_e + 0u);
            flip_bit(tmp, n_e + 1u);
            CHECK(LIB_Hamming_ExtDecode(tmp, ebits, data_bits, &cfg, eout, &stat) == 0 &&
                      stat.detected == 1u && stat.corrected == 0u,
                  "ext 同块双错应检出 det=%u corr=%u", stat.detected, stat.corrected);
        }

        /* 跨块各错一位 = 两个独立单错 → 各自纠正（体现"每块独立纠错"） */
        {
            uint8_t tmp[BUFB];

            memcpy(tmp, ecode, sizeof(tmp));
            flip_bit(tmp, 0u);       /* 块0 数据位0 */
            flip_bit(tmp, n_e + 1u); /* 块1 补零位1 */
            CHECK(LIB_Hamming_ExtDecode(tmp, ebits, data_bits, &cfg, eout, &stat) == 0 &&
                      stat.corrected == 2u && stat.detected == 0u &&
                      bits_equal(eout, data, data_bits),
                  "ext 跨块双错应各自纠正 corr=%u det=%u", stat.corrected, stat.detected);
        }
    }
}

static void test_lengths(void)
{
    static const uint32_t lens[] = {1, 7, 8, 13, 37, 63, 64, 65, 100, 121, 127, 255};
    size_t li;

    for (li = 0; li < sizeof(lens) / sizeof(lens[0]); li++)
    {
        uint8_t m = (uint8_t)(2u + (li % 6u));
        LIB_Hamming_Cfg_t cfg;
        uint32_t bits = lens[li];
        uint8_t data[BUFB];
        uint8_t out[BUFB];
        uint8_t code[BUFB];
        uint32_t cb = 0;
        uint32_t req;
        uint32_t ereq;
        uint32_t ebits = 0;

        CHECK(LIB_Hamming_Fit(bits, m, &cfg) == 0, "Fit 失败 li=%u", (unsigned)li);
        req = LIB_Hamming_RequiredBits(bits, &cfg, 0);
        ereq = LIB_Hamming_RequiredBits(bits, &cfg, 1);
        CHECK(req > 0u && req <= BUFB * 8u && ereq <= BUFB * 8u, "长度超测试缓冲 bits=%u", bits);
        if (req == 0u || req > BUFB * 8u)
            continue;

        fill_random(data, bits);

        CHECK(LIB_Hamming_Encode(data, bits, &cfg, code, BUFB * 8u, &cb) == 0 && cb == req,
              "长度编码 bits=%u", bits);
        CHECK(LIB_Hamming_Decode(code, cb, bits, &cfg, out, NULL) == 0 &&
                  bits_equal(out, data, bits),
              "长度往返 bits=%u", bits);

        memset(code, 0, sizeof(code));
        CHECK(LIB_Hamming_ExtEncode(data, bits, &cfg, code, BUFB * 8u, &ebits) == 0 &&
                  ebits == ereq,
              "ext 长度编码 bits=%u", bits);
        CHECK(LIB_Hamming_ExtDecode(code, ebits, bits, &cfg, out, NULL) == 0 &&
                  bits_equal(out, data, bits),
              "ext 长度往返 bits=%u", bits);
    }
}

static void test_multiblock_position(void)
{
    LIB_Hamming_Cfg_t cfg = {4, 8}; /* k_s=8, n_e=13 */
    uint32_t bits = 8u * 5u + 3u;   /* 6 块，末块 3 位 */
    uint8_t data[BUFB];
    uint8_t ecode[BUFB];
    uint8_t eout[BUFB];
    uint8_t tmp[BUFB];
    uint32_t ebits = 0;
    LIB_Hamming_Stat_t stat;

    fill_random(data, bits);
    CHECK(LIB_Hamming_ExtEncode(data, bits, &cfg, ecode, BUFB * 8u, &ebits) == 0, "多块编码");

    /* 块 3 的数据位 2 翻错 → 绝对位置 3*13 + 2 */
    memcpy(tmp, ecode, sizeof(tmp));
    flip_bit(tmp, 3u * 13u + 2u);
    CHECK(LIB_Hamming_ExtDecode(tmp, ebits, bits, &cfg, eout, &stat) == 0 &&
              stat.corrected == 1u && stat.first_err_pos == (int32_t)(3u * 13u + 2u) &&
              bits_equal(eout, data, bits),
          "多块绝对错误位置 corr=%u pos=%d", stat.corrected, stat.first_err_pos);

    /* 块 4 的总校验位翻错 → 数据无错、不计纠正 */
    memcpy(tmp, ecode, sizeof(tmp));
    flip_bit(tmp, 4u * 13u + 12u);
    CHECK(LIB_Hamming_ExtDecode(tmp, ebits, bits, &cfg, eout, &stat) == 0 &&
              stat.corrected == 0u && stat.detected == 0u && bits_equal(eout, data, bits),
          "多块总校验位错 corr=%u det=%u", stat.corrected, stat.detected);
}

/* ---------- 7. 缓冲与错误长度 ---------- */
static void test_buffers(void)
{
    LIB_Hamming_Cfg_t cfg = {4, 8};
    uint32_t bits = 20;
    uint32_t req = LIB_Hamming_RequiredBits(bits, &cfg, 0);
    uint8_t data[BUFB];
    uint8_t code[BUFB];
    uint8_t out[BUFB];
    uint8_t ref[BUFB];
    uint32_t cb = 0;
    uint32_t i;

    fill_random(data, bits);

    /* 缓冲不足：返回 -2 且不写 */
    memset(code, 0xAA, sizeof(code));
    memcpy(ref, code, sizeof(code));
    CHECK(LIB_Hamming_Encode(data, bits, &cfg, code, req - 1u, &cb) == -2, "缓冲不足应 -2");
    CHECK(memcmp(code, ref, sizeof(code)) == 0, "缓冲不足不应写入");

    /* 脏缓冲：编码后超出 required 的位应为 0 */
    memset(code, 0xFF, sizeof(code));
    CHECK(LIB_Hamming_Encode(data, bits, &cfg, code, BUFB * 8u, &cb) == 0 && cb == req, "脏缓冲编码");
    for (i = req; i < ((req + 7u) / 8u) * 8u; i++)
        CHECK(LIB_Hamming_BitGet(code, i) == 0u, "尾部位未清零 i=%u", i);

    /* code_bits 不匹配 → 拒绝 */
    CHECK(LIB_Hamming_Decode(code, req - 1u, bits, &cfg, out, NULL) == -1, "code_bits 少应拒绝");
    CHECK(LIB_Hamming_Decode(code, req + 1u, bits, &cfg, out, NULL) == -1, "code_bits 多应拒绝");
    CHECK(LIB_Hamming_Decode(code, req, bits, &cfg, out, NULL) == 0 && bits_equal(out, data, bits),
          "合法解码");
}

/* ---------- 8. 随机 Monte-Carlo ---------- */
static void test_random(void)
{
    uint32_t t;

    for (t = 0; t < 2000u; t++)
    {
        uint8_t m = (uint8_t)(2u + (rnd() % 6u));
        LIB_Hamming_Cfg_t cfg;
        uint32_t bits = 1u + (rnd() % 200u);
        uint8_t data[BUFB];
        uint8_t code[BUFB];
        uint8_t out[BUFB];
        uint32_t cb = 0;
        uint32_t nerr;
        LIB_Hamming_Stat_t stat;

        if (LIB_Hamming_Fit(bits, m, &cfg) != 0)
            continue;
        if (LIB_Hamming_RequiredBits(bits, &cfg, 0) > BUFB * 8u)
            continue;

        fill_random(data, bits);
        CHECK(LIB_Hamming_Encode(data, bits, &cfg, code, BUFB * 8u, &cb) == 0, "随机编码失败");
        if (cb == 0u)
            continue;

        nerr = rnd() % 2u; /* 0 或 1 位错 */
        if (nerr == 1u)
            flip_bit(code, rnd() % cb);

        if (LIB_Hamming_Decode(code, cb, bits, &cfg, out, &stat) != 0)
        {
            CHECK(0, "随机解码失败");
            continue;
        }
        CHECK(stat.corrected == nerr, "随机 corrected=%u nerr=%u", stat.corrected, nerr);
        CHECK(stat.detected == 0u, "随机 detected=%u", stat.detected);
        CHECK(bits_equal(out, data, bits), "随机往返不一致 bits=%u", bits);
    }
}

/* 编译期宏（上层据此定帧长/缓冲）必须与运行期函数逐个参数完全一致，
 * 否则 comm 协议在编译期算出的块数/码长会与实际编解码错位 */
static void test_compile_time_macros(void)
{
    uint8_t m;
    uint32_t bits;

    for (m = 2u; m <= LIB_HAMMING_M_MAX; m++)
    {
        CHECK(LIB_HAMMING_FULL_K(m) == full_k(m), "宏 FULL_K(%u)=%u 与运行期不等", m,
              (unsigned)LIB_HAMMING_FULL_K(m));

        for (bits = 1u; bits <= 400u; bits++)
        {
            LIB_Hamming_Cfg_t cfg = {0, 0};
            uint32_t ks;

            CHECK(LIB_Hamming_Fit(bits, m, &cfg) == 0, "Fit 失败 bits=%u m=%u", bits, m);
            ks = LIB_HAMMING_FIT_KS(bits, m);
            CHECK(ks == cfg.k_s, "宏 FIT_KS(%u,%u)=%u 与 Fit 的 %u 不等", bits, m, ks, cfg.k_s);
            CHECK(LIB_HAMMING_BLOCK_CNT(bits, ks) == (bits + ks - 1u) / ks, "块数宏错");

            CHECK(LIB_HAMMING_CODE_BITS(bits, cfg.k_s, m, 0) ==
                      LIB_Hamming_RequiredBits(bits, &cfg, 0),
                  "标准码长宏 %u != 运行期 %u (bits=%u m=%u)",
                  LIB_HAMMING_CODE_BITS(bits, cfg.k_s, m, 0),
                  LIB_Hamming_RequiredBits(bits, &cfg, 0), bits, m);
            CHECK(LIB_HAMMING_CODE_BITS(bits, cfg.k_s, m, 1) ==
                      LIB_Hamming_RequiredBits(bits, &cfg, 1),
                  "扩展码长宏 != 运行期 (bits=%u m=%u)", bits, m);
            CHECK(LIB_HAMMING_CODE_BYTES(bits, cfg.k_s, m, 1) ==
                      (LIB_Hamming_RequiredBits(bits, &cfg, 1) + 7u) / 8u,
                  "码字字节数宏错 (bits=%u m=%u)", bits, m);
        }
    }
}

/* 字节块规划（comm 帧用）：块数据字节数 × 块数 = 编码区字节数，
 * 且换算成位后必须与运行期 RequiredBits 一致 —— 即"编译期分块"名副其实 */
static void test_byte_block_plan(void)
{
    uint8_t m = 7u; /* 字节对齐只对 m=7 成立（(m+1)%8==0） */
    uint32_t p;

    for (p = 1u; p <= 200u; p++)
    {
        LIB_Hamming_Cfg_t cfg = {0, 0};
        uint32_t B = LIB_HAMMING_BYTE_BLKS(p, m);
        uint32_t s = LIB_HAMMING_BYTE_BLK_DATA(p, m);
        uint32_t E = LIB_HAMMING_EXT_BYTES(p, m);

        CHECK(B >= 1u && s >= 1u && s <= LIB_HAMMING_BYTE_FULL_BLK(m), "字节块规划越界 p=%u", p);
        CHECK(E == B * (s + 1u), "E != B*(s+1) p=%u", p);
        CHECK(E >= p, "编码区比 payload 还短 p=%u", p);

        /* 用规划出的 k_s = 8s 走运行期函数，码长必须正好等于 E*8 位 */
        cfg.m = m;
        cfg.k_s = (uint16_t)(s * 8u);
        CHECK(LIB_Hamming_RequiredBits(p * 8u, &cfg, 1) == E * 8u,
              "规划 E=%u 与运行期码长 %u 不等 p=%u", E,
              LIB_Hamming_RequiredBits(p * 8u, &cfg, 1), p);
    }
}

/* 编译期断言真的会展开（此处参数合法，编译通过即证明可用） */
LIB_HAMMING_EXT_CHECK_M(7);
LIB_HAMMING_CHECK_CFG(7, LIB_HAMMING_BYTE_BLK_DATA(13, 7) * 8u);

int main(void)
{
    printf("lib_hamming PC 端检验\n");

    test_params();
    test_compile_time_macros();
    test_byte_block_plan();
    test_posfromsyndrome();
    test_column_table();
    test_invalid_column();
    test_structure();
    test_single_error();
    test_double_error();
    test_padding_and_lengths();
    test_lengths();
    test_multiblock_position();
    test_buffers();
    test_random();

    printf("checks=%d fails=%d -> %s\n", g_checks, g_fails, g_fails ? "FAIL" : "PASS");
    return g_fails ? 1 : 0;
}

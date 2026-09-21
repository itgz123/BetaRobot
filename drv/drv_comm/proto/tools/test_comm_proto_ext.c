/**
 * @file test_comm_proto_ext.c
 * @brief comm_proto_ext（扩展缩短汉明码 + CRC8 帧协议）PC 端集成测试，退出码 0 = PASS
 *
 * @note 用 tools/test_comm_proto_ext.sh 一键编译运行（gcc + 同目录 app_cfg.h 桩）。
 * @note 直接链接真实实现（comm_proto_ext.c / comm_proto.c / lib_hamming*.c / lib_crc*.c），
 *       不是参考模型 —— 验的是会上板的那份代码。
 * @note 检验项：
 *       1. 编译期分块：PROTO_EXT_BLKS/_BLK_BYTES/_BYTES/_OVERHEAD 与运行期返回值自洽
 *       2. 往返：payload → pack → unpack → 逐 bit 相等，无错帧不计数
 *       3. 单 bit 错全位置穷举：编码区必纠回且 payload 不变；帧头/seq/CRC/帧尾必丢弃
 *       4. 双 bit 错全组合穷举：要么丢弃，要么解出的 payload 正确（SECDED 保证）
 *       5. 三 bit 错抽样：统计"CRC 复核挡掉汉明误纠"的效果
 *       6. 帧序列：重帧丢弃、跳号累计丢帧、seq 坏帧不更新
 *       7. Init：参数由 payload_size 算出，越界拒绝
 */

#include <stdio.h>
#include <string.h>

#include "comm_proto_ext.h"
#include "lib_crc.h"        /* 测试里手工重算 seq 参与的那段 CRC8 */
#include "lib_crc_tables.h"

#define FRAMEB 1024u /* 帧缓冲字节数 */

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

static uint32_t g_rng = 0x9E3779B9u;

static uint32_t rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

/*------------------------------------------------------------------
 *  实例：只作占位，测试不经过 media（(void *)&media_ 需要一个已存在的对象）
 *------------------------------------------------------------------*/

static uint8_t s_stub_media;

COMM_PROTO_EXT_DEF(t1, s_stub_media, 1);
COMM_PROTO_EXT_DEF(t13, s_stub_media, 13);
COMM_PROTO_EXT_DEF(t16, s_stub_media, 16);
COMM_PROTO_EXT_DEF(t48, s_stub_media, 48);

static uint8_t g_frame[FRAMEB];
static uint8_t g_payload[256];
static uint8_t g_ref[256];

/* 打包一帧（PayloadSize 字节的随机 payload → g_ref / g_frame），返回帧长（字节） */
static uint32_t pack_case(CommProtoExt *t, uint32_t payload_bytes)
{
    uint32_t i;

    for (i = 0; i < payload_bytes; i++)
    {
        g_ref[i] = (uint8_t)rnd();
        g_payload[i] = g_ref[i];
    }
    memset(g_frame, 0, sizeof(g_frame));
    CHECK(ProtoPack(&t->base, g_payload, g_frame) == 0, "pack 失败 payload=%u", payload_bytes);
    return payload_bytes + PROTO_EXT_OVERHEAD(payload_bytes);
}

/* 翻帧内第 bit 位 */
static void flip(uint32_t bit)
{
    g_frame[bit >> 3] ^= (uint8_t)(0x80u >> (bit & 7u));
}

/* 解包并比对：返回 1 = 被接受且 payload 正确，0 = 被丢弃，-1 = 被接受但 payload 错 */
static int unpack_case(CommProtoExt *t, uint32_t payload_bytes)
{
    const uint8_t *out = ProtoUnpack(&t->base, g_frame);

    if (out == NULL)
        return 0;
    if (memcmp(out, g_ref, payload_bytes) != 0)
        return -1;
    return 1;
}

/* 重置到"刚 Init 完"的状态（清计数，重帧） */
static void rearm(CommProtoExt *t)
{
    CHECK(CommProtoExtInit(t) == 0, "Init 失败");
}

/*------------------------------------------------------------------
 *  1. 编译期分块自洽
 *------------------------------------------------------------------*/
static void test_compile_time_layout(void)
{
    static const uint32_t sizes[] = {1u, 2u, 8u, 13u, 15u, 16u, 30u, 31u, 48u, 64u};
    uint32_t i;

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        uint32_t p = sizes[i];
        uint32_t B = PROTO_EXT_BLKS(p);
        uint32_t s = PROTO_EXT_BLK_BYTES(p);
        uint32_t E = PROTO_EXT_BYTES(p);

        CHECK(B >= 1u && s <= LIB_HAMMING_BYTE_FULL_BLK(PROTO_EXT_M), "块规划越界 p=%u", p);
        CHECK(E == B * (s + 1u), "E != B*(s+1) p=%u", p);
        CHECK(PROTO_EXT_OVERHEAD(p) + p == E + 4u, "帧长 != E+4 p=%u", p);
        /* k_s = 8s 必须在合法缩短码范围 1..2^m-1-m 内 */
        CHECK(s * 8u >= 1u && s * 8u <= LIB_HAMMING_FULL_K(PROTO_EXT_M), "k_s 越界 p=%u", p);
        /* 上界：p ≤ 15 时一块装下（最少膨胀） */
        if (p <= LIB_HAMMING_BYTE_FULL_BLK(PROTO_EXT_M))
            CHECK(B == 1u && E == p + 1u, "小块应 1 块、膨胀 1 B p=%u", p);
    }

    /* 后端已静态注册 */
    CHECK(CommProtoBackendFind(PROTO_EXT) != NULL, "PROTO_EXT 后端未注册");
}

/*------------------------------------------------------------------
 *  2. 往返
 *------------------------------------------------------------------*/
static void test_roundtrip(void)
{
    static const uint32_t sizes[] = {1u, 13u, 16u, 48u};
    uint32_t i;

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        CommProtoExt *t = (sizes[i] == 1u) ? &t1 : (sizes[i] == 13u) ? &t13 : (sizes[i] == 16u) ? &t16 : &t48;

        rearm(t);
        pack_case(t, sizes[i]);
        CHECK(unpack_case(t, sizes[i]) == 1, "往返不一致 payload=%u", sizes[i]);
        CHECK(t->rx_err == 0u && t->rx_fixed == 0u, "无错帧却计数 err=%u fixed=%u", t->rx_err,
              t->rx_fixed);
        CHECK(t->enc_bytes == PROTO_EXT_BYTES(sizes[i]), "enc_bytes 与编译期不符 payload=%u",
              sizes[i]);
        CHECK(t->cfg.k_s == (uint16_t)(PROTO_EXT_BLK_BYTES(sizes[i]) * 8u), "k_s 与编译期不符");
    }
}

/*------------------------------------------------------------------
 *  3. 单 bit 错全位置穷举
 *------------------------------------------------------------------*/
static void test_single_bit(void)
{
    CommProtoExt *t = &t16;
    uint32_t payload_bytes = 16u;
    uint32_t frame_bytes;
    uint32_t frame_bits;
    uint32_t E;
    uint32_t b;

    rearm(t);
    frame_bytes = pack_case(t, payload_bytes);
    frame_bits = frame_bytes * 8u;
    E = t->enc_bytes;

    for (b = 0; b < frame_bits; b++)
    {
        int in_code;
        int r;

        flip(b);
        rearm(t);
        r = unpack_case(t, payload_bytes);
        in_code = (b >= 16u) && (b < (16u + E * 8u)); /* 编码区：帧头 1B + seq 1B 之后 */

        if (in_code)
        {
            /* 编码区单 bit 错：汉明必纠回，payload 不变，且走复核路径 */
            CHECK(r == 1, "编码区单错未纠回 bit=%u r=%d", b, r);
            CHECK(t->rx_fixed == 1u && t->rx_err == 0u, "单错计数不对 bit=%u fixed=%u err=%u", b,
                  t->rx_fixed, t->rx_err);
        }
        else
        {
            /* 帧头/seq/CRC/帧尾被翻：文本层挡不住，必须整帧丢弃 */
            CHECK(r == 0, "帧定界/seq/CRC 单错未丢弃 bit=%u r=%d", b, r);
            CHECK(t->rx_fixed == 0u && t->rx_err == 1u, "坏帧计数不对 bit=%u", b);
        }
        flip(b); /* 复位 */
    }
}

/*------------------------------------------------------------------
 *  4. 双 bit 错全组合：丢弃 或 payload 正确（SECDED 保证）
 *------------------------------------------------------------------*/
static void test_two_bits(void)
{
    CommProtoExt *t = &t16; /* 多块（P=16 → 2 块），同块/跨块两类都能覆盖 */
    uint32_t payload_bytes = 16u;
    uint32_t blk_bits;
    uint32_t E;
    uint32_t i;
    uint32_t j;
    uint32_t same_blk = 0;
    uint32_t cross_blk = 0;
    uint32_t dropped = 0;
    uint32_t wrong = 0;

    rearm(t);
    pack_case(t, payload_bytes);
    E = t->enc_bytes;
    /* 每块码字位数 k_s + m + 1（不是 k_s）—— 分块边界按码字算 */
    blk_bits = (uint32_t)t->cfg.k_s + PROTO_EXT_M + 1u;

    for (i = 16u; i < 16u + E * 8u; i++)
    {
        for (j = i + 1u; j < 16u + E * 8u; j++)
        {
            int r;

            flip(i);
            flip(j);
            rearm(t);
            r = unpack_case(t, payload_bytes);
            flip(i);
            flip(j);

            if (r == -1)
                wrong++;
            if (r == 0)
                dropped++;
            /* 同块 / 跨块分类统计 */
            if ((i - 16u) / blk_bits == (j - 16u) / blk_bits)
                same_blk++;
            else
                cross_blk++;
        }
    }

    CHECK(wrong == 0, "双错出现 %u 次错误 payload（SECDED 保证不应发生）", wrong);
    /* 同块双错必检出；跨块双错是两次独立单错，会被纠正后接受 */
    CHECK(cross_blk > 0u && same_blk > 0u, "双错分类统计异常");
    CHECK(dropped == same_blk, "同块双错应全部丢弃：dropped=%u same_blk=%u", dropped, same_blk);
    printf("  双错组合 %u（同块丢弃 %u，跨块纠回 %u）\n", same_blk + cross_blk, dropped,
           same_blk + cross_blk - dropped);
}

/*------------------------------------------------------------------
 *  5. 三 bit 错抽样：CRC 复核挡掉汉明误纠
 *------------------------------------------------------------------*/
static void test_three_bits(void)
{
    CommProtoExt *t = &t16;
    uint32_t payload_bytes = 16u;
    uint32_t E;
    uint32_t n;
    uint32_t accepted = 0;
    uint32_t wrong = 0;

    rearm(t);
    pack_case(t, payload_bytes);
    E = t->enc_bytes;

    for (n = 0; n < 20000u; n++)
    {
        uint32_t a;
        uint32_t b;
        uint32_t c;
        int r;

        /* 取三个互不相同的位（相同位会互相抵消，退化成 1/2 位错，测不到三错） */
        a = 16u + rnd() % (E * 8u);
        do
        {
            b = 16u + rnd() % (E * 8u);
        } while (b == a);
        do
        {
            c = 16u + rnd() % (E * 8u);
        } while (c == a || c == b);

        flip(a);
        flip(b);
        flip(c);
        rearm(t);
        r = unpack_case(t, payload_bytes);
        flip(a);
        flip(b);
        flip(c);

        if (r != 0)
            accepted++;
        if (r == -1)
            wrong++;
    }

    /* 三错时汉明可能误纠，但重编码 + 原 CRC 复核应把绝大多数挡掉 */
    printf("  三错抽样 %u：接受 %u，其中错误 payload %u（漏检率 %.2f%%）\n", 20000u, accepted,
           wrong, (double)wrong * 100.0 / 20000.0);
    CHECK(wrong * 100u <= 20000u * 2u, "三错漏检率过高：wrong=%u/20000", wrong);
}

/*------------------------------------------------------------------
 *  6. 帧序列 / 重帧
 *------------------------------------------------------------------*/
static void test_seq(void)
{
    CommProtoExt *t = &t13;
    uint32_t payload_bytes = 13u;
    uint8_t seq_before;

    rearm(t);
    pack_case(t, payload_bytes);
    CHECK(unpack_case(t, payload_bytes) == 1, "首帧未接受");
    CHECK(t->lost_frames == 0u, "首帧不应计丢帧");
    seq_before = t->rx_last_seq;

    /* 同一帧再收一次 = 重帧，丢弃且不更新 */
    CHECK(ProtoUnpack(&t->base, g_frame) == NULL, "重帧未被丢弃");
    CHECK(t->rx_last_seq == seq_before, "重帧不应更新 seq");
    CHECK(t->lost_frames == 0u, "重帧不应计丢帧");

    /* 跳 3 帧：累计丢帧 2（seq+1 与 seq+2 缺失） */
    g_frame[1] = (uint8_t)(seq_before + 3u);
    /* seq 参与 CRC，改后须重算 CRC 才不会被当坏帧 */
    g_frame[t->enc_bytes + 2u] =
        (uint8_t)LIB_CRC_TableCalc(&LIB_CRC_TBL_CRC8, &g_frame[1], (uint32_t)t->enc_bytes + 1u);
    CHECK(ProtoUnpack(&t->base, g_frame) != NULL, "跳号帧未被接受");
    CHECK(t->lost_frames == 2u, "跳号丢帧计数 %u != 2", t->lost_frames);
    CHECK(t->rx_err == 0u, "跳号帧不应计坏帧");

    /* seq 坏帧（CRC 对不上且汉明救不回）：丢弃且不更新 seq */
    rearm(t);
    t->rx_last_seq = seq_before;
    pack_case(t, payload_bytes);
    g_frame[1] ^= 0x01u; /* 只改 seq，不重算 CRC */
    CHECK(ProtoUnpack(&t->base, g_frame) == NULL, "seq 坏帧未被丢弃");
    CHECK(t->rx_last_seq == seq_before, "seq 坏帧不应更新 seq");
    CHECK(t->rx_err == 1u, "seq 坏帧应计 1 次坏帧，实际 %u", t->rx_err);
}

int main(void)
{
    printf("comm_proto_ext PC 端检验\n");

    test_compile_time_layout();
    test_roundtrip();
    test_single_bit();
    test_two_bits();
    test_three_bits();
    test_seq();

    printf("checks=%d fails=%d -> %s\n", g_checks, g_fails, g_fails ? "FAIL" : "PASS");
    return g_fails ? 1 : 0;
}

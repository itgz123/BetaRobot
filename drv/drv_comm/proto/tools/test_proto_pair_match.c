/**
 * @file test_proto_pair_match.c
 * @brief 两端协议名必须一致（PROTO_EXT ↔ PROTO_CUSTOM 错配）PC 端验证，退出码 0 = PASS
 *
 * 背景：PROTO_CUSTOM 与 PROTO_EXT 的线上帧**不等长**（同 payload 13B：17B vs 18B）、
 * 且 CRC 覆盖范围不同（CUSTOM 覆盖 [seq|payload]，EXT 覆盖 [seq|汉明编码区]）。
 * 两端协议名写岔（如底盘侧误写 CUSTOM、云台侧按 EXT 发）时，接收端不是"降级收帧"，
 * 而是**一帧都收不下**——本测试就是把这条约束钉成回归用例：它曾是实际发生过的 bug
 * （app_chassis 的 COMM_DEF 写成 CUSTOM，云台 app_cmd 是 EXT）。
 *
 * @note 用 tools/test_proto_pair_match.sh 一键编译运行（gcc + 同目录 app_cfg.h 桩）。
 * @note 直接链接真实实现（comm_proto_{custom,ext}.c + lib_hamming*.c + lib_crc*.c），
 *       不是参考模型 —— 验的是会上板的那份代码。
 * @note 检验项：
 *       1. 同协议往返（EXT↔EXT、CUSTOM↔CUSTOM，13B/12B）：全部接受（阳性对照，
 *          防止"接收端本来就解不了"把本测试骗成 PASS）
 *       2. 错配 EXT 组帧 → CUSTOM 解帧 / CUSTOM 组帧 → EXT 解帧：接受 0 帧
 *       3. 真实业务载荷（robot_mode + 三个速度）确定性用例：错配 0 帧、正配全收
 *       4. 媒体按本端 rx 帧长多取/少取的那 1 个字节填 0x00/0x5A/0xFF 都试过
 *          （0x5A 是对接收端最有利的填充：万一帧尾判据被放行也还得过 CRC/汉明）
 */

#include <stdio.h>
#include <string.h>

#include "comm_proto_custom.h"
#include "comm_proto_ext.h"

#define RXB 64u /* 接收缓冲（帧最大 18B，留足填字节的空间） */
#define PB 64u  /* payload 缓冲 */

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, ...)                                                                                                                                       \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        g_checks++;                                                                                                                                            \
        if (!(cond))                                                                                                                                           \
        {                                                                                                                                                      \
            g_fails++;                                                                                                                                         \
            if (g_fails <= 30)                                                                                                                                 \
            {                                                                                                                                                  \
                printf("FAIL %d: ", __LINE__);                                                                                                                 \
                printf(__VA_ARGS__);                                                                                                                           \
                printf("\n");                                                                                                                                  \
            }                                                                                                                                                  \
        }                                                                                                                                                      \
    } while (0)

/* 占位媒体：协议层不经过 media，只作 (void *)&media_ 的对象 */
static uint8_t s_stub_media;

COMM_PROTO_EXT_DEF(e13, s_stub_media, 13);
COMM_PROTO_EXT_DEF(e12, s_stub_media, 12);
COMM_PROTO_CUSTOM_DEF(c13, s_stub_media, 13);
COMM_PROTO_CUSTOM_DEF(c12, s_stub_media, 12);

static uint8_t g_payload[PB]; /* 发送端要发的 payload */
static uint8_t g_ref[PB];     /* 上面那份 payload 的副本，用于判"收下来是不是同一份数据" */
static uint8_t g_tx[RXB];     /* 发送端组出的帧 */
static uint8_t g_rx[RXB];     /* 接收端看到的缓冲（= 帧 + 尾部填充） */

static uint32_t g_rng = 0x2468ACE0u;

static uint32_t rnd(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

/* 接收端帧尾下标：CUSTOM = n + 3；EXT = n + 4（编码区比 payload 多 1B） */
#define CUSTOM_TAIL_IDX(n) ((n) + 3u)
#define EXT_TAIL_IDX(n) ((n) + 4u)

typedef struct
{
    uint32_t trials;   /* 试了多少帧 */
    uint32_t accepted; /* 被接收端接受（ProtoUnpack != NULL）的帧数 */
    uint32_t wrong;    /* 接受但 payload 与发送端不一致（比丢帧更糟） */
    uint32_t tail_ok;  /* 接收端帧尾位置上的字节恰为 0x5A 的次数（说明用） */
} PairStat;

/**
 * @brief 用 tx_ 组帧、用 rx_ 解帧，跑 trials 次
 * @param rx_tail_idx 接收端按**自己的**帧长算出的帧尾下标
 * @param fill        接收缓冲里"发送端帧之后"那些字节的填充值
 *                    （模拟媒体按本端帧长多取/少取到的字节；媒体缓冲尺寸由 COMM_DEF 定）
 * @param fed_bytes   实际喂给接收端的字节数 = 发送端帧长（媒体按本端帧长截断/补零）
 */
static PairStat run_pair(CommProto *tx_, CommProto *rx_, uint32_t n, uint32_t trials, uint8_t fill, uint32_t fed_bytes, uint32_t rx_tail_idx)
{
    PairStat st = {0};
    uint32_t i, k;

    st.trials = trials;
    for (i = 0; i < trials; i++)
    {
        const uint8_t *out;

        for (k = 0; k < n; k++)
        {
            g_ref[k] = (uint8_t)rnd();
        }
        memcpy(g_payload, g_ref, n);
        memset(g_tx, 0, sizeof(g_tx));
        if (ProtoPack(tx_, g_payload, g_tx) != 0)
        {
            CHECK(0, "组帧失败");
            break;
        }
        memset(g_rx, fill, sizeof(g_rx));
        memcpy(g_rx, g_tx, fed_bytes);
        out = ProtoUnpack(rx_, g_rx);
        if (out != NULL)
        {
            st.accepted++;
            if (memcmp(out, g_ref, n) != 0)
            {
                st.wrong++;
            }
        }
        st.tail_ok += (g_rx[rx_tail_idx] == 0x5A) ? 1u : 0u;
    }
    return st;
}

static void add_stat(PairStat *acc, const PairStat *one)
{
    acc->trials += one->trials;
    acc->accepted += one->accepted;
    acc->wrong += one->wrong;
    acc->tail_ok += one->tail_ok;
}

static void report(const char *name, const PairStat *st, const char *note)
{
    printf("%-26s 接受 %u / %u", name, st->accepted, st->trials);
    if (st->accepted != 0)
    {
        printf("（payload 不一致 %u）", st->wrong);
    }
    printf("   %s\n", note);
}

/*------------------------------------------------------------------
 *  1. 阳性对照：同协议必须全收（否则本测试的"0 接受"没有意义）
 *------------------------------------------------------------------*/
static void test_same_proto(void)
{
    PairStat st;

    st = run_pair(&e13.base, &e13.base, 13, 20000, 0, 13u + PROTO_EXT_OVERHEAD(13), EXT_TAIL_IDX(13));
    CHECK(st.accepted == st.trials && st.wrong == 0, "EXT→EXT(13B) 未全收：%u/%u", st.accepted, st.trials);
    report("对照 EXT→EXT 13B", &st, "同协议，期望全收");

    st = run_pair(&e12.base, &e12.base, 12, 20000, 0, 12u + PROTO_EXT_OVERHEAD(12), EXT_TAIL_IDX(12));
    CHECK(st.accepted == st.trials && st.wrong == 0, "EXT→EXT(12B) 未全收：%u/%u", st.accepted, st.trials);
    report("对照 EXT→EXT 12B", &st, "同协议，期望全收");

    st = run_pair(&c13.base, &c13.base, 13, 20000, 0, 13u + PROTO_CUSTOM_OVERHEAD(13), CUSTOM_TAIL_IDX(13));
    CHECK(st.accepted == st.trials && st.wrong == 0, "CUSTOM→CUSTOM(13B) 未全收：%u/%u", st.accepted, st.trials);
    report("对照 CUSTOM→CUSTOM 13B", &st, "同协议，期望全收");

    st = run_pair(&c12.base, &c12.base, 12, 20000, 0, 12u + PROTO_CUSTOM_OVERHEAD(12), CUSTOM_TAIL_IDX(12));
    CHECK(st.accepted == st.trials && st.wrong == 0, "CUSTOM→CUSTOM(12B) 未全收：%u/%u", st.accepted, st.trials);
    report("对照 CUSTOM→CUSTOM 12B", &st, "同协议，期望全收");
}

/*------------------------------------------------------------------
 *  2. 错配：任何方向、任何填充都必须 0 接受
 *------------------------------------------------------------------*/
static void test_mismatch(void)
{
    static const uint8_t fills[] = {0x00u, 0x5Au, 0xFFu}; /* 0x5A = 对接收端最有利的填充 */
    static const char *fill_names[] = {"填 0x00", "填 0x5A", "填 0xFF"};
    PairStat sum_ec = {0}; /* EXT 发 → CUSTOM 收 */
    PairStat sum_ce = {0}; /* CUSTOM 发 → EXT 收 */
    PairStat st;
    uint32_t i;

    printf("\n");
    /* 云台按 EXT 发 13B、底盘误按 CUSTOM 收（历史上真实提交过的组合）。
     * 底盘侧 rx 帧长 = 17 < 对端 18 ⇒ 媒体只把前 17B 交给它，EXT 帧的帧尾正好被截掉。 */
    for (i = 0; i < sizeof(fills) / sizeof(fills[0]); i++)
    {
        st = run_pair(&e13.base, &c13.base, 13, 500000, fills[i], 13u + PROTO_CUSTOM_OVERHEAD(13), CUSTOM_TAIL_IDX(13));
        CHECK(st.accepted == 0 && st.wrong == 0, "错配 EXT→CUSTOM 被接受了 %u 帧（%s）", st.accepted, fill_names[i]);
        add_stat(&sum_ec, &st);
    }
    report("错配 EXT发 → CUSTOM收", &sum_ec, "帧尾被截 + CRC 覆盖范围不同 ⇒ 一帧都收不下");

    /* 反向：CUSTOM 短 1B ⇒ 接收端（EXT，18B）会多吃 1 个填字节。填 0x5A 时帧尾判据放行，
     * 只能靠 CRC 覆盖范围 + 汉明复核挡下来——这一段是"最有利于它被错收"的构造。 */
    for (i = 0; i < sizeof(fills) / sizeof(fills[0]); i++)
    {
        st = run_pair(&c13.base, &e13.base, 13, 500000, fills[i], 13u + PROTO_CUSTOM_OVERHEAD(13), EXT_TAIL_IDX(13));
        CHECK(st.accepted == 0 && st.wrong == 0, "错配 CUSTOM→EXT 被接受了 %u 帧（%s）", st.accepted, fill_names[i]);
        add_stat(&sum_ce, &st);
    }
    report("错配 CUSTOM发 → EXT收", &sum_ce, "帧尾放行时靠 CRC/汉明复核挡 ⇒ 仍一帧都收不下");

    /* 帧尾判据被放行的帧数 —— 说明 0 接受不是"帧尾恰好都不对"的运气：
     *   EXT发→CUSTOM收：帧尾下标 16 在对端帧内（是 EXT 的 CRC 字节），随机载荷下恰好等于
     *                   0x5A 的有约 1/256（实测 5831/1.5M），这些帧帧尾判据**是通过的**，
     *                   后面仍被 CRC 全挡；
     *   CUSTOM发→EXT收：帧尾下标 17 在对端帧外，被填充成 0x5A 的那 1/3（= 500000）全放行。
     * 两种情况下后面的 CRC / 汉明复核都是一帧没放过。 */
    printf("  两组里帧尾位置恰为 0x5A（判据被放行）的帧：%u / %u、%u / %u\n", sum_ec.tail_ok, sum_ec.trials, sum_ce.tail_ok, sum_ce.trials);
}

/*------------------------------------------------------------------
 *  3. 真实业务载荷：确定性用例（不是随机数，就是车上那一帧）
 *------------------------------------------------------------------*/
static void pack_real_payload(CommProto *tx_, uint8_t mode, float vx, float vy, float w)
{
    uint8_t p[13];

    p[0] = mode;
    memcpy(p + 1, &vx, 4);
    memcpy(p + 5, &vy, 4);
    memcpy(p + 9, &w, 4);
    memcpy(g_payload, p, 13);
    memcpy(g_ref, p, 13);
    memset(g_tx, 0, sizeof(g_tx));
    CHECK(ProtoPack(tx_, g_payload, g_tx) == 0, "真实载荷组帧失败");
}

static void test_real_payload(void)
{
    static const struct
    {
        const char *name;
        uint8_t mode; /* robot_mode_stop=0 / normal=1 / gyro=2 / hole=3 */
        float vx, vy, w;
    } cases[] = {
        {"stop + 零速度", 0u, 0.0f, 0.0f, 0.0f},
        {"normal + 零速度", 1u, 0.0f, 0.0f, 0.0f},
        {"normal + 带速度", 1u, 0.5f, -0.3f, 1.2f},
    };
    const uint8_t *out;
    uint32_t i;

    printf("\n");
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        /* 云台（EXT）发的实帧 → 底盘（误配 CUSTOM 时） */
        pack_real_payload(&e13.base, cases[i].mode, cases[i].vx, cases[i].vy, cases[i].w);
        memset(g_rx, 0x5A, sizeof(g_rx));
        memcpy(g_rx, g_tx, 13u + PROTO_CUSTOM_OVERHEAD(13));
        out = ProtoUnpack(&c13.base, g_rx);
        CHECK(out == NULL, "真实帧(%s)被误配的 CUSTOM 收下了", cases[i].name);

        /* 同一帧、同一份数据，换成 EXT 收必须收下：
         * 证明上面挡掉的是"协议配错"，不是"帧本身有问题"。 */
        memset(g_rx, 0, sizeof(g_rx));
        memcpy(g_rx, g_tx, 13u + PROTO_EXT_OVERHEAD(13));
        out = ProtoUnpack(&e13.base, g_rx);
        CHECK(out != NULL && memcmp(out, g_ref, 13) == 0, "真实帧(%s) EXT 自己都收不下", cases[i].name);
    }
    printf("真实载荷 %u 组（stop/normal 档 × 零/带速度）：错配全丢、正配全收\n", (unsigned)(sizeof(cases) / sizeof(cases[0])));
}

/*------------------------------------------------------------------
 *  4. 逐字节说明：接收端到底卡在哪一字节
 *------------------------------------------------------------------*/
static void dump_frame(const char *tag, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    printf("  %-24s", tag);
    for (i = 0; i < len; i++)
    {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

static void test_explain(void)
{
    const uint32_t n = 13u;
    const uint32_t e_bytes = n + PROTO_EXT_OVERHEAD(n);    /* 18 */
    const uint32_t c_bytes = n + PROTO_CUSTOM_OVERHEAD(n); /* 17 */

    printf("\n空闲实帧（normal + 零速度，payload 13B）逐字节：\n");
    pack_real_payload(&e13.base, 1u, 0.0f, 0.0f, 0.0f);
    dump_frame("云台 EXT 组帧(18B)", g_tx, e_bytes);
    dump_frame("底盘 CUSTOM 看(17B)", g_tx, c_bytes);
    printf("  CUSTOM 帧尾下标 = payload+3 = %u ⇒ 期望 0x5A，实际 0x%02X\n", CUSTOM_TAIL_IDX(n), g_tx[CUSTOM_TAIL_IDX(n)]);

    pack_real_payload(&c13.base, 1u, 0.0f, 0.0f, 0.0f);
    dump_frame("底盘 CUSTOM 组帧(17B)", g_tx, c_bytes);
    memset(g_rx, 0x5A, sizeof(g_rx));
    memcpy(g_rx, g_tx, c_bytes);
    dump_frame("云台 EXT 看(18B,尾部 5A)", g_rx, e_bytes);
    printf("  EXT 帧尾下标 = payload+4 = %u ⇒ 补上 0x5A 即可放行这道判据，\n"
           "  但 CRC 覆盖范围差一个字节、汉明复核也不过 ⇒ 仍被丢弃（数量见上表）\n",
           EXT_TAIL_IDX(n));
}

int main(void)
{
    printf("comm 两端协议名一致性检验（PROTO_EXT vs PROTO_CUSTOM）\n");

    CHECK(CommProtoExtInit(&e13) == 0 && CommProtoExtInit(&e12) == 0, "EXT Init 失败");
    CHECK(CommProtoCustomInit(&c13) == 0 && CommProtoCustomInit(&c12) == 0, "CUSTOM Init 失败");
    CHECK(PROTO_EXT_OVERHEAD(13) == 5u && PROTO_CUSTOM_OVERHEAD(13) == 4u, "13B payload 开销不是 5/4");
    printf("帧长：payload 13B → EXT %uB / CUSTOM %uB（云台发的是 18B）\n", 13u + PROTO_EXT_OVERHEAD(13), 13u + PROTO_CUSTOM_OVERHEAD(13));

    test_same_proto();
    test_mismatch();
    test_real_payload();
    test_explain();

    printf("\nchecks=%d fails=%d -> %s\n", g_checks, g_fails, (g_fails == 0) ? "PASS" : "FAIL");
    return (g_fails == 0) ? 0 : 1;
}

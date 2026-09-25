/**
 * @file bench_comm_proto_ext.c
 * @brief comm_proto_ext（汉明 + CRC8 帧协议）单帧耗时测量，用于评估 ISR/控制周期预算
 *
 * @note 用 tools/bench_comm_proto_ext.sh 一键编译运行。
 * @note PC 端测得的是 ns/op，不是目标板的周期数：同一段位循环 x86-64（4 发射、
 *       全 1 周期 load）约比 Cortex-M4（单发射、load 2 周期、BitGet/BitSet 走
 *       bl/bx 往返）快 4~5 倍。故目标板估计一律走下面"逐条数反汇编"的解析路径；
 *       PC ns 只用于横向比较（改前 / 改后、不同 payload）。
 * @note 历史：早先 Hamming_ColumnOf 按定义现场穷举（O(k_s²)，k_s=96 时单次编/解码
 *       5229 次内层迭代），反汇编实测约 490 µs/次，2ms 周期里 pack + ISR unpack 就吃掉
 *       53% CPU。改成查表（单条 ldrb）后降到个位数百分比，本文件测得的就是查表版。
 * @note 测量项（每项均分为 pack / 正常 unpack / 纠错 unpack 三条路径，后两者就是
 *       UNPACK_IN_ISR 下中断里真正要跑的）：
 *         pack     —— 发送：汉明编码 + CRC8
 *         unpack-ok—— 接收快路径：CRC 相符，只需汉明解码（无错帧）
 *         unpack-fix —— 接收慢路径：CRC 不符 → 汉明解码 + 重编码复核 + 再 CRC
 *       慢路径只在真有位错时才走，是偶发的最坏值，不作为常态预算。
 */

/* clock_gettime 属 POSIX，-std=c11 默认不暴露（须在包含任何头之前定义） */
#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "comm_proto_ext.h"
#include "lib_crc.h" /* 测试里手工重算 seq 参与的那段 CRC8 */
#include "lib_crc_tables.h"

#define ROUNDS 200000u /* 每项测量轮数 */

static uint8_t s_stub_media;

/* 覆盖半舵链路实际用的 12/13B，再向外扫几档看增长趋势（块数随长度线性增长） */
COMM_PROTO_EXT_DEF(b8, s_stub_media, 8);
COMM_PROTO_EXT_DEF(b12, s_stub_media, 12);
COMM_PROTO_EXT_DEF(b13, s_stub_media, 13);
COMM_PROTO_EXT_DEF(b16, s_stub_media, 16);
COMM_PROTO_EXT_DEF(b32, s_stub_media, 32);
COMM_PROTO_EXT_DEF(b64, s_stub_media, 64);

static uint8_t g_rx[4096];
static uint8_t g_tx[4096];
static uint8_t g_payload[256];

static double now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static uint32_t rng_state = 0x12345678u;

static uint8_t rnd8(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return (uint8_t)rng_state;
}

/* ---- 目标板周期估计：逐条数 Release(-Os) 反汇编 ----
 * 计数对象：build/Release/half_rudder_gimbal.elf，Cortex-M4 单发射 + ART 缓存，
 * 其核心结论是"列值已是单条 ldrb（表在 .rodata，ART 命中 2 周期）"，
 * 开销回到 per-bit 的位读写上：
 *
 *   EncodeBlock 数据位迭代     位=1 53 周期 / 位=0 23 周期 → 随机数据均值 38
 *   EncodeBlock 校验位回填     ~25 周期/位（BitSet 调用占大头，m ≤ 7 可忽略）
 *   DecodeBlock syndrome 迭代  位=1 35 周期 / 位=0 27 周期 → 随机数据均值 31
 *   DecodeBlock 输出段         ~39 周期/数据位（BitGet + 比较 + BitSet 两趟调用）
 *
 * 三个数都含 LIB_Hamming_BitGet/BitSet 的 bl/bx 往返：-Os 下这两个 static inline
 * 没被内联，各自展开成 ~12 / ~20 周期。把它们内联掉是后续最划算的一步优化
 * （可省掉约 1/3），但那是 lib_hamming.c 的事，不在本次查表改动范围内。
 *
 * MISRA-ish 提醒：上面是"数出来的"而非"量出来的"，量测请上板用 DWT->CYCCNT。 */
#define CY_ENC_BIT_AVG 38u /* 编码：每数据位（随机数据均值） */
#define CY_ENC_PAR 25u     /* 编码：每校验位回填 */
#define CY_DEC_CODEBIT 31u /* 解码：syndrome 段每码字位（随机数据均值） */
#define CY_DEC_OUTBIT 39u  /* 解码：输出段每数据位 */

#define F407_MHZ 168u
#define OG_SCALE 1.5 /* Debug(-Og) 相对 Release(-Os) 的倍数（同为调用式，差距比查表前小） */

typedef struct
{
    const char *name;
    CommProtoExt *inst;
    uint32_t payload;
} Case_s;

static const Case_s s_cases[] = {
    {"8B", &b8, 8u}, {"12B", &b12, 12u}, {"13B", &b13, 13u}, {"16B", &b16, 16u}, {"32B", &b32, 32u}, {"64B", &b64, 64u},
};

/* 跑 ROUNDS 次 f，返回每次的平均 ns */
static double time_it(void (*f)(CommProtoExt *, uint32_t), CommProtoExt *t, uint32_t payload)
{
    uint32_t i;
    double t0;
    double t1;

    /* 预热（进 cache、分页） */
    for (i = 0; i < 1000u; i++)
        f(t, payload);

    t0 = now_ns();
    for (i = 0; i < ROUNDS; i++)
        f(t, payload);
    t1 = now_ns();
    return (t1 - t0) / (double)ROUNDS;
}

static void do_pack(CommProtoExt *t, uint32_t payload)
{
    ProtoPack(&t->base, g_payload, g_tx);
    (void)payload;
}

static void do_unpack_ok(CommProtoExt *t, uint32_t payload)
{
    (void)payload;
    /* 每轮换 seq，避免被"重帧"直接挡在序列检测上（那测不到解码本身） */
    g_rx[1] = (uint8_t)(g_rx[1] + 1u);
    g_rx[t->enc_bytes + 2u] = (uint8_t)LIB_CRC_TableCalc(&LIB_CRC_TBL_CRC8, &g_rx[1], (uint32_t)t->enc_bytes + 1u);
    (void)ProtoUnpack(&t->base, g_rx);
}

static void do_unpack_fix(CommProtoExt *t, uint32_t payload)
{
    (void)payload;
    /* 造一个编码区单 bit 错 → 走"汉明解码 + 重编码复核"慢路径；每轮翻/复位 */
    g_rx[1] = (uint8_t)(g_rx[1] + 1u);
    g_rx[t->enc_bytes + 2u] = (uint8_t)LIB_CRC_TableCalc(&LIB_CRC_TBL_CRC8, &g_rx[1], (uint32_t)t->enc_bytes + 1u);
    g_rx[2] ^= 0x80u;
    (void)ProtoUnpack(&t->base, g_rx);
    g_rx[2] ^= 0x80u;
}

int main(void)
{
    uint32_t i;
    uint32_t k;

    printf("comm_proto_ext 单帧耗时（%u 轮均值）\n", ROUNDS);
    printf("⚠ PC 的 ns 别按主频折算到目标板：x86 4 发射 + 全 1 周期 load，比 Cortex-M4\n");
    printf("   快 4~5 倍。目标板估计走「每块 %u/%u 周期 × 位 + 每块 %u/%u 周期 × 位」\n", CY_ENC_BIT_AVG, CY_ENC_PAR,
           CY_DEC_CODEBIT, CY_DEC_OUTBIT);
    printf("   这条逐条数反汇编的解析路径（数值出处见文件头注释）。\n");
    printf("   Debug 列为 Release 值 × %.1f（两者同为调用式，仅作量级参考）。\n\n", (double)OG_SCALE);
    printf("%-5s %-4s %-4s %-4s %-6s %-9s %-9s %-9s %-24s %s\n", "pyld", "帧长", "包", "块", "k_s", "pack(ns)",
           "rx(ns)", "rx+纠(ns)", "F407@168MHz Release(us)", "Debug(us)");
    printf("%-5s %-4s %-4s %-4s %-6s %-9s %-9s %-9s %-24s %s\n\n", "", "B", "CAN", "", "", "", "", "",
           "pack / rx / rx+纠", "(µs)");

    for (k = 0; k < sizeof(s_cases) / sizeof(s_cases[0]); k++)
    {
        CommProtoExt *t = s_cases[k].inst;
        uint32_t payload = s_cases[k].payload;
        uint32_t frame = payload + PROTO_EXT_OVERHEAD(payload);
        uint32_t ks;
        uint32_t m;
        uint32_t blks;
        double blk_enc;
        double blk_dec;
        double cyc_pack;
        double cyc_rx;
        double cyc_fix;
        double tp;
        double to;
        double tf;

        for (i = 0; i < payload; i++)
            g_payload[i] = rnd8();

        if (CommProtoExtInit(t) != 0)
        {
            printf("FAIL: Init 失败 %s\n", s_cases[k].name);
            return 1;
        }
        if (ProtoPack(&t->base, g_payload, g_rx) != 0)
        {
            printf("FAIL: pack 失败 %s\n", s_cases[k].name);
            return 1;
        }

        tp = time_it(do_pack, t, payload);
        to = time_it(do_unpack_ok, t, payload);
        tf = time_it(do_unpack_fix, t, payload);

        /* 目标板估算：整帧 = 块数 × 单块估算（ExtEncode/ExtDecode 逐块调标准层），
         * 再除以主频换成 µs。慢路径包/解各一遍，是"真有位错"时的上界，不进常态预算。 */
        ks = (uint32_t)t->cfg.k_s;
        m = (uint32_t)t->cfg.m;
        blks = PROTO_EXT_BLKS(payload);

        blk_enc = (double)ks * (double)CY_ENC_BIT_AVG + (double)m * (double)CY_ENC_PAR;
        blk_dec = (double)(ks + m) * (double)CY_DEC_CODEBIT + (double)ks * (double)CY_DEC_OUTBIT;
        cyc_pack = (double)blks * blk_enc;
        cyc_rx = (double)blks * blk_dec;
        cyc_fix = cyc_pack + cyc_rx;

        printf("%-5s %-4u %-4u %-4u %-6u %-9.1f %-9.1f %-9.1f %7.0f /%6.0f /%6.0f   %7.0f /%6.0f /%6.0f\n",
               s_cases[k].name, frame, (frame + 7u) / 8u, blks, ks, tp, to, tf, cyc_pack / F407_MHZ, cyc_rx / F407_MHZ,
               cyc_fix / F407_MHZ, cyc_pack * OG_SCALE / F407_MHZ, cyc_rx * OG_SCALE / F407_MHZ,
               cyc_fix * OG_SCALE / F407_MHZ);
    }

    return 0;
}

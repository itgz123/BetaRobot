/**
 * @file test_referee2026.c
 * @brief drv_referee2026 收发内核的 PC 端检验（常规 + 图传 + 非链路三条样本），退出码 0 = PASS
 *
 * @note 用 tools/test_referee2026.sh 一键编译运行（gcc + 同目录的 main.h / bsp_usart.h /
 *       drv_daemon.h / bsp_dwt.h / app_cfg.h 桩）。
 * @note 直接链接真实实现（drv_referee2026.c + public/referee2026_proto.c +
 *       link_common/referee2026_common.c + link_video/referee2026_video.c +
 *       link_none/referee2026_none.c + lib_crc.c），不是参考模型 —— 验的是会上板的那份代码。
 *       只有 bsp / daemon / DWT 三层换成记录桩：它们提供"时间"和"调用记录"，不提供任何协议行为。
 *       三条链路样本覆盖三种形态：常规（周期 RX + 非周期 RX + 只发）、图传（两条 RX 全事件触发）、
 *       非链路（只发不收）。雷达链路的表与快照由头文件里的 `_Static_assert` 在 ARM 构建里把关。
 * @note 检验项：
 *       1. 元信息表与快照的**密排契约**：RX 条目的 snap_off 必须等于前面各段长度之和
 *          （这才是 ISR 里那句 memcpy 安全的依据）、非 RX 条目 snap_off == 0、命令码不重复、
 *          `min_len <= data_len` 且非 0、固定频率不超过上限、RX/TX/双向条数与普查表一致
 *       2. 配置路径：Register/Config 的调用去向、快照三个指针接线（宏里自引用）、过滤掩码展开、
 *          `daemon_reload == 0` 被提升、`rx_enabled`/`rx_periodic` 派生、daemon 的 owner_id 指回内核
 *       3. 好帧：0x0202 派发后快照逐字节相等、tick 非 0、只有 rx_ok/rx_valid 动
 *       4. 七种坏帧各只加自己的计数、快照整块不动（CRC16 错 / 帧头 CRC8 错 / 未知命令码 /
 *          被过滤掩码挡 / 方向不符 / 数据段比表里短 / data_length 超上限）；
 *          另加"比表里长"的帧：按表长截收前 N 字节（协议新增字段不认得，但不丢帧）；
 *          再加变长命令 0x0301：6B（=min_len）收下并**把余下 112B 清零**、5B（<min_len）丢、
 *          118B（=data_len）整段收下
 *       5. 跨事件重组：一包两帧、从帧头中间切开、从帧头/数据交界切开、
 *          噪声前缀里混着假 0xA5（回退重扫后仍能把真帧收回来）
 *       6. TX：方向非法拒发、长度超区间拒发（含变长命令 0x0301 的 6..118）、组帧与独立实现
 *          逐字节相同（含 seq 递增）、在途 BUSY、限速窗口内 BUSY、放行后正常、外设忙时归还"在途"标志
 *       7. bsp/daemon 回调适配：parent 接线、rx_callback 从 parent 取内核、
 *          有合法帧才喂狗、TX_ABORT 清在途、RX_STALLED 不动作、离线钩子重启接收
 *       8. 只发不收的链路（非链路 0x0306）：不起接收、不配 daemon（否则蜂鸣器会一直叫）
 *       9. 无周期 RX 的链路（图传）：daemon 仍装上（离线钩子要用）但故障动作被强制
 *          DAEMON_FAULT_NONE；两条 RX 收得下、只发的 0x0310 发得出、只收的 0x0302 发不出
 */

#include <stdio.h>
#include <string.h>

#include "app_cfg.h"
#include "lib_crc.h"            /* 真实 CRC 算法（查表计算） */
#include "drv_referee2026.h"    /* 暴露接口 + 共享内核（四条链路只此一份） */
#include "referee2026_common.h" /* 真实链路头：表 + 快照 + 实例宏 */
#include "referee2026_video.h"  /* 无周期 RX 的那条链路（daemon 故障动作被强制 NONE） */
#include "referee2026_none.h"   /* 只发不收的那条链路 */
#include "referee2026_proto.h"  /* 真实 CRC 表 */
#include "referee2026_frame.h"

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, ...)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            g_fails++;                                                                                                 \
            if (g_fails <= 30)                                                                                         \
            {                                                                                                          \
                printf("FAIL %d: ", __LINE__);                                                                         \
                printf(__VA_ARGS__);                                                                                   \
                printf("\n");                                                                                          \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

/*============================================
 *   一、记录桩：bsp_usart / drv_daemon / bsp_dwt
 *============================================*/

/** bsp 侧调用记录（每个测试段开始时清空） */
typedef struct
{
    int register_calls;
    int config_calls;
    USART_Config_s last_cfg; /* 最后一次 USARTConfig 收到的配置 */
    int receive_calls;
    uint16_t last_rx_req_len;
    BSP_Transfer_Mode_e last_rx_mode;
    int transmit_calls;
    BSP_Status_e transmit_ret; /* 桩按测试需要返回的码（默认 BSP_OK） */
    BSP_Transfer_Mode_e last_tx_mode;
    uint16_t last_tx_len;
    uint8_t tx_copy[REFEREE2026_FRAME_MAX]; /* 抄一份发给 DMA 的帧，供逐字节比对 */
    int recover_tx_calls;
    uint32_t last_recover_tx_ms;
    int recover_rx_calls;
    uint32_t last_recover_rx_ms;
} BspStub_t;

static BspStub_t g_bsp;

/** daemon 侧调用记录 */
static struct
{
    int register_calls;
    int config_calls;
    Daemon_Config_s last_cfg;
    int reload_calls;
} g_daemon;

/** 假微秒时基：调用后自增 g_step_us，故连续两次调用取到的值不同（与真 DWT 一致） */
static uint64_t g_now_us = 1000u;
static uint32_t g_step_us = 1u;

uint64_t DWT_GetTimeUs(void)
{
    const uint64_t t = g_now_us;

    g_now_us += g_step_us;
    return t;
}

BSP_Status_e USARTRegister(USARTInstance *instance)
{
    (void)instance;
    g_bsp.register_calls++;
    return BSP_OK;
}

/**
 * @brief USARTConfig 桩
 * @note **必须把配置写回实例**（parent / 三个回调）—— 真 bsp 就是这么做的，而驱动的
 *       回调适配全靠 `usart->parent` 取回内核。桩不照做的话，钩子那一整段测试就成了
 *       "测一个真实 bsp 里并不存在的行为"。
 */
BSP_Status_e USARTConfig(USARTInstance *instance, const USART_Config_s *config)
{
    g_bsp.config_calls++;
    g_bsp.last_cfg = *config;

    instance->uart_e = config->uart_e;
    instance->parent = config->parent;
    instance->rx_callback = config->rx_callback;
    instance->tx_callback = config->tx_callback;
    instance->err_callback = config->err_callback;

    return BSP_OK;
}

BSP_Status_e USARTReceive(USARTInstance *instance, uint16_t len, BSP_Transfer_Mode_e mode, uint32_t timeout_ms)
{
    (void)timeout_ms;
    g_bsp.receive_calls++;
    g_bsp.last_rx_req_len = len;
    g_bsp.last_rx_mode = mode;

    /* 真 bsp 起常开流后置 rx_armed（停摆自恢复的判据就靠它） */
    instance->rx_xfer_len = len;
    instance->rx_mode = mode;
    instance->rx_armed = 1u;

    return BSP_OK;
}

BSP_Status_e USARTTransmit(USARTInstance *instance, const uint8_t *data, uint16_t len, BSP_Transfer_Mode_e mode,
                           uint32_t timeout_ms)
{
    (void)instance;
    (void)timeout_ms;
    g_bsp.transmit_calls++;
    g_bsp.last_tx_mode = mode;
    g_bsp.last_tx_len = len;
    memcpy(g_bsp.tx_copy, data, len);
    return g_bsp.transmit_ret;
}

/**
 * @brief 发送卡死自恢复的桩
 * @note **健康时返回 BSP_BUSY**，与真 bsp 一致（`bsp_usart.c` 里只有真卡死才动手，
 *       其余返回 BUSY 表示"不用管"）。驱动把返回值丢掉了，故这个值只影响逼真度。
 */
BSP_Status_e USARTRecoverTxIfStuck(USARTInstance *instance, uint32_t stuck_ms)
{
    (void)instance;
    g_bsp.recover_tx_calls++;
    g_bsp.last_recover_tx_ms = stuck_ms;
    return BSP_BUSY;
}

BSP_Status_e USARTRecoverRxIfStalled(USARTInstance *instance, uint32_t period_ms)
{
    (void)instance;
    g_bsp.recover_rx_calls++;
    g_bsp.last_recover_rx_ms = period_ms;
    return BSP_BUSY;
}

void DaemonRegister(DaemonInstance *instance)
{
    (void)instance;
    g_daemon.register_calls++;
}

void DaemonConfig(DaemonInstance *instance, const Daemon_Config_s *config)
{
    (void)instance;
    g_daemon.config_calls++;
    g_daemon.last_cfg = *config;
}

void DaemonReload(DaemonInstance *instance)
{
    (void)instance;
    g_daemon.reload_calls++;
}

uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    (void)instance;
    return 1u;
}

/*============================================
 *   二、被测实例（用真实的 INSTANCE_DEF 宏定义）
 *============================================*/

REFEREE2026_INSTANCE_DEF(t_inst, COMMON);
REFEREE2026_INSTANCE_DEF(t_video, VIDEO);
REFEREE2026_INSTANCE_DEF(t_none, NONE);

/*============================================
 *   三、小工具
 *============================================*/

/** 内核计数的一份快照（用于算增量） */
typedef struct
{
    uint32_t valid, ok, filtered, unknown, crc, len, tx_ok, tx_dropped;
} Counts_t;

static Counts_t Counts(void)
{
    const Counts_t c = {
        .valid = t_inst.core.rx_valid,
        .ok = t_inst.core.rx_ok,
        .filtered = t_inst.core.rx_filtered,
        .unknown = t_inst.core.rx_unknown,
        .crc = t_inst.core.rx_crc_err,
        .len = t_inst.core.rx_len_err,
        .tx_ok = t_inst.core.tx_ok,
        .tx_dropped = t_inst.core.tx_dropped,
    };

    return c;
}

/**
 * @brief 比较计数的增量，并顺带复核不变式
 * @note 不变式：通过 CRC16 的帧必然被"写快照 / 被过滤 / 不认识"三分类之一接住
 *       （`rx_valid == rx_ok + rx_filtered + rx_unknown`）。它一旦破了，说明有个分支
 *       既没派发也没计数 —— 那是帧"凭空消失"，最难查的一类。
 */
static void CheckCounts(Counts_t base, const char *what, uint32_t d_valid, uint32_t d_ok, uint32_t d_filtered,
                        uint32_t d_unknown, uint32_t d_crc, uint32_t d_len)
{
    const Counts_t c = Counts();

    CHECK(c.valid - base.valid == d_valid, "%s: rx_valid +%u != +%u", what, (unsigned)(c.valid - base.valid),
          (unsigned)d_valid);
    CHECK(c.ok - base.ok == d_ok, "%s: rx_ok +%u != +%u", what, (unsigned)(c.ok - base.ok), (unsigned)d_ok);
    CHECK(c.filtered - base.filtered == d_filtered, "%s: rx_filtered +%u != +%u", what,
          (unsigned)(c.filtered - base.filtered), (unsigned)d_filtered);
    CHECK(c.unknown - base.unknown == d_unknown, "%s: rx_unknown +%u != +%u", what,
          (unsigned)(c.unknown - base.unknown), (unsigned)d_unknown);
    CHECK(c.crc - base.crc == d_crc, "%s: rx_crc_err +%u != +%u", what, (unsigned)(c.crc - base.crc), (unsigned)d_crc);
    CHECK(c.len - base.len == d_len, "%s: rx_len_err +%u != +%u", what, (unsigned)(c.len - base.len), (unsigned)d_len);

    CHECK(c.valid == c.ok + c.filtered + c.unknown, "%s: rx_valid(%u) != ok(%u)+filtered(%u)+unknown(%u)", what,
          (unsigned)c.valid, (unsigned)c.ok, (unsigned)c.filtered, (unsigned)c.unknown);
}

/** 整块快照的哨兵值：Config 之后填空，任何一次写入都能被 SnapshotUntouched 看出来 */
#define SNAP_SENTINEL 0xEEu

static int SnapshotUntouched(void)
{
    const uint8_t *p = (const uint8_t *)&t_inst.snapshot;

    for (size_t i = 0; i < sizeof(t_inst.snapshot); i++)
    {
        if (p[i] != (uint8_t)SNAP_SENTINEL)
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief 按协议手搓一帧（与内核的 TX 路径各自独立实现，正好互为对照）
 * @param out 输出缓冲（至少 REFEREE2026_FRAME_MAX 字节）
 * @param seq 包序号（RX 测试里随便填；TX 回环比对时要与内核的 seq 对上）
 * @return 整帧字节数
 * @note seq 用 0x5A 而不是 0xA5：0xA5 是帧起始符，放在帧内会让"坏帧之后的状态机走向"
 *       难以手推（真机数据里当然会有，状态机也 Handle 得了，只是这里不想引入干扰）。
 */
static uint16_t BuildFrame(uint8_t *out, uint8_t seq, uint16_t cmd_id, const void *data, uint16_t len)
{
    out[REFEREE2026_OFF_SOF] = (uint8_t)REFEREE2026_SOF;
    out[REFEREE2026_OFF_DATA_LEN] = (uint8_t)(len & 0xFFu);
    out[REFEREE2026_OFF_DATA_LEN + 1] = (uint8_t)(len >> 8);
    out[REFEREE2026_OFF_SEQ] = seq;
    out[REFEREE2026_OFF_CRC8] = (uint8_t)LIB_CRC_TableCalc(&referee2026_crc8_table, out, REFEREE2026_OFF_CRC8);
    out[REFEREE2026_OFF_CMD_ID] = (uint8_t)(cmd_id & 0xFFu);
    out[REFEREE2026_OFF_CMD_ID + 1] = (uint8_t)(cmd_id >> 8);

    if (len != 0u)
    {
        memcpy(&out[REFEREE2026_OFF_DATA], data, len);
    }

    const uint16_t n = (uint16_t)(REFEREE2026_OFF_DATA + len);
    const uint16_t crc = (uint16_t)LIB_CRC_TableCalc(&referee2026_crc16_table, out, n);

    out[n] = (uint8_t)(crc & 0xFFu);
    out[n + 1] = (uint8_t)(crc >> 8);

    return (uint16_t)(n + REFEREE2026_TAIL_SIZE);
}

static uint8_t g_frame[REFEREE2026_FRAME_MAX]; /* 手搓的帧 */

/**
 * @brief 填充一段确定且**不含 0xA5** 的字节
 * @note 数据段里出现 0xA5 对状态机没有任何问题（在 BODY 态里它就是一个普通字节），
 *       但"坏帧之后重扫到哪儿"会变得难手推，故构造用例时统一避开。
 */
static void Fill(uint8_t *p, uint16_t n, uint8_t seed)
{
    for (uint16_t i = 0; i < n; i++)
    {
        const uint8_t b = (uint8_t)(seed + i);

        p[i] = (b == (uint8_t)REFEREE2026_SOF) ? (uint8_t)0xA6u : b;
    }
}

/** 清空桩记录、复位假时基、给快照铺哨兵，再跑一次 Config（可重入：状态与计数一并复位） */
static void Relink(uint32_t filter)
{
    memset(&g_bsp, 0, sizeof(g_bsp));
    memset(&g_daemon, 0, sizeof(g_daemon));
    g_bsp.transmit_ret = BSP_OK;
    g_now_us = 1000u;
    g_step_us = 1u;

    memset(&t_inst.snapshot, (int)SNAP_SENTINEL, sizeof(t_inst.snapshot));

    const Referee2026Config_s cfg = {
        .uart_e = UART_1,
        .robot_id = 3u,
        .filter = filter,
        .daemon_reload = 0u, /* 0 应被提升成 REFEREE2026_DAEMON_RELOAD_MS */
        .daemon_fault = DAEMON_FAULT_BUZZER_SHORT,
    };

    CHECK(Referee2026Config(&t_inst.core, &cfg) == BSP_OK, "Config 失败");
}

/** 喂一段字节给内核（模拟 bsp 一次 DMA-IDLE 事件交付的内容） */
static void Feed(const uint8_t *p, uint16_t n)
{
    Referee2026RxIsr(&t_inst.core, p, n);
}

/*============================================
 *   四、检验项
 *============================================*/

/**
 * @brief 元信息表与快照的密排契约
 * @note 这段查的不是"协议对不对"（那是头文件里 _Static_assert 的事），而是**表与快照
 *       对不对得上**：ISR 里 `memcpy(snap + snap_off, ..., data_len)` 的安全性完全建立在
 *       "RX 条目按数据名顺序密排在快照里"这一条上 —— 漏一条、挪一条、改一条长度而没同步
 *       快照，都会变成越界写或静默错位，且编译期查不出来。
 */
static void test_layout(void)
{
    const Referee2026CmdInfo_t *const info = t_inst.core.info;
    uint16_t off = 0;
    uint32_t n_rx = 0;
    uint32_t n_tx_only = 0;
    uint32_t n_dup = 0;
    uint32_t n_freq_bad = 0;

    CHECK(t_inst.core.count == REFEREE2026_COMMON_DATA_COUNT, "count=%u 与枚举数 %u 不符", (unsigned)t_inst.core.count,
          (unsigned)REFEREE2026_COMMON_DATA_COUNT);
    CHECK(info == referee2026_common_cmd_info, "info 未指向本链路的表");

    for (uint16_t i = 0; i < t_inst.core.count; i++)
    {
        const Referee2026CmdInfo_t *const e = &info[i];
        const int is_rx = (e->dir & REFEREE2026_DIR_RX) != 0;
        const int is_tx = (e->dir & REFEREE2026_DIR_TX) != 0;

        CHECK(e->cmd_id != 0u, "第 %u 条命令码为 0（未填）", (unsigned)i);
        CHECK(e->data_len != 0u, "第 %u 条长度为 0（未填）", (unsigned)i);
        CHECK(e->dir != 0u, "第 %u 条方向为 0（未填）", (unsigned)i);
        /* min_len 是"可接受的最短数据段"：表项里**必须逐条写满**（0 不是"默认等于 data_len"
         * 的省略写法 —— 它是合法取值里最小的那个，漏填会变成"0 字节的帧也算合法"）。
         * 定长命令两者相等；变长命令（0x0301）min_len < data_len。 */
        CHECK(e->min_len != 0u, "第 %u 条 min_len 为 0（未填）", (unsigned)i);
        CHECK(e->min_len <= e->data_len, "第 %u 条 min_len=%u > data_len=%u", (unsigned)i, (unsigned)e->min_len,
              (unsigned)e->data_len);

        if (is_rx)
        {
            n_rx++;
            CHECK(e->snap_off == off, "第 %u 条 snap_off=%u，密排期望 %u", (unsigned)i, (unsigned)e->snap_off,
                  (unsigned)off);
            off = (uint16_t)(off + e->data_len);
        }
        else
        {
            /* 非 RX 的命令没有落点；写 0 而不是"前面之和"，且派发路径会按 dir 早退 */
            CHECK(e->snap_off == 0u, "第 %u 条非 RX 命令的 snap_off 应为 0，实为 %u", (unsigned)i,
                  (unsigned)e->snap_off);
        }

        if (is_tx && !is_rx)
        {
            n_tx_only++;
        }

        /* 固定周期的行两列同值（上限就是它本身），非固定周期的 freq_hz == 0 */
        if (e->freq_hz != 0u && e->freq_hz > e->freq_max_hz)
        {
            n_freq_bad++;
        }

        for (uint16_t j = 0; j < i; j++)
        {
            if (info[j].cmd_id == e->cmd_id)
            {
                n_dup++;
            }
        }
    }

    CHECK(n_dup == 0u, "命令码重复 %u 对", (unsigned)n_dup);
    CHECK(n_freq_bad == 0u, "固定频率超过上限的行有 %u 条", (unsigned)n_freq_bad);
    CHECK(off == (uint16_t)sizeof(t_inst.snapshot), "RX 段总长 %u != 快照 %u", (unsigned)off,
          (unsigned)sizeof(t_inst.snapshot));

    /* 与方案里那张普查表对齐：常规链路 24 条 = 21 收 + 3 只发（0x0301 收发双向，算在 RX 里） */
    CHECK(n_rx == 21u, "RX 条数 %u != 21", (unsigned)n_rx);
    CHECK(n_tx_only == 3u, "只发条数 %u != 3", (unsigned)n_tx_only);

    /* 0x0301 是全协议唯一的双向命令：两位都得有，且必须有快照成员（上面的密排循环已经查了
     * "RX 条目 snap_off 密排"，这里再钉一次"它确实被当 RX 对待"）。 */
    CHECK((info[REFEREE2026_COMMON_DATA_ROBOT_INTERACTION].dir & REFEREE2026_DIR_RX) != 0, "0x0301 应有 RX 位");
    CHECK((info[REFEREE2026_COMMON_DATA_ROBOT_INTERACTION].dir & REFEREE2026_DIR_TX) != 0, "0x0301 应有 TX 位");
    /* 它是唯一的变长命令：min_len 是报文头，data_len 是含满额子内容的上界 */
    CHECK(info[REFEREE2026_COMMON_DATA_ROBOT_INTERACTION].min_len == 6u, "0x0301 的 min_len 应为 6（报文头）");
    CHECK(info[REFEREE2026_COMMON_DATA_ROBOT_INTERACTION].min_len <
              info[REFEREE2026_COMMON_DATA_ROBOT_INTERACTION].data_len,
          "0x0301 的 min_len 应小于 data_len（否则就不再是变长命令）");

    /* 只发的那 3 条（0x0305 / 0x0307 / 0x0308）在表里必须都没有 RX 位 */
    CHECK((info[REFEREE2026_COMMON_DATA_MINI_MAP_RADAR].dir & REFEREE2026_DIR_RX) == 0, "0x0305 不该有 RX 位");
    CHECK((info[REFEREE2026_COMMON_DATA_MINI_MAP_PATH].dir & REFEREE2026_DIR_RX) == 0, "0x0307 不该有 RX 位");
    CHECK((info[REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT].dir & REFEREE2026_DIR_RX) == 0, "0x0308 不该有 RX 位");

    printf("  常规链路表 %u 条：RX %u / 只发 %u；RX 段 %u B == 快照 %u B（密排）\n", (unsigned)t_inst.core.count,
           (unsigned)n_rx, (unsigned)n_tx_only, (unsigned)off, (unsigned)sizeof(t_inst.snapshot));
}

/** 配置路径：Register/Config 的调用去向与字段落位 */
static void test_config(void)
{
    /* ① Register（一次、非重入）在 Config 之前，这是本函数唯一一次调用它 */
    memset(&g_bsp, 0, sizeof(g_bsp));
    memset(&g_daemon, 0, sizeof(g_daemon));

    CHECK(Referee2026Register(&t_inst.core) == BSP_OK, "Register 失败");
    CHECK(g_bsp.register_calls == 1, "USARTRegister 调用 %d 次", g_bsp.register_calls);
    CHECK(g_daemon.register_calls == 1, "DaemonRegister 调用 %d 次", g_daemon.register_calls);

    /* ② 之后每段测试都只跑 Config（可重入），它会清桩并复位内核状态 */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);

    /* ② 三个指向实例内部的指针必须都接在实例自己身上（ISR 写的就是它们） */
    CHECK(t_inst.core.snap == (uint8_t *)&t_inst.snapshot, "core.snap 未指向实例快照");
    CHECK(t_inst.core.tick == t_inst.tick, "core.tick 未指向实例 tick");
    CHECK(t_inst.core.tx_last_us == t_inst.tx_last_us, "core.tx_last_us 未指向实例数组");
    CHECK(t_inst.core.robot_id == 3u, "robot_id 未落位");

    /* ③ 过滤掩码：FILTER_DEFAULT 按表的 dir 逐条展开 = 21 条 RX 的那些位。
     *    手算：只发的是 0x0305/0x0307/0x0308，下标 21/22/23，故 0x00FFFFFF 去掉这三位
     *    = bit0..20 = 0x001FFFFF（0x0301 在下标 19，它双向、有 RX 位，故这一位**留着**）。 */
    CHECK(t_inst.core.filter == 0x001FFFFFu, "展开后掩码 0x%08X != 0x001FFFFF", (unsigned)t_inst.core.filter);
    CHECK(t_inst.core.rx_enabled == 1u, "本链路有 RX 命令，rx_enabled 应为 1");
    /* 常规链路有 10Hz 的周期命令（0x0003 等），故"多久没收到"是真判据，daemon 故障动作不被清 */
    CHECK(t_inst.core.rx_periodic == 1u, "本链路有周期 RX 命令，rx_periodic 应为 1");

    /* ④ daemon：reload 填 0 必须被提升（本驱动挂了离线钩子，0 若真禁用就静默失效）；
     *    owner_id 必须指回内核（离线钩子靠它取回实例） */
    CHECK(g_daemon.config_calls == 1, "DaemonConfig 调用 %d 次", g_daemon.config_calls);
    CHECK(g_daemon.last_cfg.reload_count == (uint16_t)REFEREE2026_DAEMON_RELOAD_MS, "reload 未提升：%u",
          (unsigned)g_daemon.last_cfg.reload_count);
    CHECK(g_daemon.last_cfg.fault_action == DAEMON_FAULT_BUZZER_SHORT, "故障动作未透传");
    CHECK(g_daemon.last_cfg.owner_id == (void *)&t_inst.core, "owner_id 未指回内核");
    /* 钩子都是本文件的 static，名字取不到；这里只验"挂上了"，行为在 test_hooks 里经
     * usart 的成员反调验证。 */
    CHECK(g_daemon.last_cfg.callback != NULL, "未挂离线钩子");

    /* ⑤ bsp：uart_e 透传、parent 指回内核、三个回调都装上，且起了接收常开流 */
    CHECK(g_bsp.config_calls == 1, "USARTConfig 调用 %d 次", g_bsp.config_calls);
    CHECK(g_bsp.last_cfg.uart_e == UART_1, "uart_e 未透传");
    CHECK(g_bsp.last_cfg.parent == (void *)&t_inst.core, "parent 未指回内核");
    CHECK(g_bsp.last_cfg.rx_callback != NULL, "rx_callback 未挂");
    CHECK(g_bsp.last_cfg.tx_callback != NULL, "tx_callback 未挂");
    CHECK(g_bsp.last_cfg.err_callback != NULL, "err_callback 未挂");
    CHECK(g_bsp.receive_calls == 1, "USARTReceive 调用 %d 次", g_bsp.receive_calls);
    CHECK(g_bsp.last_rx_req_len == (uint16_t)REFEREE2026_RX_BUFF_SIZE, "接收请求长度 %u != %u",
          (unsigned)g_bsp.last_rx_req_len, (unsigned)REFEREE2026_RX_BUFF_SIZE);
    CHECK(g_bsp.last_rx_mode == BSP_DMA_MODE, "接收模式不是 DMA");
    CHECK(REFEREE2026_RX_BUFF_SIZE >= (uint32_t)REFEREE2026_FRAME_MAX, "接收缓冲小于最长帧");

    /* ⑥ Init 清了 tick / tx_last_us（0 = 从未收到/从未发过） */
    CHECK(t_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT] == 0u, "tick 未被 Init 清零");
    CHECK(t_inst.tx_last_us[REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT] == 0u, "tx_last_us 未被 Init 清零");

    /* ⑦ 空指针必须被挡住 */
    CHECK(Referee2026Config(NULL, NULL) == BSP_PARAM_ERR, "Config(NULL) 未报参数错");
    CHECK(Referee2026Send(NULL, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, g_frame, 0u) == BSP_PARAM_ERR,
          "Send(NULL) 未报参数错");
}

/** 好帧：一条 0x0202 走完全程 */
static void test_good_frame(void)
{
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);

    Referee2026PowerHeat_t ph = {0};

    ph.reserved0 = 0x1111u;
    ph.reserved1 = 0x2222u;
    ph.buffer_energy = 0x3344u;
    ph.shooter_17mm_barrel_heat = 0x5566u;
    ph.shooter_42mm_barrel_heat = 0x7788u;

    const Counts_t base = Counts();
    const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0202u, &ph, (uint16_t)sizeof(ph));

    Feed(g_frame, n);

    CHECK(n == 9u + (uint16_t)sizeof(ph), "整帧长 %u 与 9+len 不符", (unsigned)n);
    CHECK(memcmp(&t_inst.snapshot.power_heat, &ph, sizeof(ph)) == 0, "快照 0x0202 与载荷不等");
    CHECK(t_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT] != 0u, "tick 未写（0 兼作从未收到的哨兵）");
    CheckCounts(base, "好帧", 1u, 1u, 0u, 0u, 0u, 0u);

    /* seqlock 的读法：这期间没有新帧，两次读 tick 必须一致（app 就是靠这个判"读稳了"） */
    const uint32_t t0 = t_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT];

    CHECK(t0 == t_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT], "tick 两次读不一致");

    /* 别的成员一个都不该动 */
    const uint8_t *const p = (const uint8_t *)&t_inst.snapshot;
    const uint16_t off = t_inst.core.info[REFEREE2026_COMMON_DATA_POWER_HEAT].snap_off;
    uint32_t dirty = 0;

    for (size_t i = 0; i < sizeof(t_inst.snapshot); i++)
    {
        if (i < off || i >= (size_t)off + sizeof(ph))
        {
            dirty += (p[i] != (uint8_t)SNAP_SENTINEL) ? 1u : 0u;
        }
    }

    CHECK(dirty == 0u, "快照里有 %u 个字节越界被改", (unsigned)dirty);

    printf("  0x0202：数据段 %u B、整帧 %u B，快照落点偏移 %u\n", (unsigned)sizeof(ph), (unsigned)n, (unsigned)off);
}

/** 七种坏帧 + 一种"比表里长"：各只加自己的计数，快照整块不动 */
static void test_bad_frames(void)
{
    /* ① CRC16 错：帧头全对、数据全对，只把帧尾改一个字节 */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
    {
        Referee2026PowerHeat_t ph = {0};

        Fill((uint8_t *)&ph, (uint16_t)sizeof(ph), 0x10u);

        const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0202u, &ph, (uint16_t)sizeof(ph));

        g_frame[n - 1u] ^= 0xFFu; /* 只坏 CRC16 的高字节 */

        const Counts_t base = Counts();

        Feed(g_frame, n);
        CheckCounts(base, "CRC16 错", 0u, 0u, 0u, 0u, 1u, 0u);
        CHECK(SnapshotUntouched(), "CRC16 错却动了快照");
        CHECK(t_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT] == 0u, "CRC16 错却写了 tick");
    }

    /* ② 帧头 CRC8 错：把 data_length 的低字节改掉（CRC8 覆盖前 4B，故必然不过）。
     *    然后**重扫**那 4 个残留字节（它们都不是 0xA5），故 crc_err 恰好 +1。 */
    {
        Referee2026PowerHeat_t ph = {0};

        Fill((uint8_t *)&ph, (uint16_t)sizeof(ph), 0x20u);

        const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0202u, &ph, (uint16_t)sizeof(ph));

        g_frame[REFEREE2026_OFF_DATA_LEN] ^= 0x01u;

        const Counts_t base = Counts();

        Feed(g_frame, n);
        CheckCounts(base, "帧头 CRC8 错", 0u, 0u, 0u, 0u, 1u, 0u);
        CHECK(SnapshotUntouched(), "帧头 CRC8 错却动了快照");
    }

    /* ③ 未知命令码：CRC 全对，但表里没有 0x0FFF */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
    {
        uint8_t payload[4];

        Fill(payload, sizeof(payload), 0x30u);

        const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0FFFu, payload, (uint16_t)sizeof(payload));
        const Counts_t base = Counts();

        Feed(g_frame, n);
        CheckCounts(base, "未知命令码", 1u, 0u, 0u, 1u, 0u, 0u);
        CHECK(SnapshotUntouched(), "未知命令码却动了快照");
    }

    /* ④ 被过滤掩码挡：只收 0x0001，喂 0x0202 */
    Relink(REFEREE2026_COMMON_FILTER_GAME_STATUS);
    {
        Referee2026PowerHeat_t ph = {0};

        Fill((uint8_t *)&ph, (uint16_t)sizeof(ph), 0x40u);

        const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0202u, &ph, (uint16_t)sizeof(ph));
        const Counts_t base = Counts();

        Feed(g_frame, n);
        CheckCounts(base, "被过滤掩码挡", 1u, 0u, 1u, 0u, 0u, 0u);
        CHECK(SnapshotUntouched(), "被过滤却动了快照");
        CHECK(t_inst.core.filter == (uint32_t)REFEREE2026_COMMON_FILTER_GAME_STATUS, "掩码未按配置生效");
    }

    /* ⑤ 方向不符：掩码给"24 位全开"（**不等于** UINT32_MAX，故不会被展开成"只 RX 的那些"），
     *    喂一条只发不收的 0x0305 —— 唯一能挡住它的就是 dir 这道门。
     *    这一条是 `dir` 字段存在的运行期理由：0x0305 的 snap_off 是 0，放进去会踩坏快照首成员。
     *    （0x0301 曾是这条测试的靶子，但它现在是双向的，挡不住了 —— 换 0x0305。） */
    Relink((Referee2026CommonFilter_e)0x00FFFFFFu);
    {
        uint8_t payload[REFEREE2026_COMMON_LEN_MINI_MAP_RADAR];

        Fill(payload, (uint16_t)sizeof(payload), 0x50u);

        const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0305u, payload, (uint16_t)sizeof(payload));
        const Counts_t base = Counts();

        CHECK(t_inst.core.info[REFEREE2026_COMMON_DATA_MINI_MAP_RADAR].snap_off == 0u, "0x0305 的 snap_off 应为 0");
        Feed(g_frame, n);
        CheckCounts(base, "方向不符", 1u, 0u, 1u, 0u, 0u, 0u);
        CHECK(SnapshotUntouched(), "方向不符却动了快照");
    }

    /* ⑥ 数据段比表里短：0x0202 只带 4B。长度是"最小可接受"，短了存不进快照 ⇒
     *    归入 rx_filtered（与"被掩码挡掉"同一类结果），rx_len_err 记"为什么" */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
    {
        uint8_t payload[4];

        Fill(payload, sizeof(payload), 0x60u);

        const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0202u, payload, (uint16_t)sizeof(payload));
        const Counts_t base = Counts();

        Feed(g_frame, n);
        CheckCounts(base, "数据段过短", 1u, 0u, 1u, 0u, 0u, 1u);
        CHECK(SnapshotUntouched(), "过短的帧却动了快照");
        CHECK(t_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT] == 0u, "过短的帧却写了 tick");
    }

    /* ⑦ data_length 超过上限：手搓一个只有帧头的"巨值长度"帧（CRC8 是对的，
     *    坏的是长度字段本身 —— 故计 rx_len_err 而不是 rx_crc_err） */
    {
        g_frame[REFEREE2026_OFF_SOF] = (uint8_t)REFEREE2026_SOF;
        g_frame[REFEREE2026_OFF_DATA_LEN] = 0x2Du; /* 301 = DATA_MAX + 1 */
        g_frame[REFEREE2026_OFF_DATA_LEN + 1] = 0x01u;
        g_frame[REFEREE2026_OFF_SEQ] = 0x5Au;
        g_frame[REFEREE2026_OFF_CRC8] =
            (uint8_t)LIB_CRC_TableCalc(&referee2026_crc8_table, g_frame, REFEREE2026_OFF_CRC8);

        const Counts_t base = Counts();

        Feed(g_frame, (uint16_t)REFEREE2026_HEADER_SIZE);
        CheckCounts(base, "长度超上限", 0u, 0u, 0u, 0u, 0u, 1u);
        CHECK(SnapshotUntouched(), "超长帧却动了快照");
        CHECK(REFEREE2026_DATA_MAX < 301u, "DATA_MAX 应小于 301");
    }

    /* ⑧ 比表里长：协议将来新增字段时真机会这么发 —— 按表长截收前 N 字节，不丢帧 */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
    {
        Referee2026PowerHeat_t ph = {0};
        uint8_t payload[REFEREE2026_COMMON_LEN_POWER_HEAT + 6u];

        Fill(payload, (uint16_t)sizeof(payload), 0x70u);
        memcpy(&ph, payload, sizeof(ph)); /* 前 14B 就是落在快照里的那 14B */

        const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0202u, payload, (uint16_t)sizeof(payload));
        const Counts_t base = Counts();

        Feed(g_frame, n);
        CheckCounts(base, "比表里长", 1u, 1u, 0u, 0u, 0u, 0u);
        CHECK(memcmp(&t_inst.snapshot.power_heat, &ph, sizeof(ph)) == 0, "截收的头 14B 与载荷不符");
    }

    /* ⑨ 变长命令 0x0301 的**下界**：它 data_len=118（含满额子内容）、min_len=6（只有报文头）。
     *    分三档验：6B（=min，合法）→ 收下并**把余下 112B 清零**；5B（<min）→ 丢、快照不动；
     *    118B（=data_len）→ 整段收下。这一条是 min_len 字段存在的运行期理由 —— 没有它，
     *    6B 的合法报文会被当成"数据段过短"丢掉。 */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
    {
        const uint16_t member_off = t_inst.core.info[REFEREE2026_COMMON_DATA_ROBOT_INTERACTION].snap_off;
        const uint8_t *const snap = (const uint8_t *)&t_inst.snapshot;
        /* 快照里该命令那一段的基址（用 inst 自己的成员取，避免依赖 snap_off 的正确性） */
        const uint8_t *const seg = (const uint8_t *)&t_inst.snapshot.robot_interaction;

        CHECK(seg == snap + member_off, "0x0301 的 snap_off 与成员地址对不上");

        /* ⑨-1 恰好 min_len：数据段 6B */
        {
            uint8_t payload[6] = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u};

            const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0301u, payload, (uint16_t)sizeof(payload));
            const Counts_t base = Counts();

            Feed(g_frame, n);
            CheckCounts(base, "0x0301 最短报文", 1u, 1u, 0u, 0u, 0u, 0u);
            CHECK(t_inst.tick[REFEREE2026_COMMON_DATA_ROBOT_INTERACTION] != 0u, "0x0301 最短报文却未写 tick");
            CHECK(memcmp(seg, payload, sizeof(payload)) == 0, "0x0301 报文头 6B 与载荷不符");

            /* 余下 112B 必须是**零**，不能留着 Config 时填的哨兵 0xEE（那会被读成"真有数据"） */
            uint32_t stale = 0;

            for (size_t i = sizeof(payload); i < (size_t)REFEREE2026_COMMON_LEN_ROBOT_INTERACTION; i++)
            {
                stale += (seg[i] != 0u) ? 1u : 0u;
            }

            CHECK(stale == 0u, "0x0301 短报文的余下 %u 字节未清零", (unsigned)stale);
            /* 也别越界写到别的成员：整块快照里只有这一段被动过 */
            uint32_t others = 0;

            for (size_t i = 0; i < sizeof(t_inst.snapshot); i++)
            {
                if (i < (size_t)member_off || i >= (size_t)member_off + REFEREE2026_COMMON_LEN_ROBOT_INTERACTION)
                {
                    others += (snap[i] != (uint8_t)SNAP_SENTINEL) ? 1u : 0u;
                }
            }

            CHECK(others == 0u, "0x0301 短报文越界改了 %u 个字节", (unsigned)others);
        }

        /* ⑨-2 比 min_len 差一个字节：数据段 5B ⇒ 丢（同时计入 rx_filtered 与 rx_len_err） */
        Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
        {
            uint8_t payload[5] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u};

            const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0301u, payload, (uint16_t)sizeof(payload));
            const Counts_t base = Counts();

            Feed(g_frame, n);
            CheckCounts(base, "0x0301 短于 min_len", 1u, 0u, 1u, 0u, 0u, 1u);
            CHECK(SnapshotUntouched(), "0x0301 过短帧却动了快照");
            CHECK(t_inst.tick[REFEREE2026_COMMON_DATA_ROBOT_INTERACTION] == 0u, "0x0301 过短帧却写了 tick");
        }

        /* ⑨-3 满额 data_len：整段 118B 收下 */
        Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
        {
            Referee2026RobotInteraction_t ri;

            Fill((uint8_t *)&ri, (uint16_t)sizeof(ri), 0xC0u);

            const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0301u, &ri, (uint16_t)sizeof(ri));
            const Counts_t base = Counts();

            Feed(g_frame, n);
            CheckCounts(base, "0x0301 满额报文", 1u, 1u, 0u, 0u, 0u, 0u);
            CHECK(memcmp(&t_inst.snapshot.robot_interaction, &ri, sizeof(ri)) == 0, "0x0301 满额载荷不等");
        }
    }
}

/** 跨 DMA-IDLE 事件的重组 */
static void test_reassembly(void)
{
    /* ① 一个事件里两帧（921600 的图传链路上是常态） */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
    {
        uint8_t pay_a[REFEREE2026_COMMON_LEN_ROBOT_STATUS];
        uint8_t pay_b[REFEREE2026_COMMON_LEN_POWER_HEAT];
        uint8_t buf[2u * REFEREE2026_FRAME_MAX];
        Referee2026PowerHeat_t ph = {0};
        uint16_t n = 0;

        Fill(pay_a, (uint16_t)sizeof(pay_a), 0x80u);
        Fill(pay_b, (uint16_t)sizeof(pay_b), 0x90u);
        memcpy(&ph, pay_b, sizeof(ph));

        n += BuildFrame(&buf[n], 0x5Au, 0x0201u, pay_a, (uint16_t)sizeof(pay_a));
        n += BuildFrame(&buf[n], 0x5Bu, 0x0202u, pay_b, (uint16_t)sizeof(pay_b));

        const Counts_t base = Counts();

        Feed(buf, n);
        CheckCounts(base, "一包两帧", 2u, 2u, 0u, 0u, 0u, 0u);
        CHECK(memcmp(&t_inst.snapshot.robot_status, pay_a, sizeof(pay_a)) == 0, "第一帧 0x0201 不等");
        CHECK(memcmp(&t_inst.snapshot.power_heat, &ph, sizeof(ph)) == 0, "第二帧 0x0202 不等");
        CHECK(t_inst.tick[REFEREE2026_COMMON_DATA_ROBOT_STATUS] != 0u, "第一帧 tick 未写");
        CHECK(t_inst.tick[REFEREE2026_COMMON_DATA_POWER_HEAT] != 0u, "第二帧 tick 未写");
    }

    /* ② 半帧 + 半帧分两次到：切点分别落在帧头中间、帧头/数据交界、数据中间 */
    const uint16_t cuts[] = {3u, (uint16_t)REFEREE2026_HEADER_SIZE, 9u};

    for (size_t k = 0; k < sizeof(cuts) / sizeof(cuts[0]); k++)
    {
        Relink(REFEREE2026_COMMON_FILTER_DEFAULT);

        Referee2026PowerHeat_t ph = {0};

        Fill((uint8_t *)&ph, (uint16_t)sizeof(ph), (uint8_t)(0xA0u + k));

        const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0202u, &ph, (uint16_t)sizeof(ph));
        const uint16_t cut = cuts[k];
        const Counts_t base = Counts();

        CHECK(cut > 0u && cut < n, "切点 %u 越界（帧长 %u）", (unsigned)cut, (unsigned)n);

        Feed(g_frame, cut);
        CHECK(t_inst.core.rx_ok == 0u, "切点 %u：半帧就派发了", (unsigned)cut);
        CHECK(t_inst.core.rx_valid == 0u, "切点 %u：半帧就计入 valid", (unsigned)cut);

        Feed(&g_frame[cut], (uint16_t)(n - cut));
        CheckCounts(base, "半帧+半帧", 1u, 1u, 0u, 0u, 0u, 0u);
        CHECK(memcmp(&t_inst.snapshot.power_heat, &ph, sizeof(ph)) == 0, "切点 %u：重组后载荷不等", (unsigned)cut);
    }

    /* ③ 噪声前缀里混着一个假 0xA5：真帧的起点正好落在那个假帧头的 5B 窗口内，
     *    于是**回退重扫**要把真帧捞回来（这正是 RxRewind 存在的理由）。
     *    代价是 1 次 crc_err（假帧头没过 CRC8），结果不能少。 */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
    {
        Referee2026PowerHeat_t ph = {0};
        uint8_t buf[8u + REFEREE2026_FRAME_MAX];
        const uint8_t noise[4] = {0x00u, 0xA5u, 0x00u, 0xAAu};
        uint16_t n = 0;

        Fill((uint8_t *)&ph, (uint16_t)sizeof(ph), 0xB0u);

        memcpy(buf, noise, sizeof(noise));
        n = (uint16_t)sizeof(noise);
        n += BuildFrame(&buf[n], 0x5Au, 0x0202u, &ph, (uint16_t)sizeof(ph));

        /* 前提：假帧头 [A5,00,AA,A5] 的 CRC8 必须对不上真帧头的第 5 个字节（0x0E），
         * 否则这条用例走的就不是"回退"分支了 */
        uint8_t probe[4] = {0xA5u, 0x00u, 0xAAu, 0xA5u};
        const uint8_t crc_calc = (uint8_t)LIB_CRC_TableCalc(&referee2026_crc8_table, probe, 4u);

        CHECK(crc_calc != g_frame[REFEREE2026_OFF_CRC8], "假帧头恰好也是合法的（用例失效）");

        const Counts_t base = Counts();

        Feed(buf, n);
        CheckCounts(base, "噪声+假帧头", 1u, 1u, 0u, 0u, 1u, 0u);
        CHECK(memcmp(&t_inst.snapshot.power_heat, &ph, sizeof(ph)) == 0, "重扫后载荷不等");
    }

    /* ④ 空交付：IDLE 事件没有数据。什么都不该发生 */
    {
        const Counts_t base = Counts();

        Feed(g_frame, 0u);
        Referee2026RxIsr(&t_inst.core, NULL, 4u); /* 空指针要挡住，不能崩 */
        Referee2026RxIsr(NULL, g_frame, 4u);
        CheckCounts(base, "空交付", 0u, 0u, 0u, 0u, 0u, 0u);
    }
}

/** TX：方向/长度校验、组帧、限速、在途 */
static void test_tx(void)
{
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);

    Referee2026PowerHeat_t ph = {0};
    uint8_t tx_payload[REFEREE2026_COMMON_LEN_MINI_MAP_ROBOT];

    Fill((uint8_t *)&ph, (uint16_t)sizeof(ph), 0xC0u);
    Fill(tx_payload, (uint16_t)sizeof(tx_payload), 0xD0u);

    /* ① 方向：0x0202 收得到，但本链路发不出去 */
    const Counts_t base = Counts();

    CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_POWER_HEAT, &ph, (uint16_t)sizeof(ph)) == BSP_PARAM_ERR,
          "给收命令发帧应报参数错");
    CHECK(g_bsp.transmit_calls == 0, "被拒的帧仍然调了 USARTTransmit");

    /* ② 长度不符 */
    CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, tx_payload,
                          (uint16_t)sizeof(tx_payload) - 1u) == BSP_PARAM_ERR,
          "长度不符应报参数错");
    CHECK(Referee2026Send(&t_inst.core, (Referee2026CommonDataId_e)REFEREE2026_COMMON_DATA_COUNT, tx_payload,
                          (uint16_t)sizeof(tx_payload)) == BSP_PARAM_ERR,
          "下标越界应报参数错");
    CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, NULL, 34u) == BSP_PARAM_ERR,
          "空载荷应报参数错");
    CHECK(g_bsp.transmit_calls == 0, "参数错的帧调了 USARTTransmit");
    CHECK(Counts().tx_ok - base.tx_ok == 0u, "参数错的帧计了 tx_ok");

    /* ③ 正常发一帧：组帧结果与独立实现逐字节相同（seq 从 0 起） */
    {
        const uint16_t n_ok = Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, tx_payload,
                                              (uint16_t)sizeof(tx_payload));

        CHECK(n_ok == BSP_OK, "发送失败：%d", (int)n_ok);
        CHECK(g_bsp.transmit_calls == 1, "USARTTransmit 调用 %d 次", g_bsp.transmit_calls);

        const uint16_t expect_len = (uint16_t)(9u + sizeof(tx_payload));

        CHECK(g_bsp.last_tx_len == expect_len, "整帧长 %u != %u", (unsigned)g_bsp.last_tx_len, (unsigned)expect_len);
        CHECK(g_bsp.last_tx_mode == BSP_DMA_MODE, "发送模式不是 DMA（发送缓冲要能走 DMA）");

        const uint16_t n = BuildFrame(g_frame, 0u, 0x0308u, tx_payload, (uint16_t)sizeof(tx_payload));

        CHECK(n == expect_len, "独立实现整帧长 %u != %u", (unsigned)n, (unsigned)expect_len);
        CHECK(memcmp(g_bsp.tx_copy, g_frame, n) == 0, "组帧与独立实现不一致");
        CHECK(t_inst.core.tx_ok == 1u, "tx_ok=%u", (unsigned)t_inst.core.tx_ok);
        CHECK(t_inst.tx_last_us[REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT] != 0u, "tx_last_us 未记");
    }

    /* ④ 在途：tx_callback 还没来，再发一帧必须被拒（无队列，与 bsp 既定策略一致） */
    {
        const Counts_t b = Counts();

        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, tx_payload,
                              (uint16_t)sizeof(tx_payload)) == BSP_BUSY,
              "在途时未报忙");
        CHECK(Counts().tx_dropped - b.tx_dropped == 1u, "在途丢弃未计数");
        CHECK(g_bsp.transmit_calls == 1, "在途时仍调了 USARTTransmit"); /* 被拒的帧连组帧都没做 */
    }

    /* ⑤ 完成回调清了在途，但时间没走 ⇒ 撞的是**限速**（0x0308 上限 3Hz） */
    t_inst.core.usart->tx_callback(t_inst.core.usart);
    CHECK(t_inst.core.tx_busy == 0u, "TxDone 未清在途");
    {
        const Counts_t b = Counts();

        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, tx_payload,
                              (uint16_t)sizeof(tx_payload)) == BSP_BUSY,
              "限速窗口内未报忙");
        CHECK(Counts().tx_dropped - b.tx_dropped == 1u, "限速丢弃未计数");
    }

    /* ⑥ 时间走过最小间隔（1e6/3 = 333333µs）⇒ 放行，且 seq 递增到 1 */
    {
        g_now_us += 333333u;

        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, tx_payload,
                              (uint16_t)sizeof(tx_payload)) == BSP_OK,
              "过了限速窗口仍被拒");

        const uint16_t n = BuildFrame(g_frame, 1u, 0x0308u, tx_payload, (uint16_t)sizeof(tx_payload));

        CHECK(memcmp(g_bsp.tx_copy, g_frame, n) == 0, "第二帧 seq 未递增到 1");
        CHECK(t_inst.core.tx_ok == 2u, "tx_ok=%u", (unsigned)t_inst.core.tx_ok);
    }

    /* ⑦ 外设忙：USARTTransmit 返回错误 ⇒ 原样透传、计入丢弃、且"在途"标志必须被归还
     *    （不归还的话这一位再没人清，发送永久卡死） */
    {
        t_inst.core.usart->tx_callback(t_inst.core.usart);
        g_now_us += 333333u;
        g_bsp.transmit_ret = BSP_HW_ERR;

        const Counts_t b = Counts();

        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, tx_payload,
                              (uint16_t)sizeof(tx_payload)) == BSP_HW_ERR,
              "外设错误未透传");
        CHECK(Counts().tx_dropped - b.tx_dropped == 1u, "外设错误未计入丢弃");
        CHECK(t_inst.core.tx_busy == 0u, "外设错误后未归还在途标志");

        /* 归还之后应能正常再发 */
        g_bsp.transmit_ret = BSP_OK;
        g_now_us += 333333u;
        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, tx_payload,
                              (uint16_t)sizeof(tx_payload)) == BSP_OK,
              "归还后仍发不出去");
    }

    /* ⑧ 变长命令 0x0301 的发送：长度判据是 `min_len <= len <= data_len`（6..118）。
     *    6B（只有报文头）必须能发出去 —— 没有 min_len、"必须恰好等于 118"的话它会被挡掉；
     *    5B 与 119B 两侧都要拒。定长命令不受影响（两者相等 ⇒ 仍是"恰好等于"）。 */
    t_inst.core.usart->tx_callback(t_inst.core.usart); /* 清掉 ⑦ 留下的在途 */
    {
        uint8_t head[6] = {1u, 2u, 3u, 4u, 5u, 6u};
        uint8_t short_buf[5] = {0};
        uint8_t over[REFEREE2026_COMMON_LEN_ROBOT_INTERACTION + 1u] = {0};

        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_ROBOT_INTERACTION, short_buf,
                              (uint16_t)sizeof(short_buf)) == BSP_PARAM_ERR,
              "0x0301 数据段短于 min_len 应报参数错");
        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_ROBOT_INTERACTION, over, (uint16_t)sizeof(over)) ==
                  BSP_PARAM_ERR,
              "0x0301 数据段长于 data_len 应报参数错");

        const Counts_t b = Counts();

        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_ROBOT_INTERACTION, head, (uint16_t)sizeof(head)) ==
                  BSP_OK,
              "0x0301 最短报文（6B）发不出去");
        CHECK(g_bsp.last_tx_len == (uint16_t)(9u + sizeof(head)), "0x0301 最短报文整帧长 %u != %u",
              (unsigned)g_bsp.last_tx_len, (unsigned)(9u + sizeof(head)));
        CHECK(Counts().tx_ok - b.tx_ok == 1u, "0x0301 最短报文未计 tx_ok");
    }

    /* ⑨ 入口确实调了发送卡死自恢复（0 = 让 bsp 用它的默认阈值）。
     *    这一步必须在"在途判断"之前，否则真卡死时再也走不到 USARTTransmit。 */
    CHECK(g_bsp.recover_tx_calls > 0, "发送入口未调 USARTRecoverTxIfStuck");
    CHECK(g_bsp.last_recover_tx_ms == 0u, "卡死阈值应为 0（用 bsp 默认值）");
}

/** bsp / daemon 的回调适配 */
static void test_hooks(void)
{
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);

    Referee2026PowerHeat_t ph = {0};

    Fill((uint8_t *)&ph, (uint16_t)sizeof(ph), 0xE0u);

    const uint16_t n = BuildFrame(g_frame, 0x5Au, 0x0202u, &ph, (uint16_t)sizeof(ph));
    USARTInstance *const usart = t_inst.core.usart;

    /* ① rx_callback：bsp 把缓冲交过来，内核经 parent 取回自己 */
    CHECK(usart->rx_buff != NULL, "bsp 实例没有接收缓冲");
    CHECK(usart->rx_buff_size >= n, "接收缓冲装不下这条帧");

    memcpy(usart->rx_buff, g_frame, n);
    usart->rx_len = n;
    usart->rx_callback(usart);

    CHECK(memcmp(&t_inst.snapshot.power_heat, &ph, sizeof(ph)) == 0, "经钩子喂入后快照不等");
    CHECK(g_daemon.reload_calls == 1, "收到合法帧却没喂狗（reload %d 次）", g_daemon.reload_calls);

    /* ② 只有噪声 ⇒ 一帧都没通过校验 ⇒ 不喂狗 */
    memset(usart->rx_buff, 0x11, 16);
    usart->rx_len = 16;
    usart->rx_callback(usart);
    CHECK(g_daemon.reload_calls == 1, "没收到帧却喂了狗");

    /* ③ 被过滤的帧同样喂狗：看门狗答的是"链路还活着吗" */
    {
        const Referee2026Config_s cfg = {
            .uart_e = UART_1,
            .robot_id = 3u,
            .filter = REFEREE2026_COMMON_FILTER_GAME_STATUS, /* 不收 0x0202 */
            .daemon_reload = 100u,
            .daemon_fault = DAEMON_FAULT_NONE,
        };

        CHECK(Referee2026Config(&t_inst.core, &cfg) == BSP_OK, "Config 失败");
        g_daemon.reload_calls = 0;

        memcpy(usart->rx_buff, g_frame, n);
        usart->rx_len = n;
        usart->rx_callback(usart);
        CHECK(t_inst.core.rx_filtered == 1u, "被过滤的帧未计数");
        CHECK(g_daemon.reload_calls == 1, "被过滤的帧没喂狗（链路是活着的）");
        CHECK(g_daemon.last_cfg.reload_count == 100u, "非 0 的 reload 应原样使用");
    }

    /* ④ TxDoneHook 清在途 */
    Relink(REFEREE2026_COMMON_FILTER_DEFAULT);
    {
        uint8_t pay[REFEREE2026_COMMON_LEN_MINI_MAP_ROBOT];

        Fill(pay, (uint16_t)sizeof(pay), 0xF0u);
        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, pay, (uint16_t)sizeof(pay)) ==
                  BSP_OK,
              "发送失败");
        CHECK(t_inst.core.tx_busy == 1u, "发送后未置在途");

        usart->tx_callback(usart);
        CHECK(t_inst.core.tx_busy == 0u, "tx_callback 未清在途");
    }

    /* ⑤ err_callback 分流：TX_ABORT 清在途；RX_STALLED 与 HW 什么都不做
     *    （RX_STALLED 里绝不能调 USARTRecoverRxIfStalled —— 那是 ISR 上下文，会死等） */
    {
        uint8_t pay[REFEREE2026_COMMON_LEN_MINI_MAP_ROBOT];

        Fill(pay, (uint16_t)sizeof(pay), 0xF0u);
        g_now_us += 333333u; /* 越过 0x0308 的 3Hz 窗口，让这一发真能出去 */

        CHECK(Referee2026Send(&t_inst.core, REFEREE2026_COMMON_DATA_MINI_MAP_ROBOT, pay, (uint16_t)sizeof(pay)) ==
                  BSP_OK,
              "发送失败");
        CHECK(t_inst.core.tx_busy == 1u, "发送后未置在途");

        usart->err_callback(usart, USART_ERR_RX_STALLED);
        CHECK(t_inst.core.tx_busy == 1u, "RX_STALLED 不该动在途标志");
        CHECK(g_bsp.recover_rx_calls == 0, "RX_STALLED 在 ISR 里重启了接收（会死等）");

        usart->err_callback(usart, USART_ERR_HW);
        CHECK(t_inst.core.tx_busy == 1u, "HW 不该动在途标志（判不了，交给 Send 入口）");

        usart->err_callback(usart, USART_ERR_TX_ABORT);
        CHECK(t_inst.core.tx_busy == 0u, "TX_ABORT 未清在途（此后 tx_callback 不会再来）");
    }

    /* ⑥ 离线钩子（任务上下文）：重启接收，判据与限频都在 bsp 内。
     *    钩子是 static，名字取不到 —— 经 daemon 配置里记下的回调指针反调，正是 DaemonTask 的走法。 */
    CHECK(g_daemon.last_cfg.callback != NULL, "daemon 未挂离线钩子");
    g_daemon.last_cfg.callback(g_daemon.last_cfg.owner_id);
    CHECK(g_bsp.recover_rx_calls == 1, "离线钩子未重启接收");
    CHECK(g_bsp.last_recover_rx_ms == (uint32_t)REFEREE2026_RX_RESTART_PERIOD_MS, "重启限频周期不符");

    /* ⑦ 空指针不崩（经实例上的回调指针调，那里存的就是本文件的 static 适配函数） */
    usart->rx_callback(NULL);
    usart->tx_callback(NULL);
    usart->err_callback(NULL, USART_ERR_TX_ABORT);
    g_daemon.last_cfg.callback(NULL);
    g_checks += 1; /* 走到这儿就是没崩 */
}

/**
 * @brief 只发不收的链路（非链路 0x0306）
 * @note 这段验的是"从根上不挂 daemon"：DaemonTask 在离线期间**每拍**都重放故障动作，
 *       给一条永远收不到帧的链路配上 daemon，蜂鸣器就会一直叫。
 */
static void test_tx_only_link(void)
{
    memset(&g_bsp, 0, sizeof(g_bsp));
    memset(&g_daemon, 0, sizeof(g_daemon));
    g_bsp.transmit_ret = BSP_OK;

    const Referee2026Config_s cfg = {
        .uart_e = UART_6,
        .robot_id = 3u,
        .filter = REFEREE2026_NONE_FILTER_DEFAULT,
        .daemon_reload = 100u,
        .daemon_fault = DAEMON_FAULT_BUZZER_LONG,
    };

    CHECK(Referee2026Register(&t_none.core) == BSP_OK, "非链路 Register 失败");
    CHECK(Referee2026Config(&t_none.core, &cfg) == BSP_OK, "非链路 Config 失败");

    CHECK(t_none.core.count == REFEREE2026_NONE_DATA_COUNT, "非链路表长不符");
    CHECK(t_none.core.rx_enabled == 0u, "本链路没有 RX 命令，rx_enabled 应为 0");
    CHECK(t_none.core.filter == 0u, "没有 RX 命令 ⇒ 展开后掩码必为 0");
    CHECK(g_bsp.config_calls == 1, "USARTConfig 调用 %d 次", g_bsp.config_calls);
    CHECK(g_bsp.receive_calls == 0, "只发不收的链路却起了接收常开流");
    CHECK(g_daemon.config_calls == 0, "只发不收的链路却配了 daemon（会永久喂不上狗）");

    /* 发一帧：0x0306 是这条链路唯一（也是只发）的命令 */
    Referee2026CustomClientData_t cc = {0};

    cc.key_value = 0x1234u;
    cc.x_position = 0x321u;
    cc.y_position = 0x654u;
    cc.mouse_left = 1u;
    cc.mouse_right = 0u;

    CHECK(Referee2026Send(&t_none.core, REFEREE2026_NONE_DATA_CUSTOM_CLIENT_DATA, &cc, (uint16_t)sizeof(cc)) == BSP_OK,
          "非链路发送失败");

    const uint16_t n = BuildFrame(g_frame, 0u, 0x0306u, &cc, (uint16_t)sizeof(cc));

    CHECK(g_bsp.transmit_calls == 1, "USARTTransmit 调用 %d 次", g_bsp.transmit_calls);
    CHECK(g_bsp.last_tx_len == n, "整帧长 %u != %u", (unsigned)g_bsp.last_tx_len, (unsigned)n);
    CHECK(memcmp(g_bsp.tx_copy, g_frame, n) == 0, "非链路组帧与独立实现不一致");

    CHECK(Referee2026Send(&t_none.core, REFEREE2026_NONE_DATA_CUSTOM_CLIENT_DATA, &cc, 7u) == BSP_PARAM_ERR,
          "长度不符应报参数错");
    CHECK(Referee2026Config(NULL, NULL) == BSP_PARAM_ERR, "Config(NULL) 未报参数错");
    CHECK(Referee2026Send(NULL, REFEREE2026_NONE_DATA_CUSTOM_CLIENT_DATA, &cc, (uint16_t)sizeof(cc)) == BSP_PARAM_ERR,
          "Send(NULL) 未报参数错");
}

/**
 * @brief 图传链路：两条 RX 全是**事件触发**（`freq_hz == 0`）
 * @note 这段验的是"没有周期 RX 的链路，离线判据不成立"：daemon 仍要装上（离线钩子还要用），
 *       但故障动作必须被 `Config` 强制成 NONE —— 否则对端正常但没在下发时，链路空闲会被判成
 *       离线、蜂鸣器在空档里一直叫。`rx_periodic` 这个派生字段就是为此存在的。
 */
static void test_video_link(void)
{
    memset(&g_bsp, 0, sizeof(g_bsp));
    memset(&g_daemon, 0, sizeof(g_daemon));
    g_bsp.transmit_ret = BSP_OK;

    const Referee2026Config_s cfg = {
        .uart_e = UART_6,
        .robot_id = 3u,
        .filter = REFEREE2026_VIDEO_FILTER_DEFAULT,
        .daemon_reload = 0u, /* 0 仍要被提升成默认值（离线钩子要用这个节拍） */
        .daemon_fault = DAEMON_FAULT_BUZZER_SHORT,
    };

    CHECK(Referee2026Register(&t_video.core) == BSP_OK, "图传 Register 失败");
    CHECK(Referee2026Config(&t_video.core, &cfg) == BSP_OK, "图传 Config 失败");

    CHECK(t_video.core.count == REFEREE2026_VIDEO_DATA_COUNT, "图传表长不符");
    CHECK(t_video.core.rx_enabled == 1u, "图传有 2 条 RX，rx_enabled 应为 1");
    CHECK(t_video.core.rx_periodic == 0u, "图传两条 RX 全是事件触发，rx_periodic 应为 0");
    /* 展开后 = 两条 RX 的位：0x0302 是下标 0、0x0311 是下标 3 ⇒ 1<<0 | 1<<3 = 0x9 */
    CHECK(t_video.core.filter == 0x9u, "图传展开后掩码 0x%08X != 0x9", (unsigned)t_video.core.filter);

    /* daemon 仍要装上（离线钩子是接收停摆后唯一的重启入口），但故障动作必须被强制 NONE */
    CHECK(g_daemon.config_calls == 1, "图传未配 daemon（接收停摆自恢复会一起失效）");
    CHECK(g_daemon.last_cfg.fault_action == DAEMON_FAULT_NONE, "无周期 RX 的链路未强制 NONE，实为 %d",
          (int)g_daemon.last_cfg.fault_action);
    CHECK(g_daemon.last_cfg.reload_count == (uint16_t)REFEREE2026_DAEMON_RELOAD_MS, "图传 reload 未提升：%u",
          (unsigned)g_daemon.last_cfg.reload_count);
    CHECK(g_daemon.last_cfg.callback != NULL, "图传未挂离线钩子");
    CHECK(g_bsp.receive_calls == 1, "图传未起接收常开流");
    CHECK(g_bsp.last_rx_mode == BSP_DMA_MODE, "图传接收不是 DMA 模式");

    /* 两条 RX 都收得下（图传 921600 下一包里连着的两帧） */
    {
        uint8_t pay_a[REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER];
        uint8_t pay_b[REFEREE2026_VIDEO_LEN_CLIENT_TO_ROBOT];
        uint8_t buf[2u * REFEREE2026_FRAME_MAX];
        uint16_t n = 0;

        Fill(pay_a, (uint16_t)sizeof(pay_a), 0x21u);
        Fill(pay_b, (uint16_t)sizeof(pay_b), 0x31u);

        n += BuildFrame(&buf[n], 0x11u, 0x0302u, pay_a, (uint16_t)sizeof(pay_a));
        n += BuildFrame(&buf[n], 0x12u, 0x0311u, pay_b, (uint16_t)sizeof(pay_b));

        Referee2026RxIsr(&t_video.core, buf, n);

        CHECK(t_video.core.rx_ok == 2u, "图传 rx_ok=%u != 2", (unsigned)t_video.core.rx_ok);
        CHECK(t_video.core.rx_filtered == 0u, "图传有帧被挡");
        CHECK(memcmp(&t_video.snapshot.custom_controller, pay_a, sizeof(pay_a)) == 0, "0x0302 快照不等");
        CHECK(memcmp(&t_video.snapshot.client_to_robot, pay_b, sizeof(pay_b)) == 0, "0x0311 快照不等");
        CHECK(t_video.tick[REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER] != 0u, "0x0302 tick 未写");
        CHECK(t_video.tick[REFEREE2026_VIDEO_DATA_CLIENT_TO_ROBOT] != 0u, "0x0311 tick 未写");
    }

    /* 方向：只发的 0x0310 发得出去；只收的 0x0302 发不出去（图传机器人这侧是终点） */
    {
        uint8_t pay[REFEREE2026_VIDEO_LEN_ROBOT_TO_CLIENT];
        uint8_t rx_pay[REFEREE2026_VIDEO_LEN_CUSTOM_CONTROLLER];

        Fill(pay, (uint16_t)sizeof(pay), 0x41u);
        Fill(rx_pay, (uint16_t)sizeof(rx_pay), 0x42u);

        CHECK(Referee2026Send(&t_video.core, REFEREE2026_VIDEO_DATA_ROBOT_TO_CLIENT, pay, (uint16_t)sizeof(pay)) ==
                  BSP_OK,
              "图传 0x0310 发不出去");
        CHECK(Referee2026Send(&t_video.core, REFEREE2026_VIDEO_DATA_CUSTOM_CONTROLLER, rx_pay,
                              (uint16_t)sizeof(rx_pay)) == BSP_PARAM_ERR,
              "只收的 0x0302 竟然发出去了");
    }
}

int main(void)
{
    printf("referee2026 PC 端检验（常规 + 图传 + 非链路）\n");

    test_layout();
    test_config();
    test_good_frame();
    test_bad_frames();
    test_reassembly();
    test_tx();
    test_hooks();
    test_tx_only_link();
    test_video_link();

    printf("checks=%d fails=%d -> %s\n", g_checks, g_fails, g_fails ? "FAIL" : "PASS");

    return g_fails ? 1 : 0;
}

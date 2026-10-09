/**
 * @file drv_referee2026.c
 * @brief 2026 赛季裁判系统驱动的**暴露接口实现** —— 四条链路只此一份的 Register/Config/Send + RX 内核
 *
 * 结构：
 *   一、小工具（小端取存 / 命令码查表）
 *   二、RX：重组状态机（逐字节）→ 校验 → 过滤 → 写快照
 *   三、TX：校验 → 限速 → 组帧 → 启动 DMA
 *   四、生命周期：Register / Config（含内部 Init）
 *   五、bsp 与 daemon 的**静态**回调适配
 *
 * **四条链路的收发接口都在这儿**：那四份 `Referee2026<Link>Register` / `_Config` / `_Send`
 * 逐字同构，收进本文件后各链路只提供"本链路长什么样"（表 + 快照 + 实例），**一行函数都不定义**。
 * 调用方传 `&inst.core`；`Config` 也由本文件从零走完"存 ID → 初始化内核 → 装 bsp 回调 →
 * 装 daemon → 起接收"，链路侧不再有中间层。
 *
 * 本文件**不含任何链路特有知识**（命令码、长度、快照字段、过滤枚举），全部经 `core->info` /
 * `core->snap` / `core->filter` 取得 —— 改动链路的东西不该动到这里。
 *
 * 生效条件：`DRV_REFEREE2026_USED`（app_cfg.h 里开）且 `HAL_UART_MODULE_ENABLED`。
 * 未开时本文件编译为空 TU（与 public/referee2026_proto.c 同一写法）。
 */

#include "drv_referee2026.h"

#ifdef HAL_UART_MODULE_ENABLED

#include <string.h>

#include "app_cfg.h" /* DRV_REFEREE2026_USED */
#include "bsp_dwt.h" /* DWT_GetTimeUs —— ISR 里唯一可用的时基 */
#include "lib_crc.h" /* LIB_CRC_TableCalc */

#include "referee2026_frame.h" /* Referee2026FrameOffset_e / Referee2026FrameConst_e（public/） */
#include "referee2026_proto.h" /* referee2026_crc8_table / referee2026_crc16_table（public/） */

#if defined(DRV_REFEREE2026_USED)

/* 本文件后半段才定义的回调适配，生命周期一节要用 */
static void Referee2026CoreRxHook(USARTInstance *usart);
static void Referee2026CoreTxDoneHook(USARTInstance *usart);
static void Referee2026CoreErrHook(USARTInstance *usart, USART_ErrReason_e reason);
static void Referee2026CoreOfflineHook(void *owner);

/*============================================
 *   一、小工具
 *============================================*/

/**
 * @brief 帧内取 16 位小端
 * @note 不用 `*(uint16_t *)p`：协议数据段的指针不保证 2 字节对齐，虽然 Cortex-M4/M7 能
 *       非对齐访问，但那是实现定义行为 —— 帧头这几处拆字节更省事也更可靠。
 */
static inline uint16_t Referee2026CoreGetU16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/** @brief 帧内写 16 位小端 */
static inline void Referee2026CorePutU16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

/**
 * @brief 命令码 → 数据名下标
 * @return 下标；表里没有该命令码时返回 -1
 * @note 线性扫表。常规链路 24 项、图传 4 项，在 ISR 里可接受（最坏 24 次 16 位比较），
 *       不值得为它上哈希或二分（表本来就没排序，二分还要先排）。
 */
static int16_t Referee2026CoreFindId(const Referee2026Core_t *core, uint16_t cmd_id)
{
    for (uint16_t i = 0; i < core->count; i++)
    {
        if (core->info[i].cmd_id == cmd_id)
        {
            return (int16_t)i;
        }
    }

    return -1;
}

/*============================================
 *   二、RX：重组状态机
 *============================================*/

/* 两个函数互相调用（rewind 把残留字节回喂给 byte），故先声明。 */
static void Referee2026CoreRxByte(Referee2026Core_t *core, uint8_t b);
static void Referee2026CoreRxRewind(Referee2026Core_t *core);

/**
 * @brief 复位重组状态：丢掉半帧，回到找起点
 * @note **不清 `asm_buf` 内容** —— 长度由 `asm_len` 单独记着，清 309B 的缓冲纯属浪费
 *       （这一句在 ISR 里跑）。
 */
static void Referee2026CoreRxReset(Referee2026Core_t *core)
{
    core->asm_len = 0;
    core->asm_total = 0;
    core->asm_state = REFEREE2026_RX_ST_SOF;
}

/**
 * @brief 整帧到齐：校 CRC16 → 查表 → 方向/过滤 → 写快照 + 时间戳
 * @note 全程在 ISR 上下文，只做定长比较与一次 memcpy，无循环无日志。
 */
static void Referee2026CoreRxDispatch(Referee2026Core_t *core)
{
    const uint16_t frame_len = core->asm_total;

    /* ① CRC16：覆盖「帧头 5B + cmd_id 2B + 数据段」，不含自身 2B 帧尾 */
    const uint16_t crc_rx = Referee2026CoreGetU16(&core->asm_buf[frame_len - REFEREE2026_TAIL_SIZE]);
    const uint16_t crc_calc = (uint16_t)LIB_CRC_TableCalc(&referee2026_crc16_table, core->asm_buf, (uint32_t)(frame_len - REFEREE2026_TAIL_SIZE));

    if (crc_rx != crc_calc)
    {
        core->rx_crc_err++;
        return;
    }

    /* 到这儿就算"收到了一条协议合法的帧"，喂狗用它 —— 被过滤/不认识的帧同样证明链路活着 */
    core->rx_valid++;

    /* ② 命令码 → 数据名 */
    const int16_t data_id = Referee2026CoreFindId(core, Referee2026CoreGetU16(&core->asm_buf[REFEREE2026_OFF_CMD_ID]));

    if (data_id < 0)
    {
        core->rx_unknown++;
        return;
    }

    const Referee2026CmdInfo_t *info = &core->info[data_id];

    /* ③ 方向：本链路收不到的命令，即便调用方把过滤位打开了也不写。
     *    这道门与过滤掩码是两回事 —— 过滤是"我今天关不关心"，方向是"协议上我收不收得到"；
     *    更重要的是：非 RX 命令的 `snap_off` 是 0，放进去会踩坏快照的第一个成员。 */
    if ((info->dir & REFEREE2026_DIR_RX) == 0u)
    {
        core->rx_filtered++;
        return;
    }

    /* ④ 过滤掩码：ISR 里真正省时间的一步（0x0307 一条就 105B 的 memcpy） */
    if (((core->filter >> (uint32_t)data_id) & 1u) == 0u)
    {
        core->rx_filtered++;
        return;
    }

    /* ⑤ 长度：真机长度以帧头为准，`min_len`/`data_len` 是可接受区间的下上界（定长命令两者相等）。
     *    比 `min_len` 还短 ⇒ 丢（快照成员的前几个字节都凑不齐，写进去没有意义）。
     *    这一支**同时计入 `rx_filtered`**：帧认得、CRC 也过了，只是没写进快照 —— 与"被掩码
     *    挡掉"是同一类结果，`rx_len_err` 回答的是"为什么"（它是原因计数，不是分类计数）。
     *    分开计的话 `rx_valid == rx_ok + rx_filtered + rx_unknown` 就破了（见 drv_referee2026.h 的计数表）。 */
    const uint16_t seg_len = (uint16_t)(frame_len - REFEREE2026_OFF_DATA - REFEREE2026_TAIL_SIZE);

    if (seg_len < info->min_len)
    {
        core->rx_filtered++;
        core->rx_len_err++;
        return;
    }

    /* ⑥ 拷贝长度 = min(收到的, 快照成员尺寸)。长出来的部分是协议将来新增的字段（或变长命令
     *    多带的子内容），本模块还不认识，只留前 `data_len` 字节；短了则**把余下清零** ——
     *    否则那个成员里会留着上一帧的尾巴，读起来比读到 0 更像"真有数据"。这一步只对变长
     *    命令（0x0301）会走到：定长命令的 `seg_len >= data_len`，memcpy 长度恒为 `data_len`。 */
    const uint16_t n_copy = (seg_len < info->data_len) ? seg_len : info->data_len;

    /* ⑦ 写快照：tick 当头尾序号（seqlock）。两次写入**通常**不同（中间隔了一次 memcpy + 一次
     *    (uint32_t)DWT_GetTimeUs()），但**不保证** —— 最短的数据段（1B）在高主频 M7 上可以
     *    整个 memcpy 都跑在同一个 µs 里。真正的读侧判据不依赖这一点：app 读的是
     *    "tick 相对**上一帧**变了没有"，而 ISR 相对任务上下文是原子的，故两次写入即使同值
     *    也不会让 app 漏检这次更新（最坏是白重读一次）。 */
    core->tick[data_id] = (uint32_t)DWT_GetTimeUs();
    memcpy(core->snap + info->snap_off, core->asm_buf + REFEREE2026_OFF_DATA, n_copy);

    if (n_copy < info->data_len)
    {
        memset(core->snap + info->snap_off + n_copy, 0, (size_t)(info->data_len - n_copy));
    }

    core->tick[data_id] = (uint32_t)DWT_GetTimeUs();

    core->rx_ok++;
}

/**
 * @brief 帧头 CRC8 不过：把已收的 [1..asm_len) 挪到最前，回喂给状态机重扫
 *
 * 这一帧的 0xA5 不是真起点，真起点很可能就在这 5B 之内（比如线上一帧被打断，下一个 0xA5
 * 紧跟在旧帧的残留里）。参考工程在 ISR 里用尾递归处理多帧，本函数**递归深度有界**：
 * 每次回退至少前进 1 字节，而此刻 `asm_len == HEADER_SIZE == 5`，故最多嵌 4 层。
 */
static void Referee2026CoreRxRewind(Referee2026Core_t *core)
{
    uint8_t tail[REFEREE2026_HEADER_SIZE];
    const uint16_t n = core->asm_len;

    for (uint16_t i = 1; i < n; i++)
    {
        tail[i - 1] = core->asm_buf[i];
    }

    Referee2026CoreRxReset(core);

    for (uint16_t i = 1; i < n; i++)
    {
        Referee2026CoreRxByte(core, tail[i - 1]);
    }
}

/**
 * @brief 喂一个字节进重组状态机（RX 的核心）
 * @note 三个状态各自 return，不 fall through；每次进 BODY 攒满即派发并复位，
 *       故一个事件里连着几帧也能逐个处理（921600 的图传链路上是常态）。
 */
static void Referee2026CoreRxByte(Referee2026Core_t *core, uint8_t b)
{
    switch (core->asm_state)
    {
    case REFEREE2026_RX_ST_SOF: {
        /* 未对齐的字节直接丢。0xA9 图传遥控帧不以 0xA5 打头，与 0xA5 混跑时天然被跳过
         * （本轮不解析 0xA9；等哪天要收遥控帧，在这里按 0xA9 另起一条分支）。 */
        if (b != (uint8_t)REFEREE2026_SOF)
        {
            return;
        }

        core->asm_buf[0] = b;
        core->asm_len = 1;
        core->asm_state = REFEREE2026_RX_ST_HEADER;
        return;
    }

    case REFEREE2026_RX_ST_HEADER: {
        core->asm_buf[core->asm_len++] = b;

        if (core->asm_len < (uint16_t)REFEREE2026_HEADER_SIZE)
        {
            return;
        }

        /* 帧头满了，先校 CRC8（覆盖前 4B：SOF / data_length 小端 / seq，不含 CRC8 自身）。
         * 一失败就重扫，**不等收满整帧** —— 这是噪声下最快回到对齐的路。 */
        const uint8_t crc8_calc = (uint8_t)LIB_CRC_TableCalc(&referee2026_crc8_table, core->asm_buf, REFEREE2026_OFF_CRC8);

        if (crc8_calc != core->asm_buf[REFEREE2026_OFF_CRC8])
        {
            core->rx_crc_err++;
            Referee2026CoreRxRewind(core);
            return;
        }

        const uint16_t data_len = Referee2026CoreGetU16(&core->asm_buf[REFEREE2026_OFF_DATA_LEN]);

        /* 长度字段被噪声填成巨值：直接判错复位，别拿它去算要收多少字节。
         * 计入 `rx_len_err` 而不是 `rx_crc_err` —— 帧头 CRC8 是过的，坏的是长度字段本身。 */
        if (data_len > (uint16_t)REFEREE2026_DATA_MAX)
        {
            core->rx_len_err++;
            Referee2026CoreRxReset(core);
            return;
        }

        core->asm_total = (uint16_t)(REFEREE2026_HEADER_SIZE + REFEREE2026_CMD_ID_SIZE + data_len + REFEREE2026_TAIL_SIZE);
        core->asm_state = REFEREE2026_RX_ST_BODY;
        return;
    }

    case REFEREE2026_RX_ST_BODY: {
        core->asm_buf[core->asm_len++] = b;

        if (core->asm_len < core->asm_total)
        {
            return;
        }

        Referee2026CoreRxDispatch(core);
        Referee2026CoreRxReset(core);
        return;
    }

    default: {
        /* 不该到这儿（`asm_state` 由本文件独占写）。真到了就复位重来，别让状态机卡死。 */
        Referee2026CoreRxReset(core);
        return;
    }
    }
}

void Referee2026RxIsr(Referee2026Core_t *core, const uint8_t *buf, uint16_t len)
{
    if (core == NULL || buf == NULL)
    {
        return;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        Referee2026CoreRxByte(core, buf[i]);
    }
}

/*============================================
 *   三、TX
 *============================================*/

/** @brief 发送完成：清"在途"（本函数只在 bsp 的 tx_callback 里被调） */
static void Referee2026CoreTxDone(Referee2026Core_t *core)
{
    if (core != NULL)
    {
        core->tx_busy = 0;
    }
}

BSP_Status_e Referee2026Send(Referee2026Core_t *core, uint16_t data_id, const void *data, uint16_t len)
{
    /* `usart` 也必须判：Register / Config 都判了，Send 少了这一道的话，判据三个入口不一致，
     * 且后面 `USARTRecoverTxIfStuck(NULL, ...)` 虽然安全（bsp 侧有 NULL 守卫）却会白走一趟。 */
    if (core == NULL || core->usart == NULL || data == NULL)
    {
        return BSP_PARAM_ERR;
    }

    if (data_id >= core->count)
    {
        return BSP_PARAM_ERR;
    }

    const Referee2026CmdInfo_t *info = &core->info[data_id];

    /* ① 方向：这条命令本链路根本发不出去。常规链路 24 条里 20 条如此，写错一个枚举成员
     *    就是往裁判系统总线上打一帧非法数据 —— 这是 `dir` 字段唯一的运行期消费者。 */
    if ((info->dir & REFEREE2026_DIR_TX) == 0u)
    {
        return BSP_PARAM_ERR;
    }

    /* ② 长度：35 条命令的数据段类型各不相同，C 里给不出统一的 payload 类型，长度只能由调用方
     *    给 —— 这一次比较就是防"拷越界/拷不全"的兜底。区间是 [min_len, data_len]：定长命令
     *    两者相等，等价于"必须恰好等于"（行为与拆分前一致）；变长命令（0x0301）允许在区间内
     *    取任意值——子内容多长就发多长，不必补到 118B。 */
    if (len < info->min_len || len > info->data_len)
    {
        return BSP_PARAM_ERR;
    }

    /* ③ 发送卡死自恢复必须在"在途判断"**之前**：真卡死时 `tx_busy` 永远不会被清，而本函数
     *    的各个提前返回都在 `USARTTransmit` 之前 —— 少了这一步就再也走不到那一行，复位永远
     *    没机会执行，链接永久静默。它经 err_callback(TX_ABORT) 把 `tx_busy` 清掉（见 ErrHook）。 */
    (void)USARTRecoverTxIfStuck(core->usart, 0u);

    /* ④ 单写者、无队列：一帧在途就拒（排队是上层的事，同 bsp 的既定策略） */
    if (core->tx_busy != 0u)
    {
        core->tx_dropped++;
        return BSP_BUSY;
    }

    /* ⑤ 限速：`freq_max_hz == 0` 表示协议没给速率约束，不限 */
    const uint32_t now = (uint32_t)DWT_GetTimeUs();

    if (info->freq_max_hz != 0u)
    {
        const uint32_t min_us = 1000000u / (uint32_t)info->freq_max_hz;
        const uint32_t last = core->tx_last_us[data_id];

        /* last == 0 兼作"从未发过"（DWT 恰好返回 0 只有上电后头 1µs，可接受） */
        if (last != 0u && (uint32_t)(now - last) < min_us)
        {
            core->tx_dropped++;
            return BSP_BUSY;
        }
    }

    /* ⑥ 组帧。写进内核自持的 `tx_buf`：DMA 是异步读，绝不能用调用方的栈缓冲。 */
    uint8_t *const f = core->tx_buf;

    f[REFEREE2026_OFF_SOF] = (uint8_t)REFEREE2026_SOF;
    Referee2026CorePutU16(&f[REFEREE2026_OFF_DATA_LEN], len);
    f[REFEREE2026_OFF_SEQ] = core->seq++;
    f[REFEREE2026_OFF_CRC8] = (uint8_t)LIB_CRC_TableCalc(&referee2026_crc8_table, f, REFEREE2026_OFF_CRC8);
    Referee2026CorePutU16(&f[REFEREE2026_OFF_CMD_ID], info->cmd_id);
    memcpy(&f[REFEREE2026_OFF_DATA], data, len);

    const uint16_t frame_len = (uint16_t)(REFEREE2026_OFF_DATA + len + REFEREE2026_TAIL_SIZE);
    const uint16_t crc16 = (uint16_t)LIB_CRC_TableCalc(&referee2026_crc16_table, f, (uint32_t)(frame_len - REFEREE2026_TAIL_SIZE));

    Referee2026CorePutU16(&f[frame_len - REFEREE2026_TAIL_SIZE], crc16);

    /* ⑦ 先置"在途"再启动：反过来的话，DMA 刚起来就完成（回调清 `tx_busy`）之后我们才置 1，
     *    这一位就再没人清，发送永久卡死。 */
    core->tx_busy = 1u;

    const BSP_Status_e ret = USARTTransmit(core->usart, f, frame_len, BSP_DMA_MODE, 0u);

    if (ret != BSP_OK)
    {
        core->tx_busy = 0u;
        core->tx_dropped++;
        return ret;
    }

    core->tx_last_us[data_id] = now;
    core->tx_ok++;
    return BSP_OK;
}

/*============================================
 *   四、生命周期
 *============================================*/

/**
 * @brief 内核初始化：展开过滤掩码、派生 `rx_enabled` / `rx_periodic`、复位状态与计数
 * @note **静态**：`Referee2026Config` 的第一步，调用方不再直接调它（四条链路曾各抄一遍）。
 */
static void Referee2026CoreInit(Referee2026Core_t *core, uint32_t filter)
{
    if (core == NULL)
    {
        return;
    }

    /* 一次遍历出三个结果：① "全部可取 RX 的命令"掩码（供 FILTER_DEFAULT 展开）；
     * ② 本链路到底有没有收得到的命令（决定 Config 要不要起接收/挂看门狗）；
     * ③ 其中有没有**周期**命令（`freq_hz != 0`）—— 决定"多久没收到"这个离线判据成不成立。 */
    uint32_t mask_all_rx = 0;
    uint8_t rx_enabled = 0;
    uint8_t rx_periodic = 0;

    for (uint16_t i = 0; i < core->count; i++)
    {
        if ((core->info[i].dir & REFEREE2026_DIR_RX) == 0u)
        {
            continue;
        }

        mask_all_rx |= 1u << (uint32_t)i;
        rx_enabled = 1;

        if (core->info[i].freq_hz != 0u)
        {
            rx_periodic = 1;
        }
    }

    core->rx_enabled = rx_enabled;
    core->rx_periodic = rx_periodic;
    core->filter = (filter == REFEREE2026_FILTER_DEFAULT) ? mask_all_rx : filter;

    Referee2026CoreRxReset(core);
    core->seq = 0;
    core->tx_busy = 0;

    core->rx_valid = 0;
    core->rx_ok = 0;
    core->rx_filtered = 0;
    core->rx_unknown = 0;
    core->rx_crc_err = 0;
    core->rx_len_err = 0;
    core->tx_ok = 0;
    core->tx_dropped = 0;

    /* tick / tx_last_us 一并清零：0 在本模块兼作"从未收到 / 从未发过"的哨兵 */
    if (core->tick != NULL)
    {
        memset(core->tick, 0, (size_t)core->count * sizeof(core->tick[0]));
    }

    if (core->tx_last_us != NULL)
    {
        memset(core->tx_last_us, 0, (size_t)core->count * sizeof(core->tx_last_us[0]));
    }
}

BSP_Status_e Referee2026Register(Referee2026Core_t *core)
{
    if (core == NULL || core->usart == NULL)
    {
        return BSP_PARAM_ERR;
    }

    const BSP_Status_e ret = USARTRegister(core->usart);

    if (ret != BSP_OK)
    {
        return ret;
    }

    if (core->daemon != NULL)
    {
        DaemonRegister(core->daemon);
    }

    return BSP_OK;
}

BSP_Status_e Referee2026Config(Referee2026Core_t *core, const Referee2026Config_s *cfg)
{
    if (core == NULL || cfg == NULL || core->usart == NULL)
    {
        return BSP_PARAM_ERR;
    }

    core->robot_id = cfg->robot_id;

    /* 先把内核复位并展开过滤掩码 —— 下面 `rx_enabled` 的门要用它 */
    Referee2026CoreInit(core, cfg->filter);

    /* bsp 的回调一律从 `usart->parent` 取回内核，故 parent 必须指内核本身而不是链路实例 */
    const USART_Config_s usart_cfg = {
        .uart_e = cfg->uart_e,
        .parent = core,
        .rx_callback = Referee2026CoreRxHook,
        .tx_callback = Referee2026CoreTxDoneHook,
        .err_callback = Referee2026CoreErrHook,
    };

    BSP_Status_e ret = USARTConfig(core->usart, &usart_cfg);

    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 只发不收的链路（表里没有任何 `dir & RX`）：到这儿就结束了。
     * 不能顺手把 daemon 配上 —— 那样它会永久喂不上狗，而 DaemonTask 在离线期间**每拍**
     * 都重放故障动作（蜂鸣器会一直叫）；也不能起接收，收了也没人认。 */
    if (core->rx_enabled == 0u)
    {
        return BSP_OK;
    }

    if (core->daemon != NULL)
    {
        /* 本链路**一条周期 RX 命令都没有**（图传链路：两条 RX 全是事件触发）时，"多久没收到"
         * 不是故障判据 —— 对端正常但没在下发时链路上本来就一条帧都没有，照调用方配的故障动作
         * 执行只会在空档里误鸣蜂鸣器。故**强制**成 NONE。
         * 注意**只清故障动作、不注销 daemon**：它还挂着 RX 停摆自恢复的离线钩子，那是唯一能在
         * 接收真停摆后把 DMA 拉起来的地方；用 `rx_periodic == 0` 就整个不挂 daemon，保护会一起没了。
         * 调用方若真想要这种链路的离线指示，得先让它有周期性（协议层面），不是在这里绕过。 */
        const DaemonFaultAction_e fault = (core->rx_periodic != 0u) ? cfg->daemon_fault : (DaemonFaultAction_e)DAEMON_FAULT_NONE;

        /* 本驱动挂了离线钩子，故 reload 填 0（本义"禁用监控"）要提升成默认值，
         * 否则那层"接收停摆自恢复"会因少配一个字段而静默失效。 */
        const Daemon_Config_s daemon_cfg = {
            .reload_count = (cfg->daemon_reload == 0u) ? (uint16_t)REFEREE2026_DAEMON_RELOAD_MS : cfg->daemon_reload,
            .fault_action = fault,
            .callback = Referee2026CoreOfflineHook,
            .owner_id = core,
        };

        DaemonConfig(core->daemon, &daemon_cfg);
    }

    /* 起常开流（内核状态与回调都已就位） */
    return USARTReceive(core->usart, REFEREE2026_RX_BUFF_SIZE, BSP_DMA_MODE, 0u);
}

/*============================================
 *   五、回调适配（**静态**：由 Config 装进 bsp / daemon，外部不直接调）
 *============================================*/

static void Referee2026CoreRxHook(USARTInstance *usart)
{
    if (usart == NULL)
    {
        return;
    }

    Referee2026Core_t *const core = (Referee2026Core_t *)usart->parent;

    if (core == NULL)
    {
        return;
    }

    const uint32_t valid_before = core->rx_valid;

    Referee2026RxIsr(core, usart->rx_buff, usart->rx_len);

    /* 有帧就喂狗 —— 包括被过滤、不认识的帧：看门狗答的是"链路还活着吗"，不是"我要不要这条" */
    if (core->daemon != NULL && core->rx_valid != valid_before)
    {
        DaemonReload(core->daemon);
    }
}

static void Referee2026CoreTxDoneHook(USARTInstance *usart)
{
    if (usart == NULL)
    {
        return;
    }

    Referee2026CoreTxDone((Referee2026Core_t *)usart->parent);
}

static void Referee2026CoreErrHook(USARTInstance *usart, USART_ErrReason_e reason)
{
    if (usart == NULL)
    {
        return;
    }

    Referee2026Core_t *const core = (Referee2026Core_t *)usart->parent;

    if (core == NULL)
    {
        return;
    }

    switch (reason)
    {
    case USART_ERR_TX_ABORT: {
        /* 在途发送已被强制中止，bsp 契约保证 tx_callback 不会再来 ⇒ 这一位只能由我们清 */
        core->tx_busy = 0;
        return;
    }

    case USART_ERR_RX_STALLED: {
        /* 接收常开流已断，此后不再有 rx_callback。重启要经 USARTReceive →
         * HAL_UART_AbortReceive 按 HAL_GetTick 自旋，而本回调在 ISR 上下文，调用即死等。
         * 交给 daemon 的离线钩子（任务上下文）补：接收停了 → 没帧喂狗 → daemon 转离线 → 重启。 */
        return;
    }

    case USART_ERR_HW:
    default: {
        /* 硬件错误（PE/FE/NE/ORE/DMA…）。发送是否仍在途要看 gState，此处判不了：
         * 若 tx_callback 还会来，它自己会清；若不来了，Send 入口的 RecoverTxIfStuck 补一刀。 */
        return;
    }
    }
}

static void Referee2026CoreOfflineHook(void *owner)
{
    Referee2026Core_t *const core = (Referee2026Core_t *)owner;

    if (core == NULL || core->usart == NULL)
    {
        return;
    }

    /* 判据（`rx_armed == 0` 才算真停摆）、重启参数、限频基准都在 bsp 内 */
    (void)USARTRecoverRxIfStalled(core->usart, REFEREE2026_RX_RESTART_PERIOD_MS);
}

#endif /* DRV_REFEREE2026_USED */

#endif /* HAL_UART_MODULE_ENABLED */

/**
 * @file bsp_log.c
 * @brief 日志系统实现（日志层）
 *
 * @note 实现 BSPLOG 宏的核心 BSPLogV()：限频检查 + 组装
 *       "[颜色][分级][时间戳][模块名]:内容[重置]\r\n"。
 *
 * 缓冲池设计（不占调用方栈、生命周期长、DMA 友好）：
 *   - bsp_log.c 静态区定义 log_buf_s 缓冲池（LOG_BUF_NUM 个，每槽
 *     data + 状态 + 长度）。BSPLogV 借用空闲缓冲，用 LibFormatV 一次模板
 *     格式化（头部模板 LOG_HDR_FMT：颜色/分级/时间戳/模块名，固定串从 flash
 *     拷贝、数字格式化到其中）直接写进 data，随后 uart 空闲时直接 DMA 发送、
 *     忙时置 WAIT_SEND 排队等完成回调调度。
 *   - 状态机防可重入冲突：WRITE（借用后未提交）/SEND（已提交传输层）期间
 *     该缓冲不可再借，中断里再调 BSPLOG 会拿到别的空闲缓冲。
 *   - 传输层 DMA 完成后 LogUartTxCplt 归还缓冲并调度下一个，否则池耗尽。
 *   - 借用/归还/状态切换只做单字节状态写（uint8_t，MCU 上单字节写原子），
 *     无需关中断；竞态最坏只会让两条日志复用同一缓冲（内容覆盖），可接受。
 *   - **状态机不能卡死**（丢一条日志可以，永久静默不行）：SEND 槽与"在途传输"不一致
 *     的两种残局都有兜底 —— 被 HAL 停在 BUSY_TX 的由 USARTRecoverTxIfStuck 管，
 *     槽还占着而 UART 已空闲（那次传输死了）的由 LogReclaimOrphanSend 管
 *     （两者都在 BSPLogV 开头调用）。
 *
 * 时间戳依赖 DWT（include bsp_dwt.h）；DWT_Init 由启动流程负责，
 * BSPLogInit 不再重复初始化。
 */

#include "bsp_log.h"

#if (defined(BSP_LOG_USED)) && (defined(LOG_UART))

#include "bsp_usart.h"
#include "bsp_dwt.h"
#include <stdarg.h>
#include <string.h>

// 结构体，枚举
typedef enum : uint8_t
{
    LOG_BUF_FREE = 0,  /* 空闲：可借出 */
    LOG_BUF_WRITE,     /* 写入：BSPLogV 正在格式化，未提交 */
    LOG_BUF_SEND,      /* 发送：已提交传输层，DMA 排队/发送中 */
    LOG_BUF_WAIT_SEND, /* 等待发送：发送完成回调会扫描s_buf_state，如果有待发送就发送 */
} LOG_BUF_STATE;

typedef struct
{
    char buf_pool[LOG_LEN_MAX];       /* 生命周期长，DMA 直接读取 */
    volatile LOG_BUF_STATE buf_state; /* LOG_BUF_STATE */
    uint16_t buf_len;                 /* 每缓冲有效长度：WAIT_SEND 出队发送时需要 */
} log_buf_s;

// 静态变量
/*============ 内部查表 ============*/
/* 裸标签（[D]/[I]/[W]/[E]，宏里用 %s 包裹进 []） */
static const char *s_level_str[] = {
    "D",
    "I",
    "W",
    "E",
};
static const char *s_level_color[] = {
    BSP_LOG_COLOR_BLUE,
    BSP_LOG_COLOR_GREEN,
    BSP_LOG_COLOR_YELLOW,
    BSP_LOG_COLOR_RED,
};

/* 各级别累计日志条数（全局，跨所有实例；借用成功即计入，含排队；限频/池满丢弃不计） */
uint64_t level_cnt[LOG_LEVEL_NUM];
/* DMA 直接读取缓冲，须放 DMA 可访问 RAM（H7 上 .ram_d1，见 bsp_map.h 的 DMA_RAM） */
static log_buf_s s_log_buf[LOG_BUF_NUM] DMA_RAM = {0};
USART_INSTANCE_DEF(s_log_uart, 1);          /* 日志串口实例（TX DMA 完成中断回调 = LogUartTxCplt） */
static volatile log_buf_s *s_tx_buf = NULL; /* 当前 DMA 发送中的缓冲（完成回调里归还并调度下一个） */

/* >0 = 有**任务上下文**的 BSPLogV 正处在"置 SEND → 启动 DMA → 认领 s_tx_buf"的窗口内。
 * 只有这个窗口允许出现"有 SEND 槽、但 s_tx_buf 仍为 NULL"的中间态，而此刻 UART 还是
 * READY（DMA 尚未启动），所以 LogReclaimOrphanSend 单看 gState 分不出"正在启动"与
 * "真残留"，必须靠这个计数排除掉；否则会把一笔正在启动的发送收走（边发边被改写）。
 * ISR 侧不需要它：任务不可能抢占 ISR。
 *
 * 用计数而不是 0/1 标志或指针：本模块的多个任务都会记日志，两个任务可以**同时**处在
 * 各自窗口内（A 被 B 抢占在窗口里），一个槽的标记会被后者整个覆盖 —— 前者的槽就暴露在
 * 回收判据下了。计数只增只减、谁都不覆盖谁。 */
static volatile uint8_t s_log_starting_cnt = 0;

/** @brief 进入/退出"正在启动"窗口（自增自减各一次，成对使用）
 * @note 必须夹在临界区里：Cortex-M 上 `cnt++` 是可抢占的读-改-写，两个任务互抢会丢一次
 *       更新（计数少一个正好等于把标记抹掉，即上面要防的那件事）。只关中断、不阻塞，
 *       调用点本来就在发送路径上。 */
static inline void LogStartingEnter(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    s_log_starting_cnt++;
    if (primask == 0U)
    {
        __enable_irq();
    }
}

static inline void LogStartingExit(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (s_log_starting_cnt > 0U)
    {
        s_log_starting_cnt--;
    }
    if (primask == 0U)
    {
        __enable_irq();
    }
}

/* 回收过的残留槽累计数（见 LogReclaimOrphanSend）。
 * 恒为 0 = 那条泄漏路径从未发生；非 0 = 发生过、值是被救回来的槽数。
 * 排障用：日志链突然"变哑"时先看它，非 0 就说明命中的是下面注释里那条窄路径。 */
uint32_t g_log_orphan_cnt = 0;

/* UART DMA 发送完成回调（中断上下文）：
 * 归还刚发完的缓冲，并扫描 WAIT_SEND 提交下一个，串起发送链 */
static void LogUartTxCplt(USARTInstance *instance)
{
    uint8_t i;

    (void)instance;

    if (s_tx_buf != NULL)
    {
        s_tx_buf->buf_state = LOG_BUF_FREE; /* SEND → FREE：归还 */
        s_tx_buf = NULL;
    }
    /* 内联原 BSPLogTakeWaitSend：扫描 WAIT_SEND 置 SEND 出队，交给 DMA 发送 */
    for (i = 0; i < LOG_BUF_NUM; i++)
    {
        if (s_log_buf[i].buf_state == LOG_BUF_WAIT_SEND)
        {
            /* 与 BSPLogV 同款：先置 SEND、启动成功才认领（s_tx_buf 此刻必为 NULL） */
            s_log_buf[i].buf_state = LOG_BUF_SEND;
            if (USARTTransmit(&s_log_uart, (const uint8_t *)s_log_buf[i].buf_pool, s_log_buf[i].buf_len,
                              BSP_DMA_MODE, 0) != BSP_OK)
            {
                /* 启动失败：立即归还本槽（后续 WAIT_SEND 由下一次发送完成回调接续） */
                s_log_buf[i].buf_state = LOG_BUF_FREE;
            }
            else
            {
                s_tx_buf = &s_log_buf[i];
            }
            break;
        }
    }
}

/* 是否已有缓冲在发送中（SEND）：同一时刻至多一个，用作"能否直接发送"的就绪判据 */
static uint8_t LogBufSending(void)
{
    uint8_t i;

    for (i = 0; i < LOG_BUF_NUM; i++)
    {
        if (s_log_buf[i].buf_state == LOG_BUF_SEND)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief 回收"残留槽"：UART 手里已没有任何在途传输，链上却还有槽没被归还
 * @return 1 = 本次回收了（至少一个槽），0 = 没动作
 *
 * @note 这是个**兜底**，正常路径永远不该命中。先记清楚它是怎么产生的：
 *       BSPLogV 的直发路径是"先置 SEND → 启动 DMA → 成功后才认领 s_tx_buf = lb"。
 *       中间那几拍若来了一次会把这笔发送**收尾**的中断（最现实的是 TX DMA 传输错误 TE——
 *       DMA 流配置坏掉时 TE 就在使能瞬间触发），此刻 s_tx_buf 仍是 NULL，
 *       LogUartErrHandler 按判据直接返回（它不能把"在途但未认领"的槽当成被中止的那次来归还），
 *       而 HAL_UART_Transmit_DMA 早已返回 OK —— 外层于是照旧把这笔认领下来。
 *       净结果：**槽停在 SEND、s_tx_buf 指着它、gState 已被 HAL 复位成 READY、
 *       而且再也不会有人来收尾**（那次传输已经死了，不会有 TxCplt）。
 *       这个残局三条既有自救路全都够不到：
 *         - LogUartTxCplt：只认完成回调，而它永远不会来；
 *         - LogUartErrHandler：错误已经报过一次（当时 s_tx_buf 为 NULL 直接返回），不会再报；
 *         - USARTRecoverTxIfStuck：见 gState == READY 就刷新基准早退，判不出卡死
 *           （它治的是"gState 停在 BUSY_TX"那种，正好是本残局的反面）。
 *       于是 LogBufSending() 恒真 → 之后所有日志转 WAIT_SEND → 池满即丢，
 *       整条日志链永久静默，且没有任何既有计数器会涨 —— 这就是 g_log_orphan_cnt 的由来。
 *
 * @note 判据（三条同时成立 = UART 手里确实没有在途传输，链上不该还有未归还的槽）：
 *       ① gState == HAL_UART_STATE_READY：HAL 在**启动 DMA 之前**就置 BUSY_TX，所以
 *          "正在启动的那笔"绝不可能是 READY；而 READY 只在处理完成/错误的**那个 ISR 内部**
 *          出现（HAL 先改 gState、紧接着几个指令内就调回调），任务上下文一旦看到 READY，
 *          那次收尾就已经执行完了 —— 不存在"READY 但 TxCplt 还挂着"的时刻。
 *          于是"认领者还在，UART 却已空闲"只有一个解释：那次传输死了（见上）。
 *       ② 没有别的 BSPLogV 处在启动窗口内（s_log_starting_cnt）。这一条不能省：
 *          那个窗口里 gState 也恰好是 READY，光看 gState 会把正在启动的槽当成残留收走，
 *          而调用方随后照样启动 DMA —— 于是 DMA 去读一个可能已被别人借走改写的缓冲。
 *       ③ 任务上下文（ISR 里 gState 未必已刷新；临界区里的状态由调用者退出后自行收敛）。
 *       ①+②成立后，被认领的槽与未认领的 SEND 槽都是死的，一并归还。
 */
static uint8_t LogReclaimOrphanSend(USARTInstance *instance)
{
    uint8_t i, reclaimed = 0;

    if (instance == NULL || instance->handle == NULL ||
        instance->handle->gState != HAL_UART_STATE_READY)
    {
        return 0; /* 非空闲：可能有在途传输，别动 */
    }
    if (s_log_starting_cnt > 0U)
    {
        return 0; /* 有任务正处在"置 SEND → 认领"的窗口内，那是它的槽 */
    }
    /* ③ 上下文判据与 bsp 各模块的 *CanBlockingAbort 同式：ISR / 临界区里一律不动手 */
    if ((__get_IPSR() != 0U) || (__get_PRIMASK() != 0U) || (__get_BASEPRI() != 0U))
    {
        return 0;
    }

    /* 已认领的槽（上面那类残局）：UART 空闲却还挂着认领 —— 那次传输已经死了。
     * 这里放掉它不会误伤"迟到的 TxCplt"：那样的回调只可能是上面的 ISR 路径，
     * 而它不可能与任务上下文里的 READY 并存（见判据①）。 */
    if (s_tx_buf != NULL)
    {
        s_tx_buf->buf_state = LOG_BUF_FREE; /* 归还 */
        s_tx_buf = NULL;
        g_log_orphan_cnt++;
        reclaimed = 1;
    }

    /* 未认领的 SEND 槽：现有两条写 SEND 的路径（BSPLogV / LogUartTxCplt）失败时都会
     * 立刻回写 FREE 或 WAIT_SEND，所以正常到不了这里；留着当不变量兜底 ——
     * 真要命中，说明又多了一条写 SEND 的路，正好在这里现形。 */
    for (i = 0; i < LOG_BUF_NUM; i++)
    {
        if (s_log_buf[i].buf_state == LOG_BUF_SEND)
        {
            s_log_buf[i].buf_state = LOG_BUF_FREE; /* 归还 */
            g_log_orphan_cnt++;
            reclaimed = 1;
        }
    }
    return reclaimed;
}

/* UART 错误回调（ISR 或**任务上下文**；后者来自 bsp 的发送卡死自恢复 USART_RecoverTx）：
 * 若本次发送已被中止（HAL/Abort 已把状态置回 READY），就不会再有完成回调，
 * 须归还在途缓冲，否则它永久停在 SEND、反复出错会耗光缓冲池。
 * 反过来若发送仍在途（gState 非 READY），说明错在接收侧，不能动这个缓冲——
 * 那种情况下的残留由下一次发送完成回调兜底归还。
 *
 * 幂等：硬件错误可能连续触发，bsp 的卡死恢复也可能与它叠加，重复调用须无副作用。
 *
 * 这里刻意**不**补发 WAIT_SEND：本回调可能是在 USARTTransmit 的调用栈内被触发的
 * （卡死恢复路径），补发等于嵌套调用 USARTTransmit；而外层失败后还会写 s_tx_buf /
 * buf_state，会把内层刚启动的那个槽覆盖掉——那个槽就永久停在 SEND，堵死整条日志链。
 * 积压的 WAIT_SEND 由下一次发送完成回调（TxCplt）带出即可，不必在这里抢；
 * 期间若再没有任何一条日志发出去，这些槽就一直挂在 WAIT_SEND，直到下一条日志触发发送。 */
static void LogUartErrHandler(USARTInstance *instance, USART_ErrReason_e reason)
{
    /* RX_STALLED 与本模块无关（日志实例只发不收，没启动接收） */
    if (reason == USART_ERR_RX_STALLED)
        return;

    if (s_tx_buf == NULL)
        return;

    /* TX_ABORT：bsp 刚强止了本次发送并把 gState 复位为 READY，rx 已确知不会再来完成回调；
     * HW：HAL 的错误位都在接收侧，gState 非 READY 恰恰说明"有一次健康的发送仍在途"，
     *     那种情况不能动这个缓冲（残留由下一次 TxCplt 兜底归还）*/
    if (reason == USART_ERR_TX_ABORT ||
        (instance != NULL && instance->handle != NULL &&
         instance->handle->gState == HAL_UART_STATE_READY))
    {
        s_tx_buf->buf_state = LOG_BUF_FREE; /* 归还，后续由新的日志重新调度 */
        s_tx_buf = NULL;
    }
}

/*============ 内部工具（static） ============*/

/**
 * @brief 限频检查：通过返回 1，超频/禁用返回 0（本条丢弃）
 * @note 1 秒窗口：窗口内计数达到 times_per_second 后丢弃，距窗口起点满
 *       1 秒重置计数并记录新窗口起点。
 * @note times_per_second == 0 表示禁用该实例：所有日志丢弃（编译期定值，
 *       配合 LOG_INSTANCE_DEF 传 0 即可关掉整模块输出）。
 */
static int BSPLogCheckLimit(LOGInstance *inst)
{
    uint64_t now_us;

    if (inst->times_per_second == 0)
    {
        return 0; /* 禁用该实例：本条丢弃 */
    }

    now_us = DWT_GetTimeUs();

    /* 本 1 秒窗口内计数已达上限 */
    if (inst->log_cnt >= inst->times_per_second)
    {
        /* 距窗口起点不足 1 秒：丢弃本条 */
        if (now_us - inst->last_timestamp_us < 1000000u)
        {
            return 0;
        }
        /* 窗口已结束：重置计数并记录新窗口起点 */
        inst->log_cnt = 0;
        inst->last_timestamp_us = now_us;
    }

    inst->log_cnt++;
    return 1;
}

static const char *BSPLogLevelStr(LOG_LEVEL level)
{
    if ((unsigned int)level > (unsigned int)LOG_LEVEL_ERROR)
    {
        level = LOG_LEVEL_ERROR;
    }
    return s_level_str[level];
}

static const char *BSPLogLevelColor(LOG_LEVEL level)
{
    if ((unsigned int)level > (unsigned int)LOG_LEVEL_ERROR)
    {
        level = LOG_LEVEL_ERROR;
    }
    return s_level_color[level];
}

/* 追加以 '\0' 结尾的字符串段（防越界，最多写到 end-1） */
static char *BSPLogAppend(char *dst, const char *end, const char *s)
{
    if (s == NULL)
    {
        return dst;
    }
    while (*s != '\0' && dst + 1 < end)
    {
        *dst++ = *s++;
    }
    return dst;
}

/* 日志头部模板：颜色+分级+[时间戳]+模块名+冒号，一次 LibFormatEx 格式化。
 * 时间戳风格由 TIME_STAMP_STYLE 编译期决定（0=无 / 1=us / 2=ms）。
 * LOG_HDR_ARGS 同步调整变参个数（STYLE==0 只传 3 个，不含 st）——
 * va_list 顺序消费，多传的参数会错位，必须与模板转换个数严格一致。 */
#if TIME_STAMP_STYLE == 0
#define LOG_HDR_FMT "%s[%s][%s]:"
#define LOG_HDR_ARGS(color, lvl, st, module) (color), (lvl), (module)
#else
#define LOG_HDR_FMT "%s[%s][%llu][%s]:"
#define LOG_HDR_ARGS(color, lvl, st, module) (color), (lvl), (st), (module)
#endif

/*============ 外部接口（2 个） ============*/

/* 默认日志实例：开箱即用，模块名按无后缀文件名 "bsp_log"、
 * 默认限频 10 条/秒，可编译期用 BSP_LOG_LOG_LIMIT(-D) 覆盖（extern 声明见 bsp_log.h） */
#ifndef BSP_LOG_LOG_LIMIT
#define BSP_LOG_LOG_LIMIT 10
#endif // !BSP_LOG_LOG_LIMIT
LOG_INSTANCE_DEF(g_log, "bsp_log", BSP_LOG_LOG_LIMIT);

void BSPLogInit(void)
{
    USART_Config_s cfg = {
        .uart_e = LOG_UART,
        .parent = NULL,
        .rx_callback = NULL,
        .tx_callback = LogUartTxCplt,
        .err_callback = LogUartErrHandler,
    };

    if (USARTRegister(&s_log_uart) != BSP_OK)
    {
        return;
    }
    /* 只发不收：不需要 USARTReceive，也不会再被要求配 RX DMA */
    if (USARTConfig(&s_log_uart, &cfg) != BSP_OK)
    {
        return;
    }
    BSPLOG(&g_log, LOG_LEVEL_INFO, "你好！"); // 测试中文（需要指定文件编码和串口接收都是UTF-8）
}

void BSPLogV(LOGInstance *inst, LOG_LEVEL level, const char *fmt, ...)
{
    log_buf_s *lb;
    char *buf;
    char *end;
    char *p;
    va_list args;
    int n;
    uint8_t i;
    BSP_Status_e ret;

    /* 日志传输层尚未就绪（BSPLogInit 未跑，或该串口 Config 失败）：无处可发，直接丢弃。
     * 必须在这里拦住，不能让下面的 USARTTransmit 去挡——它在实例未配置时会打一条错误日志，
     * 而那条日志走的是同一个串口，会以 WAIT_SEND 滞留到下一次成功发送时才冒出来
     * （时间戳还是旧的），看起来像"启动顺序反了"。
     * 典型场景：BSPLogInit 里 USARTRegister 成功、USARTConfig 还没跑的瞬间自己打的日志。 */
    if (s_log_uart.handle == NULL)
    {
        return;
    }

    /* 发送链自检，放在最前（早于限频与借槽）：本模块的发送链一旦卡住就再也调用不到
     * USARTTransmit —— 在途槽永远停在 SEND、新日志全部转 WAIT_SEND、池满后连排都不排，
     * 整条链静默。只有每帧必经的这里能收拾。两条互补的判据、缺一不可：
     *   ① USARTRecoverTxIfStuck：治"gState 被 HAL 停在 BUSY_TX"的卡死，
     *      它复位后按契约回调 LogUartErrHandler 归还那个 SEND 槽。
     *   ② LogReclaimOrphanSend：治①够不到的另一种残留 —— 发起方以为在途、UART 却已空闲
     *      （gState == READY），那次传输其实死了、只剩槽被占着，详见该函数注释。
     * ISR 里调用都是安全的：两者都自带上下文/状态守卫，不满足就什么都不做。
     * 空闲时各自只做一次状态读，开销可忽略。 */
    (void)USARTRecoverTxIfStuck(&s_log_uart, 0);
    (void)LogReclaimOrphanSend(&s_log_uart);

    if (!BSPLogCheckLimit(inst))
    {
        return; /* 模块超频：丢弃本条 */
    }

    /* 内联原 BSPLogTakeBuf：扫描 FREE 槽，写入前即置 WRITE（单字节写原子，无需关中断） */
    lb = NULL;
    for (i = 0; i < LOG_BUF_NUM; i++)
    {
        if (s_log_buf[i].buf_state == LOG_BUF_FREE)
        {
            s_log_buf[i].buf_state = LOG_BUF_WRITE;
            lb = &s_log_buf[i];
            break;
        }
    }
    if (lb == NULL)
    {
        return; /* 池满：丢弃本条 */
    }

    /* 各级别累计计数（借用成功即计入；限频/池满丢弃不计；非法级别钳制到 ERROR）：
     * 本实例计数 + 全局统计都 +1 */
    if ((unsigned int)level > (unsigned int)LOG_LEVEL_ERROR)
    {
        level = LOG_LEVEL_ERROR;
    }
    inst->level_cnt[level]++;
    level_cnt[level]++;

    buf = lb->buf_pool;

    /* 直接格式化进静态缓冲（不占调用方栈）：头部固定段一次模板格式化，
     * 颜色/分级/时间戳/模块名全走 %s/%u，不再逐段 Append */
    end = buf + LOG_LEN_MAX;
    p = buf;
    {
        uint64_t st = 0;
#if TIME_STAMP_STYLE != 0
        st = DWT_GetTimeUs(); /* 完整 64 位 us，不截断（2^64 µs ≈ 58 万年才回绕） */
        if (TIME_STAMP_STYLE == 2)
        {
            st = st / 1000u;
        }
#endif
        (void)st; /* STYLE==0 时不参与格式化 */
        int hn = LibFormatEx(p, (size_t)(end - p), LOG_HDR_FMT, sizeof(LOG_HDR_FMT) - 1,
                             LOG_HDR_ARGS(BSPLogLevelColor(level), BSPLogLevelStr(level), st, inst->module_name));
        /* 与正文同款钳制：LibFormatEx 的契约是"返回本应写入的字符数"（lib_format.h），
         * 截断时它大于缓冲余量，直接 p += 会让 p 越过 end —— 后面的 (size_t)(end - p)
         * 就从"余量"变成巨大的无符号数（下溢），把钳制全部架空。
         * 头部本身够长时（模块名超长，或 LOG_LEN_MAX 被调小）就会踩到。 */
        p += (hn < (int)(end - p)) ? hn : ((int)(end - p) - 1);
    }

    va_start(args, fmt);
    n = LibFormatV(p, (size_t)(end - p), fmt, strlen(fmt), args);
    va_end(args);
    p += (n < (int)(end - p)) ? n : ((int)(end - p) - 1); /* 截断时钳到 end-1 */

    p = BSPLogAppend(p, end, BSP_LOG_COLOR_RESET "\r\n");

    {
        uint32_t len = (uint32_t)(p - buf);
        if (len >= (uint32_t)LOG_LEN_MAX)
        {
            len = (uint32_t)LOG_LEN_MAX - 1; /* 截断兜底，防越界读 */
        }
        lb->buf_len = (uint16_t)len; /* 记录长度，供 WAIT_SEND 出队 */

        /* 提交：WRITE → SEND 允许 DMA 读取；uart 空闲直发，忙则排队（内联原 BSPLogMediaSend）。
         * 就绪判据用本模块自己的"SEND 槽是否存在"而不是去问 uart 状态：同一时刻至多一个
         * SEND，据此判断即无 IsReady→Transmit 之间被打断的窗口（该窗口会让两个缓冲同时
         * 交给 DMA）。 */
        uint8_t direct = (uint8_t)(LogBufSending() == 0);

        if (!direct)
        {
            /* 有帧在途：排队等完成回调扫描发送 */
            lb->buf_state = LOG_BUF_WAIT_SEND;

            /* 上面那次判断与这次写状态之间有一段窗口：完成回调可能恰好在这时归还了那个
             * SEND 槽，并且扫描 WAIT_SEND 时还没看到本槽 —— 那就没有任何在途传输了，
             * 本帧会白等到下一条日志的完成回调才被带出去。这里再看一眼，确实空了就直发。 */
            direct = (uint8_t)(LogBufSending() == 0);
        }

        if (direct)
        {
            /* 先置 SEND、再启动、**成功后才认领**（s_tx_buf = lb）：
             *   - 置 SEND 在前：中断里再进来的 BSPLOG 看到"有槽在发"即排队，不会与本帧
             *     抢着启动 DMA；
             *   - 认领在后：本帧尚未交给 DMA 时 s_tx_buf 仍为 NULL，这期间到来的
             *     err_callback（发送卡死自恢复可能就在 USARTTransmit 调用栈内触发）
             *     会直接返回，不会把本帧当成"被中止的那次"释放掉。若提前认领，一个挂起
             *     已久、此刻才被响应的错误中断会把本槽置 FREE 交给别的日志，而外层照旧
             *     把 DMA 启动起来了（边发边被改写）；
             *   - 于是失败分支只需回写 lb->buf_state：s_tx_buf 从未指向它，也没人动过它
             *     （SEND 态的槽不可被借用）。
             * 代价是这一段里"有 SEND 槽、但 s_tx_buf 为 NULL"会短暂成立（上面第 2 条正是
             * 它想要的），而此刻 UART 也还是 READY（DMA 还没启动）—— 残留槽回收的两个
             * 判据同时为真，因此**必须先挂上 s_log_starting_cnt 再置 SEND**：
             * 反过来的话，抢占夹在这两条语句之间时，抢进来的那次回收会把这笔尚未启动的
             * 槽当残留收走（随即被别的日志借走改写），而本函数返回后照旧启动 DMA。
             * 同理，若这段时间里真来了一次把发送收尾的中断，槽会停在 SEND 而无人认领
             * —— 或者外层把 s_tx_buf 认领下来 —— 那就是残留态，留给下一次 BSPLogV 开头回收。 */
            LogStartingEnter();
            lb->buf_state = LOG_BUF_SEND;
            ret = USARTTransmit(&s_log_uart, (const uint8_t *)buf, (uint16_t)len, BSP_DMA_MODE, 0);
            if (ret != BSP_OK)
            {
                /* 忙：排队等完成回调；其它失败（参数/硬件）：丢弃本条、立即归还槽 */
                lb->buf_state = (ret == BSP_BUSY) ? LOG_BUF_WAIT_SEND : LOG_BUF_FREE;
            }
            else
            {
                s_tx_buf = lb; /* 传输层持有直到 DMA 完成，完成回调归还 */
            }
            LogStartingExit();
        }
    }
}

#endif // (defined(BSP_LOG_USED))||(defined(LOG_UART))

/**
 * @file drv_terminal_lite.c
 * @brief 串口调参极简终端实现（只收发消息）
 *
 * @note 数据流（无线程模型）：
 *   - bsp_usart RX 中断里只把整段字节拷进环形缓冲（不含任何业务）；
 *   - app 在自己的任务里周期调 TerminalLiteExecute()：从环形缓冲逐字节组行(\r/\n)
 *     → 解析 tlget/tlset → 匹配 token → 校验 → 就地 exec → 回显；
 *   - 手动调参的发送间隔一般 5~10s，轮询处理即可，故本模块不创建任务/队列，
 *     也不依赖任何 FreeRTOS API；
 *   - TX 只有一个写者（都在 TerminalLiteExecute 调用链里），内部再排一个小队列抗连发。
 *
 * @note 启用门控（仿 drv_vofa）：需在 app_cfg.h 同时定义 DRV_TERMINAL_LITE_USED 与
 *       TERMINAL_LITE_UART，否则本文件全部为空实现。
 */

#include "drv_terminal_lite.h"

#if (defined(DRV_TERMINAL_LITE_USED)) && (defined(TERMINAL_LITE_UART))

#include "bsp_usart.h"
#include "bsp_log.h"
#include "lib_format.h"
#include "main.h" // __get_PRIMASK / __disable_irq / __enable_irq
#include <stdarg.h>
#include <string.h>

#if (TERMINAL_LITE_RING_SIZE & (TERMINAL_LITE_RING_SIZE - 1)) != 0
#error "TERMINAL_LITE_RING_SIZE must be a power of two!"
#endif

/*------------- 内部错误码 --------------*/

typedef enum
{
    TL_ERR_CMD = 1,   // 未知命令 / 缺 token / 缺 exec
    TL_ERR_TOKEN = 2, // 未知 token
    TL_ERR_TYPE = 3,  // 该 token 不允许该操作
    TL_ERR_VALUE = 4, // 数值解析失败 / 缺参
} TerminalLiteErr_e;

/*------------- 环形缓冲（ISR 写 / Execute 读） --------------*/

static volatile uint8_t s_ring[TERMINAL_LITE_RING_SIZE];
static volatile uint16_t s_ring_head = 0; // 写指针（ISR）
static volatile uint16_t s_ring_tail = 0; // 读指针（Execute）
static volatile uint16_t s_ring_overrun = 0;
#define RING_NEXT(i) (((i) + 1) & (TERMINAL_LITE_RING_SIZE - 1))

/*------------- 命令表（Init 保存指针，表存 flash） --------------*/

static const TerminalLiteCmd_s *s_table = NULL;
static uint8_t s_cnt = 0;

/*------------- 实例定义（日志 / USART） --------------*/

LOG_INSTANCE_DEF(g_terminal_lite_log, "drv_terminal_lite", TERMINAL_LITE_LOG_LIMIT);

USART_INSTANCE_DEF(s_tl_uart, TERMINAL_LITE_MAX_LINE);

/*------------- 发送缓冲池（仿 bsp_log 槽池状态机；单写者=execute 所在任务） --------------
 * 每槽自带 data+state+len（替代原"数据/长度分数组 + 头尾游标"的写法）；
 * 状态机：FREE 借出 → WRITE 格式化 → SEND 直发（忙则 WAIT_SEND 排队），
 * DMA 完成回调把发完的 SEND 槽归还 FREE 并补发下一个 WAIT_SEND，串起发送链。 */

typedef enum : uint8_t
{
    TL_TX_FREE = 0,  // 空闲：可借出
    TL_TX_WRITE,     // 写入：TerminalLiteSend 正在格式化，未提交
    TL_TX_SEND,      // 发送：已提交传输层，DMA 发送中
    TL_TX_WAIT_SEND, // 等待发送：DMA 完成回调扫描补发
} TerminalLiteTxState_e;

typedef struct
{
    char data[TERMINAL_LITE_TX_BUF];      // DMA 直接读取，生命周期长
    volatile TerminalLiteTxState_e state; // 槽状态
    uint8_t len;                          // 本条有效长度（WAIT_SEND 补发时要用）
} TerminalLiteTxBuf_s;

static TerminalLiteTxBuf_s s_tx_pool[TERMINAL_LITE_TX_QUEUE_LEN] DMA_RAM;
static volatile TerminalLiteTxBuf_s *s_tx_now = NULL; // 当前 DMA 发送中的槽（完成回调归还并补发下一个）

/*------------- 行缓冲（Execute 私有） --------------*/

static char s_line[TERMINAL_LITE_MAX_LINE];
static uint16_t s_line_len = 0;
static uint8_t s_line_over = 0; // 当前行已超长，丢弃直到换行

/*============================================
 *              小工具
 *============================================*/

static uint8_t streq(const char *a, const char *b)
{
    if (!a || !b)
        return 0;
    size_t la = strlen(a);
    return la == strlen(b) && memcmp(a, b, la) == 0;
}

/**
 * @brief 极简字符串转浮点：支持 符号/整数/小数/指数(e/E)
 * @retval 0 成功
 */
static int parse_float(const char *s, float *out)
{
    if (!s || !out)
        return -1;
    const char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    int neg = 0;
    if (*p == '-')
    {
        neg = 1;
        p++;
    }
    else if (*p == '+')
        p++;

    float m = 0.0f;
    int frac = 0, exp = 0, digits = 0;
    while (*p >= '0' && *p <= '9')
    {
        if (digits < 15)
            m = m * 10.0f + (float)(*p - '0');
        digits++;
        p++;
    }
    if (*p == '.')
    {
        p++;
        while (*p >= '0' && *p <= '9')
        {
            if (digits < 15)
                m = m * 10.0f + (float)(*p - '0');
            digits++;
            frac++;
            p++;
        }
    }
    if (digits == 0)
        return -1;
    if (*p == 'e' || *p == 'E')
    {
        p++;
        int eneg = 0;
        if (*p == '-')
        {
            eneg = 1;
            p++;
        }
        else if (*p == '+')
            p++;
        int e = 0;
        if (!(*p >= '0' && *p <= '9'))
            return -1;
        while (*p >= '0' && *p <= '9')
        {
            if (e < 100000)
                e = e * 10 + (*p - '0');
            p++;
        }
        exp = eneg ? -e : e;
    }
    else if (*p != '\0')
        return -1;

    if (neg)
        m = -m;
    exp -= frac;
    while (exp > 0)
    {
        m *= 10.0f;
        exp--;
    }
    while (exp < 0)
    {
        m *= 0.1f;
        exp++;
    }
    *out = m;
    return 0;
}

/*============================================
 *              临界区（不依赖 FreeRTOS）
 *============================================*/

/* 只用于与 DMA 完成/错误回调（ISR）互斥：保存 PRIMASK 以支持嵌套，
 * 退出时只在原本开着中断的情况下恢复中断。 */
static inline uint32_t tl_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static inline void tl_exit_critical(uint32_t primask)
{
    if (primask == 0U)
        __enable_irq();
}

/*============================================
 *              RX：ISR 收字节
 *============================================*/

// bsp_usart RX 回调（ISR 上下文）：只拷字节，不解析；由 Execute 轮询取走
static void TerminalLiteRxHook(USARTInstance *instance)
{
    for (uint16_t i = 0; i < instance->rx_len; i++)
    {

        uint16_t h = s_ring_head;
        if (RING_NEXT(h) == s_ring_tail)
        {
            s_ring_overrun++;
            return;
        }
        s_ring[h] = instance->rx_buff[i];
        s_ring_head = RING_NEXT(h);
    }
}

/**
 * @brief 接收停摆自恢复（任务上下文，由 TerminalLiteExecute 触发）
 * @note 接收停摆（bsp 续收最终失败、rx_armed 被清）后 rx_callback 不再触发，
 *       只能靠 Execute 的周期性调用来自检。判据（`rx_armed == 0` 才是真停摆，
 *       用户只是没敲命令不算）、限频与重启参数都在 bsp 的 USARTRecoverRxIfStalled 里，
 *       四条 UART 链路共用同一份实现；这里只给限频周期。
 * @note bsp 禁止在 ISR 里重启接收（Abort 要自旋等 HAL tick），本函数在任务上下文，正合适。
 */
static void TerminalLiteRxRecover(void)
{
    /* 接收在跑 / 未到限频周期 → BSP_BUSY，空转；只有真重启成功才打日志 */
    if (USARTRecoverRxIfStalled(&s_tl_uart, TERMINAL_LITE_RX_RESTART_PERIOD_MS) == BSP_OK)
    {
        BSPLOG(&g_terminal_lite_log, LOG_LEVEL_WARNING, "RX stalled, receive restarted");
    }
}

/*============================================
 *              TX：发送队列 + DMA
 *============================================*/

static const char *err_text(TerminalLiteErr_e err)
{
    switch (err)
    {
    case TL_ERR_CMD:
        return "err: bad cmd\r\n";
    case TL_ERR_TOKEN:
        return "err: unknown token\r\n";
    case TL_ERR_TYPE:
        return "err: not allowed\r\n";
    case TL_ERR_VALUE:
        return "err: bad value\r\n";
    default:
        return "err\r\n";
    }
}

// 前置：调用方已在临界区（task）或处于 ISR（TX 完成回调）
// 启动 DMA 发送一个槽，成功才置 SEND/认领（调用方须已临界 / 处于 ISR，s_tx_now 不被并发改）
static void tx_start_slot(TerminalLiteTxBuf_s *tb)
{
    /* 顺序刻意是"先启动、成功才认领"：置 SEND/s_tx_now 在前的话，一个早先挂起、此刻才被
     * 响应的 UART 错误中断会在启动窗口里看到"本槽在途"，把尚未交给 DMA 的它当成被中止的
     * 那次释放掉，而外层照旧把 DMA 启动了 —— 槽随即可能被别的发送借走、边发边被改写。
     * 反过来没有窗口：DMA 完成中断至少要等一帧的传输时间，抢不到这两行之前。 */
    if (USARTTransmit(&s_tl_uart, (const uint8_t *)tb->data, tb->len, BSP_DMA_MODE, 0) != BSP_OK)
    {
        // DMA 启动失败：归还本槽；后续若有 WAIT_SEND 由下次完成回调补发
        tb->state = TL_TX_FREE;
        BSPLOG(&g_terminal_lite_log, LOG_LEVEL_WARNING, "DMA send start failed, dropped!");
        return;
    }

    tb->state = TL_TX_SEND;
    s_tx_now = tb;
}

/* UART 错误回调（ISR 或**任务上下文**；后者来自 bsp 的发送卡死自恢复 USART_RecoverTx）：
 * 若本次发送已被中止（HAL/Abort 已把状态置回 READY），不会再有待归还的完成回调，
 * 须释放在途槽，否则它永久停在 SEND、反复出错会耗光发送槽池（终端静默）。
 * 若发送仍在途（gState 非 READY），错在接收侧，此刻不能释放 DMA 还在读的缓冲——
 * 那种残留由下一次发送完成回调兜底归还。
 *
 * 幂等：重复调用须无副作用（同一次错误可能既走硬件错误又走卡死恢复）。
 *
 * 这里刻意**不**补发 WAIT_SEND：本回调可能在 USARTTransmit 调用栈内被触发（卡死恢复），
 * 补发等于嵌套调用 USARTTransmit，外层失败后还会写 s_tx_now / 槽状态，会把内层刚启动的
 * 槽覆盖掉。积压的 WAIT_SEND 由下一次发送完成回调（TxCplt）带出；
 * 期间若再没有一次 TerminalLiteSend，这些槽就一直挂在 WAIT_SEND，直到下一次发送触发。 */
static void TerminalLiteErrHandler(USARTInstance *instance, USART_ErrReason_e reason)
{
    /* RX_STALLED 与本函数无关：接收停摆由 TerminalLiteExecute 的周期自检 + TerminalLiteRxRecover 处理 */
    if (reason == USART_ERR_RX_STALLED)
        return;

    if (s_tx_now == NULL)
        return;

    /* TX_ABORT：bsp 已强止发送并把 gState 复位；HW：错误位都在接收侧，
     * gState 非 READY 说明发送仍在途，不能归还 DMA 正在读的缓冲 */
    if (reason == USART_ERR_TX_ABORT || (instance != NULL && instance->handle != NULL && instance->handle->gState == HAL_UART_STATE_READY))
    {
        s_tx_now->state = TL_TX_FREE;
        s_tx_now = NULL;
    }
}

// DMA 发送完成回调（ISR 上下文）：归还发完的槽（SEND→FREE），并补发一个 WAIT_SEND
static void TerminalLiteTxCplt(USARTInstance *instance)
{
    (void)instance;
    if (s_tx_now != NULL)
    {
        s_tx_now->state = TL_TX_FREE;
        s_tx_now = NULL;
    }
    for (uint8_t i = 0; i < TERMINAL_LITE_TX_QUEUE_LEN; i++)
    {
        if (s_tx_pool[i].state == TL_TX_WAIT_SEND)
        {
            tx_start_slot(&s_tx_pool[i]);
            break;
        }
    }
}

// 公开函数
void TerminalLiteSend(const char *fmt, ...)
{
    if (!fmt || !s_tl_uart.handle)
        return;

    /* 发送卡死自检：放在最前（早于借槽），因为本模块的发送链一旦卡住就再也调用不到
     * USARTTransmit —— 在途槽永远停在 SEND、新内容全部转 WAIT_SEND、槽池占满后
     * 连借都借不到，终端彻底静默。只有每条发送必经的这里能把状态复位、
     * 并借 err_callback 归还那个 SEND 槽。
     * 本函数是 TX 唯一写者，故这里仍满足"单写者"约定；从 ISR 调用也安全
     * （bsp 入口直接返回 BSP_BUSY），空闲时只做一次 DWT 读。
     * 返回值刻意不消费（bsp 层的分工）：BSP_OK = 本次刚复位过一次卡死发送，bsp 已按
     * WARNING 记日志、已计 tx_recover、并已按 err_callback 契约通知上层归还缓冲
     * （对端就是下面那个 SEND 槽）；BSP_BUSY = 什么都没做（ISR / 空闲 / 未登记句柄）。
     * 本层要的结果只有一个——下面能不能借到槽，看槽池状态就够了。 */
    (void)USARTRecoverTxIfStuck(&s_tl_uart, 0);

    // 借空闲槽；WRITE 期间 DMA/完成回调不会碰该槽，格式化可在临界区外做
    TerminalLiteTxBuf_s *tb = NULL;
    for (uint8_t i = 0; i < TERMINAL_LITE_TX_QUEUE_LEN; i++)
    {
        if (s_tx_pool[i].state == TL_TX_FREE)
        {
            s_tx_pool[i].state = TL_TX_WRITE;
            tb = &s_tx_pool[i];
            break;
        }
    }
    if (tb == NULL)
    {
        BSPLOG(&g_terminal_lite_log, LOG_LEVEL_WARNING, "tx pool full, dropped");
        return;
    }

    // 直接格式化进槽内缓冲（复用 lib_format，与日志同款；无 %f，浮点先定点化由 app echo 处理）
    va_list args;
    va_start(args, fmt);
    int n = LibFormatV(tb->data, sizeof(tb->data), fmt, strlen(fmt), args);
    va_end(args);
    if (n <= 0)
    {
        tb->state = TL_TX_FREE; // 无内容，归还
        return;
    }
    if (n >= (int)sizeof(tb->data))
        n = (int)sizeof(tb->data) - 1; // LibFormatV 至多写满 cap-1
    tb->len = (uint16_t)n;

    // 提交：临界区内判空闲并启动，保证与完成回调互斥；忙则排队等补发
    uint32_t primask = tl_enter_critical();
    if (s_tx_now == NULL)
        tx_start_slot(tb); // 空闲：直接发
    else
        tb->state = TL_TX_WAIT_SEND; // 忙：排队，完成回调扫描补发
    tl_exit_critical(primask);
}

/*============================================
 *              执行（app 任务上下文）
 *============================================*/

/**
 * @brief 解析并执行 s_line 中的一行（Execute 上下文）
 * @note 用 '\0' 原地把行切开最多 3 个词（行已取走，改写 s_line 无害）。
 *       语法/数值/越限都在这里校验，失败直接回 err；成功按该命令 scale 定点化回显。
 */
static void tl_exec_line(void)
{
    char *w[3];
    uint8_t nw = 0;
    char *p = s_line;
    while (nw < 3)
    {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;
        w[nw++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
        {
            *p = '\0';
            p++;
        }
    }
    if (nw == 0) // 空行
        return;

    CmdType_e op;
    if (streq(w[0], "tlset"))
        op = CMD_SET;
    else if (streq(w[0], "tlget"))
        op = CMD_GET;
    else
    {
        TerminalLiteSend(err_text(TL_ERR_CMD));
        return;
    }
    if (nw < 2) // 缺 token
    {
        TerminalLiteSend(err_text(TL_ERR_CMD));
        return;
    }

    // 查表匹配 token
    uint8_t idx = 0;
    uint8_t found = 0;
    for (uint8_t i = 0; i < s_cnt; i++)
    {
        if (streq(s_table[i].token, w[1]))
        {
            idx = i;
            found = 1;
            break;
        }
    }
    if (!found)
    {
        TerminalLiteSend(err_text(TL_ERR_TOKEN));
        return;
    }

    const TerminalLiteCmd_s *c = &s_table[idx];
    if (!(c->type & op)) // type 是允许操作掩码(1/2/3)，op 是本次单一操作(1/2)
    {
        TerminalLiteSend(err_text(TL_ERR_TYPE));
        return;
    }

    float value = 0.0f;
    if (op == CMD_SET)
    {
        if (nw < 3 || parse_float(w[2], &value) != 0)
        {
            TerminalLiteSend(err_text(TL_ERR_VALUE));
            return;
        }
        // min>max 表示不检查
        if (c->min <= c->max && (value < c->min || value > c->max))
        {
            TerminalLiteSend("err: %s\r\n", c->token ? c->token : "?");
            return;
        }
    }

    if (!c->exec)
    {
        TerminalLiteSend(err_text(TL_ERR_CMD));
        return;
    }

    float val = 0.0f;
    // op=CMD_GET/CMD_SET（单一操作，非掩码）；set 值已校验
    int8_t ret = c->exec(op, (op == CMD_SET) ? value : 0.0f, &val);
    // 默认回显：float 无 %f，val*scale 四舍五入转 int32 后 %d 输出
    if (ret == 0)
    {
        uint32_t sc = c->scale ? c->scale : 1u;
        TerminalLiteSend("%s = %d\r\n", c->token ? c->token : "?", (int32_t)(val * (float)sc + (val >= 0 ? 0.5f : -0.5f)));
    }
    else
    {
        TerminalLiteSend("err: %s\r\n", c->token ? c->token : "?");
    }
}

uint8_t TerminalLiteExecute(uint8_t max)
{
    if (!s_table || s_cnt == 0)
        return 0;

    uint8_t n = 0;
    // 逐字节从环形缓冲组行；一行收齐就地解析执行，执行满 max 条即把余下字节留给下次
    while (n < max && s_ring_tail != s_ring_head)
    {
        uint8_t b = s_ring[s_ring_tail];
        s_ring_tail = RING_NEXT(s_ring_tail);

        if (s_line_over) // 上一行超长被丢弃，吞到下一个换行
        {
            if (b == '\r' || b == '\n')
                s_line_over = 0;
            continue;
        }
        if (b == '\r' || b == '\n')
        {
            if (s_line_len == 0) // 连续换行（\r\n / 空行）：忽略
                continue;
            s_line[s_line_len] = '\0';
            s_line_len = 0; // 已取走：立即清行
            tl_exec_line();
            n++;
            continue;
        }
        if (b < 0x20 && b != '\t') // 忽略其他控制字符（退格/EOF 等）
            continue;
        if (s_line_len < (uint16_t)(TERMINAL_LITE_MAX_LINE - 1))
        {
            s_line[s_line_len++] = (char)b;
            continue;
        }
        s_line_over = 1;
        s_line_len = 0;
        BSPLOG(&g_terminal_lite_log, LOG_LEVEL_WARNING, "line overflow, dropped");
    }

    if (s_ring_overrun)
    {
        BSPLOG(&g_terminal_lite_log, LOG_LEVEL_WARNING, "rx ring overrun: %u", s_ring_overrun);
        s_ring_overrun = 0;
    }

    // 接收停摆自检：bsp 内部按 TERMINAL_LITE_RX_RESTART_PERIOD_MS 限频，
    // 接收在跑时直接返回 BSP_BUSY，开销可忽略
    TerminalLiteRxRecover();

    return n;
}

/*============================================
 *              初始化
 *============================================*/

void TerminalLiteInit(const TerminalLiteCmd_s *table, uint8_t cnt)
{
    s_table = table;
    s_cnt = cnt;

    USART_Config_s usart_cfg = {
        .uart_e = TERMINAL_LITE_UART,
        .parent = NULL,
        .rx_callback = TerminalLiteRxHook,
        .tx_callback = TerminalLiteTxCplt,
        .err_callback = TerminalLiteErrHandler,
    };
    /* Register / Config / Receive 三个返回值都只用来决定"要不要往下走"，不做本层的错误上报：
     * 每一个失败分支 bsp 都已按 ERROR/WARNING 记进日志（部分路径还计了 s_usart_status[] 的
     * 对应计数），本层再打一遍只会重复。唯一静默的是 BSP_BUSY——那是背压/已在跑，属正常。
     * 这也是 drv 侧不主动处理错误的统一分工：错误的上报归 bsp，本层只管流程。 */
    if (USARTRegister(&s_tl_uart) == BSP_OK && USARTConfig(&s_tl_uart, &usart_cfg) == BSP_OK)
    {
        // 启动接收常开流（Config 不再自动启动接收，收到的字节由 TerminalLiteRxHook 取走）。
        // 不因失败而放弃：TerminalLiteExecute 的周期自检会按同一套参数反复重启它
        // （TerminalLiteRxRecover），此刻放弃反而把自恢复一起关掉了。
        (void)USARTReceive(&s_tl_uart, s_tl_uart.rx_buff_size, BSP_DMA_MODE, 0);
    }

    BSPLOG(&g_terminal_lite_log, LOG_LEVEL_INFO, "Initialized, UART=%d cmds=%u", (int)TERMINAL_LITE_UART, cnt);
}

#else // !(DRV_TERMINAL_LITE_USED) || !(TERMINAL_LITE_UART)

void TerminalLiteInit(const TerminalLiteCmd_s *table, uint8_t cnt)
{
    (void)table;
    (void)cnt;
}

uint8_t TerminalLiteExecute(uint8_t max)
{
    (void)max;
    return 0;
}

void TerminalLiteSend(const char *fmt, ...)
{
    (void)fmt;
}

#endif // (defined(DRV_TERMINAL_LITE_USED)) && (defined(TERMINAL_LITE_UART))

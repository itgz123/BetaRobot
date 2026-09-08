/**
 * @file drv_terminal_lite.c
 * @brief 串口调参极简终端实现（只收发消息）
 *
 * @note 数据流（线程模型）：
 *   - bsp_usart RX 中断里只把整段字节拷进环形缓冲并唤醒小任务（不含任何业务）；
 *   - 模块小任务(TerminalLiteTaskFunc) 从环形缓冲逐字节组行(\r/\n) → 解析 tlget/tlset
 *     → 匹配 token → 把事件压入"待执行队列"(不阻塞，满则丢)；
 *   - app 在自己的任务里周期调 TerminalLiteExecute() 取事件执行 exec→echo（业务上下文）；
 *   - TX 只有一个写者（都在 TerminalLiteExecute 调用链里），内部再排一个小队列抗连发。
 *
 * @note 启用门控（仿 drv_vofa）：需在 app_cfg.h 同时定义 DRV_TERMINAL_LITE_USED 与
 *       TERMINAL_LITE_UART，否则本文件全部为空实现。
 */

#include "drv_terminal_lite.h"

#if (defined(DRV_TERMINAL_LITE_USED)) && (defined(TERMINAL_LITE_UART))

#include "bsp_usart.h"
#include "bsp_freertos.h"
#include "bsp_log.h"
#include "lib_format.h"
#include <stdarg.h>
#include <string.h>

#if (TERMINAL_LITE_RING_SIZE & (TERMINAL_LITE_RING_SIZE - 1)) != 0
#error "TERMINAL_LITE_RING_SIZE must be a power of two!"
#endif

/*------------- 内部事件/错误码 --------------*/

// 事件错误码（err 字段取值）
typedef enum
{
    TL_NORMAL = 0,    // 正常，无错误
    TL_ERR_CMD = 1,   // 未知命令 / 缺 token / 缺 exec
    TL_ERR_TOKEN = 2, // 未知 token
    TL_ERR_TYPE = 3,  // 该 token 不允许该操作
    TL_ERR_VALUE = 4, // 数值解析失败 / 缺参
    TL_ERR_RANGE = 5, // tlset 值越界（超出表内 min/max）
} TerminalLiteErr_e;

// 待执行命令事件（模块小任务 → app 任务）。err!=0 时为错误事件（idx 无效）
typedef struct
{
    uint8_t idx;           // 命令表下标（命中时）
    CmdType_e op;          // 本次具体操作：CMD_GET(=1) 读 / CMD_SET(=2) 写（只传单一操作，不带掩码 3）
    TerminalLiteErr_e err; // 0=正常，否则错误码（见 TerminalLiteErr_e）
    float value;           // tlset 的写入值
} TerminalLiteEvent_s;

/*------------- 环形缓冲（ISR 写 / 小任务读） --------------*/

static volatile uint8_t s_ring[TERMINAL_LITE_RING_SIZE];
static volatile uint16_t s_ring_head = 0; // 写指针（ISR）
static volatile uint16_t s_ring_tail = 0; // 读指针（小任务）
static volatile uint16_t s_ring_overrun = 0;
#define RING_NEXT(i) (((i) + 1) & (TERMINAL_LITE_RING_SIZE - 1))

/*------------- 命令表（Init 保存指针，表存 flash） --------------*/

static const TerminalLiteCmd_s *s_table = NULL;
static uint8_t s_cnt = 0;

/*------------- 实例定义（日志 / USART / 任务 / 队列） --------------*/

LOG_INSTANCE_DEF(g_terminal_lite_log, "drv_terminal_lite", TERMINAL_LITE_LOG_LIMIT);

USART_INSTANCE_DEF(s_tl_uart, TERMINAL_LITE_MAX_LINE);
TASK_INSTANCE_DEF(s_tl_task, TERMINAL_LITE_TASK_STACK);
QUEUE_INSTANCE_DEF(s_exec_q, TERMINAL_LITE_EXEC_QUEUE_LEN, TerminalLiteEvent_s);
static TaskHandle_t s_tl_task_h = NULL; // RX ISR 用任务通知唤醒（来数据即醒，无计数攒批）
static QueueHandle_t s_exec_q_h = NULL; // 待执行事件队列

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

/*------------- 行缓冲（小任务私有） --------------*/

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
 *              事件入队
 *============================================*/

static void push_event(const TerminalLiteEvent_s *ev)
{
    if (!s_exec_q_h)
        return;
    if (xQueueSend(s_exec_q_h, ev, 0) != pdTRUE)
    {
        BSPLOG(&g_terminal_lite_log, LOG_LEVEL_WARNING, "exec queue full, cmd dropped");
    }
}

static void push_err(TerminalLiteErr_e err)
{
    TerminalLiteEvent_s ev;
    memset(&ev, 0, sizeof(ev));
    ev.err = err;
    push_event(&ev);
}

/*============================================
 *              RX：ISR + 小任务
 *============================================*/

// bsp_usart RX 回调（ISR 上下文）：只拷字节 + 唤醒，不解析
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

    // 任务通知唤醒：每收到一段数据即醒一次（二值语义，无计数攒批；ring 保证字节不丢）
    if (s_tl_task_h)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(s_tl_task_h, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// 小任务：收字节组行 → 行齐就地解析 tlget/tlset → 事件入队（原 task_byte/parse_line 已内联）
static void TerminalLiteTaskFunc(void *argument)
{
    (void)argument;
    for (;;)
    {
        // 采集：逐字节从环形缓冲组行。一行以 \r/\n 收齐即退出采集去解析；
        // 环形被取空仍无完整行 → 本轮结束（下方据此决定是否阻塞）。
        uint8_t line_ready = 0;
        while (s_ring_tail != s_ring_head)
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
                s_line_len = 0; // 已取走：立即清行，解析期间可安全收下一行
                line_ready = 1;
                break;
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
            uint16_t o = s_ring_overrun;
            s_ring_overrun = 0;
            BSPLOG(&g_terminal_lite_log, LOG_LEVEL_WARNING, "rx ring overrun: %u", o);
        }
        if (!line_ready)
        {
            // 环形空且无完整行：阻塞等下次 RX 通知（clearOnExit=pdTRUE：通知清零，不攒批）
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }

        // ============ 解析刚收齐的一行（原 parse_line 内联） ============
        // 用 '\0' 原地把行切开最多 3 个词（行已取走，改写 s_line 无害）
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
            continue;

        CmdType_e op;
        if (streq(w[0], "tlset"))
            op = CMD_SET;
        else if (streq(w[0], "tlget"))
            op = CMD_GET;
        else
        {
            push_err(TL_ERR_CMD);
            continue;
        }
        if (nw < 2) // 缺 token
        {
            push_err(TL_ERR_CMD);
            continue;
        }

        // 查表匹配 token
        uint8_t idx = 0;
        uint8_t found = 0;
        if (s_table && s_cnt)
        {
            for (uint8_t i = 0; i < s_cnt; i++)
            {
                if (streq(s_table[i].token, w[1]))
                {
                    idx = i;
                    found = 1;
                    break;
                }
            }
        }
        if (!found)
        {
            push_err(TL_ERR_TOKEN);
            continue;
        }

        if (!(s_table[idx].type & op)) // type 是允许操作掩码(1/2/3)，op 是本次单一操作(1/2)
        {
            push_err(TL_ERR_TYPE);
            continue;
        }

        TerminalLiteEvent_s ev;
        memset(&ev, 0, sizeof(ev));
        ev.idx = idx;
        ev.op = op;
        if (op == CMD_SET)
        {
            if (nw < 3 || parse_float(w[2], &ev.value) != 0)
            {
                push_err(TL_ERR_VALUE);
                continue;
            }
            // 越限校验与 tlget/tlset 语法校验同批做（小任务解析阶段）；min>max 表示不限
            const TerminalLiteCmd_s *c = &s_table[idx];
            if (c->min <= c->max && (ev.value < c->min || ev.value > c->max))
            {
                ev.err = TL_ERR_RANGE; // 越界：错误事件带命令表下标，执行时回显 token 名
                push_event(&ev);
                continue;
            }
        }
        push_event(&ev);
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
    case TL_ERR_RANGE:
        return "err: out of range\r\n";
    default:
        return "err\r\n";
    }
}

// 前置：调用方已在临界区（task）或处于 ISR（TX 完成回调）
// 置 SEND 并启动 DMA 发送一个槽（调用方须已临界 / 处于 ISR，s_tx_now 不被并发改）
static void tx_start_slot(TerminalLiteTxBuf_s *tb)
{
    tb->state = TL_TX_SEND;
    s_tx_now = tb;
    if (USARTTransmit(&s_tl_uart, (uint8_t *)tb->data, tb->len, 0) != 0)
    {
        // DMA 启动失败：归还本槽；后续若有 WAIT_SEND 由下次完成回调补发
        tb->state = TL_TX_FREE;
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
    taskENTER_CRITICAL();
    if (s_tx_now == NULL)
        tx_start_slot(tb); // 空闲：直接发
    else
        tb->state = TL_TX_WAIT_SEND; // 忙：排队，完成回调扫描补发
    taskEXIT_CRITICAL();
}

/*============================================
 *              执行（app 任务上下文）
 *============================================*/

uint8_t TerminalLiteExecute(uint8_t max)
{
    if (!s_exec_q_h || !s_table || s_cnt == 0)
        return 0;

    uint8_t n = 0;
    TerminalLiteEvent_s ev;
    while (n < max && xQueueReceive(s_exec_q_h, &ev, 0) == pdTRUE)
    {
        n++;
        if (ev.err)
        {
            // 越界错误事件带命令表下标：回显带出 token 名；其余解析错误用固定文本
            if (ev.err == TL_ERR_RANGE && ev.idx < s_cnt && s_table[ev.idx].token)
                TerminalLiteSend("err: %s\r\n", s_table[ev.idx].token);
            else
                TerminalLiteSend(err_text(ev.err));
            continue;
        }
        if (ev.idx >= s_cnt || !s_table[ev.idx].exec)
        {
            TerminalLiteSend(err_text(TL_ERR_CMD));
            continue;
        }
        const TerminalLiteCmd_s *c = &s_table[ev.idx];
        float val = 0.0f;
        // 越限已在解析阶段拦截，这里只执行合法命令；op=CMD_GET/CMD_SET（单一操作，非掩码）
        int8_t ret = c->exec(ev.op, (ev.op == CMD_SET) ? ev.value : 0.0f, &val);
        // 默认回显：float 无 %f，val*scale 四舍五入转 int32 后 %d 输出
        if (ret == 0)
        {
            uint32_t sc = c->scale ? c->scale : 1u;
            TerminalLiteSend("%s = %d\r\n", c->token ? c->token : "?",
                             (int32_t)(val * (float)sc + (val >= 0 ? 0.5f : -0.5f)));
        }
        else
        {
            TerminalLiteSend("err: %s\r\n", c->token ? c->token : "?");
        }
    }
    return n;
}

/*============================================
 *              初始化
 *============================================*/

void TerminalLiteInit(const TerminalLiteCmd_s *table, uint8_t cnt)
{
    s_table = table;
    s_cnt = cnt;

    s_exec_q_h = QueueRegister(&s_exec_q);

    USARTRegister(&s_tl_uart);
    USART_Config_s usart_cfg = {
        .uart_e = TERMINAL_LITE_UART,
        .tx_mode = USART_DMA_MODE,
        .rx_callback = TerminalLiteRxHook,
        .tx_callback = TerminalLiteTxCplt,
    };
    USARTConfig(&s_tl_uart, &usart_cfg);

    Task_Init_Config_s task_cfg = {
        .func = TerminalLiteTaskFunc,
        .priority = TERMINAL_LITE_TASK_PRIORITY,
    };
    s_tl_task_h = TaskRegister(&s_tl_task, &task_cfg);

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

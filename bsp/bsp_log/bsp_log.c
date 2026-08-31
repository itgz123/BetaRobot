/**
 * @file bsp_log.c
 * @brief 日志系统实现（日志层）
 *
 * @note 实现 BSPLOG 宏的核心 BSPLogV()：限频检查 + 组装
 *       "[颜色][分级][时间戳][模块名]:内容[重置]\r\n"。
 *
 * 缓冲池设计（不占调用方栈、生命周期长、DMA 友好）：
 *   - bsp_log.c 静态区定义 log_buf_s 缓冲池（LOG_BUF_NUM 个，每槽
 *     data + 状态 + 长度）。BSPLogV 借用空闲缓冲，用 BSPFormatV 一次模板
 *     格式化（头部模板 LOG_HDR_FMT：颜色/分级/时间戳/模块名，固定串从 flash
 *     拷贝、数字格式化到其中）直接写进 data，随后 uart 空闲时直接 DMA 发送、
 *     忙时置 WAIT_SEND 排队等完成回调调度。
 *   - 状态机防可重入冲突：WRITE（借用后未提交）/SEND（已提交传输层）期间
 *     该缓冲不可再借，中断里再调 BSPLOG 会拿到别的空闲缓冲。
 *   - 传输层 DMA 完成后 LogUartTxCplt 归还缓冲并调度下一个，否则池耗尽。
 *   - 借用/归还/状态切换只做单字节状态写（uint8_t，MCU 上单字节写原子），
 *     无需关中断；竞态最坏只会让两条日志复用同一缓冲（内容覆盖），可接受。
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

/*============ 缓冲池（静态区） ============*/
typedef enum : uint8_t
{
    LOG_BUF_FREE = 0,  /* 空闲：可借出 */
    LOG_BUF_WRITE,     /* 写入：BSPLogV 正在格式化，未提交 */
    LOG_BUF_SEND,      /* 发送：已提交传输层，DMA 排队/发送中 */
    LOG_BUF_WAIT_SEND, /* 等待发送：发送完成回调会扫描s_buf_state，如果有待发送就发送 */
} LOG_BUF_STATE;

/*============ uart相关 ============*/
typedef struct
{
    char buf_pool[LOG_LEN_MAX];       /* 生命周期长，DMA 直接读取 */
    volatile LOG_BUF_STATE buf_state; /* LOG_BUF_STATE */
    uint16_t buf_len;                 /* 每缓冲有效长度：WAIT_SEND 出队发送时需要 */
} log_buf_s;

static log_buf_s s_log_buf[LOG_BUF_NUM];
USART_INSTANCE_DEF(s_log_uart, 1);          /* 日志串口实例（TX DMA 完成中断回调 = LogUartTxCplt） */
static volatile log_buf_s *s_tx_buf = NULL; /* 当前 DMA 发送中的缓冲（完成回调里归还并调度下一个） */

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
            s_log_buf[i].buf_state = LOG_BUF_SEND;
            s_tx_buf = &s_log_buf[i];
            USARTTransmit(&s_log_uart, (uint8_t *)s_log_buf[i].buf_pool, s_log_buf[i].buf_len, 0);
            break;
        }
    }
}

/*============ 内部工具（static） ============*/

/**
 * @brief 限频检查：通过返回 1，超频返回 0（本条丢弃）
 * @note 1 秒窗口：窗口内计数达到 times_per_second 后丢弃，距窗口起点满
 *       1 秒重置计数并记录新窗口起点。
 */
static int BSPLogCheckLimit(LOGInstance *inst)
{
    uint64_t now_us;

    if (inst->times_per_second == 0)
    {
        return 1; /* 不限频 */
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

/* 日志头部模板：颜色+分级+[时间戳]+模块名+冒号，一次 BSPFormatEx 格式化。
 * 时间戳风格由 TIME_STAMP_STYLE 编译期决定（0=无 / 1=us / 2=ms）。
 * LOG_HDR_ARGS 同步调整变参个数（STYLE==0 只传 3 个，不含 st）——
 * va_list 顺序消费，多传的参数会错位，必须与模板转换个数严格一致。 */
#if TIME_STAMP_STYLE == 0
#define LOG_HDR_FMT "%s[%s][%s]:"
#define LOG_HDR_ARGS(color, lvl, st, module) (color), (lvl), (module)
#else
#define LOG_HDR_FMT "%s[%s][%u][%s]:"
#define LOG_HDR_ARGS(color, lvl, st, module) (color), (lvl), (st), (module)
#endif

/*============ 外部接口（3 个） ============*/

/* 默认日志实例：开箱即用，BSPLogInit 里初始化为模块名 "log"、不限频 */
LOGInstance g_log = {0};

void BSPLogInit(void)
{
    /* 默认实例：模块名 "log"、不限频，供 BSPLOG(&g_log, ...) 直接使用 */
    BSPLogInitInstance(&g_log, &(LOG_Config_s){.module_name = "log"});

    if (USARTRegister(&s_log_uart) != 0)
    {
        return;
    }
    USART_Config_s cfg = {
        .uart_e = LOG_UART,
        .tx_mode = USART_DMA_MODE,
        .rx_callback = NULL,
        .tx_callback = LogUartTxCplt,
        .timeout_ms = 0,
    };
    USARTConfig(&s_log_uart, &cfg);
}

void BSPLogInitInstance(LOGInstance *inst, LOG_Config_s *cfg)
{
    uint8_t n = 0;
    const char *name = (cfg != NULL) ? cfg->module_name : NULL;

    if (name != NULL)
    {
        while (name[n] != '\0' && n < MODULE_NAME_LEN_MAX - 1)
        {
            inst->module_name[n] = name[n];
            n++;
        }
    }
    inst->module_name[n] = '\0';
    inst->module_name_len = n;

    inst->times_per_second = (cfg != NULL) ? cfg->times_per_second : 0;
    inst->log_cnt = 0;
    /* 窗口起点记为 init 时刻：1 秒窗口从配置时刻开始计算 */
    inst->last_timestamp_us = DWT_GetTimeUs();
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
    buf = lb->buf_pool;

    /* 直接格式化进静态缓冲（不占调用方栈）：头部固定段一次模板格式化，
     * 颜色/分级/时间戳/模块名全走 %s/%u，不再逐段 Append */
    end = buf + LOG_LEN_MAX;
    p = buf;
    {
        uint32_t st = 0;
#if TIME_STAMP_STYLE != 0
        st = (uint32_t)DWT_GetTimeUs();
        if (TIME_STAMP_STYLE == 2)
        {
            st = st / 1000u;
        }
#endif
        (void)st; /* STYLE==0 时不参与格式化 */
        p += BSPFormatEx(p, (size_t)(end - p), LOG_HDR_FMT, sizeof(LOG_HDR_FMT) - 1,
                         LOG_HDR_ARGS(BSPLogLevelColor(level), BSPLogLevelStr(level), st, inst->module_name));
    }

    va_start(args, fmt);
    n = BSPFormatV(p, (size_t)(end - p), fmt, strlen(fmt), args);
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

        /* 提交：WRITE → SEND 允许 DMA 读取；uart 空闲直发，忙则排队（内联原 BSPLogMediaSend） */
        lb->buf_state = LOG_BUF_SEND;
        if (USARTIsReady(&s_log_uart))
        {
            s_tx_buf = lb; /* 传输层持有直到 DMA 完成，完成回调归还 */
            if (USARTTransmit(&s_log_uart, (uint8_t *)buf, (uint16_t)len, 0) != 0)
            {
                lb->buf_state = LOG_BUF_FREE; /* 启动失败：立即归还 */
                s_tx_buf = NULL;
            }
        }
        else
        {
            lb->buf_state = LOG_BUF_WAIT_SEND; /* 排队，完成回调扫描发送 */
        }
    }
}

#endif // (defined(BSP_LOG_USED))||(defined(LOG_UART))

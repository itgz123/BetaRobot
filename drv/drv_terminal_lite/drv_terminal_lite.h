/**
 * @file drv_terminal_lite.h
 * @brief 串口调参极简终端（只收发消息，无光标/删除等行编辑）
 *
 * @note 与 vofa/完整 terminal 的定位差异：
 *       - 方向与 vofa 相反：vofa 只上行发 float 帧、不能收文本命令；
 *         terminal_lite 从串口收文本命令并允许自定义回显。
 *       - 与 log 分离：命令/回显走 TERMINAL_LITE_UART，日志仍走 LOG_UART。
 *       - 无行编辑：本版本只把收到的字节按 \r / \n 组行并解析成标准命令。
 *
 * @note 数据流（无模块任务）：
 *       RX ISR 只把串口字节拷入内部环形缓冲；
 *       app 在自己的任务里周期调 TerminalLiteExecute()：从环形缓冲组行 → 解析
 *       tlget/tlset → 匹配 token → 就地执行 exec → 按该命令的 scale 定点化回显。
 *       手动调参的发送周期一般 5~10s，轮询处理完全够用，故本模块不创建任务/队列。
 *
 * @note 命令格式：
 *       tlget <token>         读：exec(type=CMD_GET, value=0, &pval)
 *       tlset <token> <value> 写：exec(type=CMD_SET, value=<float>, &pval)
 *       token 需在 TerminalLiteInit 时注册的命令表里；value 为 float。
 *
 * @note 启用方式（仿 vofa/log）：在 app_cfg.h 定义
 *       #define DRV_TERMINAL_LITE_USED
 *       #define TERMINAL_LITE_UART UART_10    // 该串口需已配 RX/TX DMA
 *       未启用时本模块所有接口为空实现。
 */

#ifndef __DRV_TERMINAL_LITE_H
#define __DRV_TERMINAL_LITE_H

#include <stdint.h>
#include <stddef.h>
#include "app_cfg.h"

/*============================================
 *              配置（可在 app_cfg.h 预定义覆盖）
 *============================================*/

#ifndef TERMINAL_LITE_MAX_LINE
#define TERMINAL_LITE_MAX_LINE 32 // 单行最大字符数(不含'\0')，同时用作 bsp_usart RX DMA 段大小（一行须能整段收齐）
#endif
#ifndef TERMINAL_LITE_RING_SIZE
#define TERMINAL_LITE_RING_SIZE 128 // ISR→Execute 环形缓冲大小（必须为 2 的幂）
#endif
#ifndef TERMINAL_LITE_TX_BUF
#define TERMINAL_LITE_TX_BUF 32 // 单条回显/发送最大字节数（超长截断）
#endif
#ifndef TERMINAL_LITE_TX_QUEUE_LEN
#define TERMINAL_LITE_TX_QUEUE_LEN 8 // 发送队列深度（按每帧 execute 条数配；满则丢弃并记日志）
#endif
#ifndef TERMINAL_LITE_LOG_LIMIT
#define TERMINAL_LITE_LOG_LIMIT 10 // 模块日志限频（条/秒）
#endif
#ifndef TERMINAL_LITE_RX_RESTART_PERIOD_MS
#define TERMINAL_LITE_RX_RESTART_PERIOD_MS 100 // 接收停摆自检的限频周期（ms），在 TerminalLiteExecute 里检查
#endif

/*============================================
 *              类型定义
 *============================================*/

/**
 * @brief 命令允许的操作（按位掩码使用）
 */
typedef enum : uint8_t
{
    CMD_GET = 0b00000001,            // 允许 tlget（读）
    CMD_SET = 0b00000010,            // 允许 tlset（写）
    CMD_GET_SET = CMD_GET | CMD_SET, // 允许读 + 写
} CmdType_e;

/**
 * @brief 一条命令 = 一个 token。app 在 TerminalLiteInit 时提供该表 + 条数
 */
typedef struct
{
    const char *token; // 匹配 tlget/tlset 的 <token>，如 "kp"
    CmdType_e type;    // CmdType_e：该 token 允许的操作掩码
    uint32_t scale;    // 默认回显定点化倍率（无 %f）：成功回 "token = <v>\r\n"，
                       // <v> = (int32_t)(val*scale)（四舍五入），如 scale=1000 显示千分位；0=按 1
    float min;         // tlset 写入值下限（含）。min>max 表示不检查（用 TERMINAL_LITE_NO_BOUND）
    float max;         // tlset 写入值上限（含）。越限视作 exec 失败 → 回 "err: <token>"，不调 exec
    // exec: type=CMD_SET 把 value 写入（set 值已由模块按 min/max 校验过）；type=CMD_GET 把当前值读到 *pval
    //       返回 0=成功，非 0=失败（如只读，失败回 "err: <token>\r\n"）
    //       type 为本次具体操作（单一 bit）：模块只传 CMD_GET(=1) 或 CMD_SET(=2)，不传掩码 CMD_GET_SET(=3)
    int8_t (*exec)(CmdType_e type, float value, float *pval);
} TerminalLiteCmd_s;

/* 命令表行无 set 上下限时，min/max 两字段用它（min=1 > max=-1 哨兵，模块不校验） */
#define TERMINAL_LITE_NO_BOUND 1.0f, -1.0f

/*============================================
 *              外部接口声明
 *============================================*/

/**
 * @brief 初始化终端（注册/配置串口 + 启动接收 + 收命令表）
 * @param table 命令表数组指针（须静态存储，模块只保存指针）
 * @param cnt   命令条数
 * @note 不创建任务或队列；在 app 初始化流程里、调度器启动前调用（与 DaemonInit 一致）
 */
void TerminalLiteInit(const TerminalLiteCmd_s *table, uint8_t cnt);

/**
 * @brief 处理接收缓冲并执行命令（业务入口）
 * @param max 本帧最多执行的命令条数
 * @return 实际处理的条数
 * @note 由 app 在自己的任务里周期调用（缓冲空时几乎零开销）。本函数从环形缓冲
 *       组行 → 解析 tlget/tlset → 校验语法/数值/越限(min/max) → 执行 exec →
 *       按该命令 scale 定点化回显，全在同一上下文完成；解析错直接回 err。
 *       每次调用还会做一次接收停摆自检（bsp 内部按 TERMINAL_LITE_RX_RESTART_PERIOD_MS 限频）。
 */
uint8_t TerminalLiteExecute(uint8_t max);

/**
 * @brief 直接格式化并发送回显文本（接口同日志：fmt + 变参，\r\n 自行写在 fmt 里）
 * @param fmt 格式串，仅支持 lib_format 的 %d/%u/%x/%X/%s（不支持 %f/%c/%%，见 lib_format.h）
 * @param ... 对应变参
 * @note float 值请先乘固定倍数转 int32 再 %d 输出（模块不提供浮点格式化）；
 *       约定只在调 TerminalLiteExecute 的那个任务里调用（TX 单写者）；
 *       超长自动截断到 TERMINAL_LITE_TX_BUF。
 */
void TerminalLiteSend(const char *fmt, ...);

#endif // __DRV_TERMINAL_LITE_H

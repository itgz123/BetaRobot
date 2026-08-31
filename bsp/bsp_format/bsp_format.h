/**
 * @file bsp_format.h
 * @brief BSP 层快速格式化模块（零除法整数转换）
 *
 * @note 目标场景：日志系统的"编译期固定字符串 + 整数"快速格式化。
 *       支持整数转换说明 %d %u %x %X 与字符串 %s，均可带固定宽度。
 *       对外只暴露一个宏 BSPFORMAT 和它内部调用的一个函数 BSPFormatEx。
 *       十进制整数转换全程零除法：低位(<1000) 查 3 位 flash 表，
 *       高位用分层减法剥离（每级最多 9 次减法）。十六进制用位运算。
 *       不依赖 libc 的 snprintf/vsnprintf。
 *
 * 支持的转换说明（5 种）：
 *   %d  有符号十进制整数
 *   %u  无符号十进制整数
 *   %x  十六进制小写
 *   %X  十六进制大写
 *   %s  字符串（const char*；NULL 安全，输出 "(null)"）
 *
 * 宽度：作为最大输出长度，如 %5d / %8x：整个数字串（含负号）超过
 *       width 个字符时，从高位截断保留 width 个；不超过则原样输出
 *       （不补空格/零）。字符串同理：%s 超过 width 个字符时保留前
 *       width 个，不补空格/零。width == 0（不写宽度）表示无限制。
 *
 * 明确不支持：%c %i %% %f %p 及 flags(-/0/+/空格/#)、长度修饰符(h/l/ll/...)、
 *   精度(.N)、动态宽度(*)。未支持的转换说明按字面输出该字符（容错，不崩）；
 *   注意格式串中若出现 %%，两个 % 都会按字面输出。
 *
 * 约束：格式串必须是编译期字符串字面量（BSPFORMAT 宏依赖 sizeof(fmt) 计算长度）。
 */

#ifndef __BSP_FORMAT_H
#define __BSP_FORMAT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/*------------- 外部接口 --------------*/

/**
 * @brief 带格式串长度版本的变参格式化函数
 * @param out     输出缓冲区，末尾自动补 '\0'
 * @param cap     缓冲区容量（字节）。cap<=1 时只写终止符；cap==0 或 out==NULL 返回 0
 * @param fmt     格式串（约束：必须是字符串字面量，见 BSPFORMAT）
 * @param fmt_len 格式串长度（编译期由 BSPFORMAT 宏注入）
 * @param ...     与格式串对应的变参
 * @return 本应写入的字符数（不含终止符）；截断时返回完整长度，调用者据此判断是否溢出
 * @note 一般不应直接调用，统一通过 BSPFORMAT 宏（fmt 为字面量时免运行期 strlen）。
 */
int BSPFormatEx(char *out, size_t cap, const char *fmt, size_t fmt_len, ...);

/**
 * @brief va_list 版本：供日志层 BSPLogV 等已持有 va_list 的实现调用
 * @param args 已 va_start 的变参列表（调用者负责 va_end）
 * @return 同 BSPFormatEx
 */
int BSPFormatV(char *out, size_t cap, const char *fmt, size_t fmt_len, va_list args);

/**
 * @brief 编译期固定格式串宏：fmt 为字面量时，sizeof(fmt)-1 在编译期求出，
 *       直接注入长度，格式串零运行期 strlen 开销。
 * @warning fmt 必须是字符串字面量，传入变量会因 sizeof(指针) 得到错误长度。
 * @note char buf[64];
 *       int n = BSPFORMAT(buf, sizeof(buf), "motor=%d speed=%u\n", id, speed);
 *       以上例子，如果格式化之后结果大于sizeof(buf)，后面消息将被截断
 */
#define BSPFORMAT(out, cap, fmt, ...) \
    BSPFormatEx(out, cap, fmt, sizeof(fmt) - 1, ##__VA_ARGS__)

#endif /* __BSP_FORMAT_H */

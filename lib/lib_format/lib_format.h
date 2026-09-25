/**
 * @file lib_format.h
 * @brief lib 层快速格式化模块（零除法整数转换）
 *
 * @note 目标场景：日志系统的"编译期固定字符串 + 整数"快速格式化。
 *       支持整数转换说明 %d %u %x %X 与字符串 %s，均可带固定宽度。
 *       对外只暴露一个宏 LIBFORMAT 和它内部调用的一个函数 LibFormatEx。
 *       十进制整数转换全程零除法：低位(<1000) 查 3 位 flash 表，
 *       高位用分层减法剥离（每级最多 9 次减法）。十六进制用位运算。
 *       不依赖 libc 的 snprintf/vsnprintf。
 *
 * 支持的转换说明（5 种，整数均可带可选长度修饰符 l（32 位）/ ll（64 位））：
 *   %d  有符号十进制整数（32 位：按 int 读取）
 *   %u  无符号十进制整数（32 位：按 uint32_t 读取）
 *   %x  十六进制小写（32 位：按 uint32_t 读取）
 *   %X  十六进制大写（32 位：按 uint32_t 读取）
 *   %s  字符串（const char*；NULL 安全，输出 "(null)"）
 *
 * 位数约束：
 *   - 不带修饰符的 %d/%u/%x/%X 按 32 位（int/uint32_t）读取；
 *     8/16 位整数参数会先经 C 默认整数提升成 int/unsigned int，传 %d/%u/%x
 *     可正常显示（值域不超 32 位）。
 *   - %l/%ll 按 C 标准语义（与目标 ABI 一致，arm-none-eabi 上 int/long 均为 32 位）：
 *     l → 32 位（%ld 读 int32_t，%lu/%lx 读 uint32_t）；
 *     ll → 64 位（%lld 读 int64_t，%llu/%llx 读 uint64_t）。
 *     注意 l 与 ll 不能混用：给 %lX 传 64 位实参、或给 %llu 传 32 位实参，
 *     都会因参数槽数量不符而让同一格式串后续转换全部错位。
 *   - 其余长度修饰符（h/z/...）不识别，按字面输出（见下面"未支持的说明符"一节）。
 *     64 位十进制转换用 64 位魔数乘法+移位（%1000 分块查表），无硬件除法指令。
 *
 * 宽度：作为最大输出长度，如 %5d / %8x：整个数字串（含负号）超过
 *       width 个字符时，从高位截断保留 width 个；不超过则原样输出
 *       （不补空格/零）。字符串同理：%s 超过 width 个字符时保留前
 *       width 个，不补空格/零。width == 0（不写宽度）表示无限制。
 *
 * %% ：支持，输出一个字面 %（与 C 标准一致），不消费参数；格式串以单个 % 结尾
 *   （"duty=100%"）时同样输出那个 %，不静默吞字符。
 *
 * 明确不支持：%c %i %f %p %o 及 flags(-/0/+/空格/#)、长度修饰符(h/z/...)、
 *   精度(.N)、动态宽度(*)。未支持的说明符**原样输出 "%" + 该字符**（容错，不崩，
 *   日志里能直接看出调用方写了什么不支持的说明符），且若该字符是字母
 *   （%p/%c/%i/%f/%zu/%hd...）则**再消费一个参数槽**：调用方既然写了转换说明，
 *   多半也为它传了实参，消费掉才能让同一格式串里后续转换不错位——不消费的后果是
 *   读到别人的实参且不报错，比某个字段打印得不好看严重得多（bsp_adc 的
 *   "handle=0x%p, channel=%lu" 曾把 handle 指针当成 channel 打出来）。
 *   % 后不是字母时（flags 写法 %-5d、或消息正文里的 "100% 占空比"）不消费参数、
 *   原文照抄：这一档字面上分不开（flags 字符在正文里就是标点），选择照顾正文。
 *   所以 %-5d 不是"宽度不生效"而是**整体错位**（后续每个实参前移一格），本仓库不用它；
 *   同理不处理动态宽度 %*d 与精度 .N。
 *   已知例外：浮点（%f/%e/%g），double 按 ABI 占两个参数槽，消费一个仍会错位——
 *   本仓库的日志不使用浮点，要打印浮点请先自行转成整数或字符串。
 *   查全仓库有没有踩到：`python3 lib/lib_format/tools/scan_fmt_specs.py`。
 *   l/ll 长度修饰符已支持（见上）。
 *
 * 约束：格式串必须是编译期字符串字面量（LIBFORMAT 宏依赖 sizeof(fmt) 计算长度）。
 */

#ifndef __LIB_FORMAT_H
#define __LIB_FORMAT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/*------------- 外部接口 --------------*/

/**
 * @brief 带格式串长度版本的变参格式化函数
 * @param out     输出缓冲区，末尾自动补 '\0'
 * @param cap     缓冲区容量（字节）。cap<=1 时只写终止符；cap==0 或 out==NULL 返回 0
 * @param fmt     格式串（约束：必须是字符串字面量，见 LIBFORMAT）
 * @param fmt_len 格式串长度（编译期由 LIBFORMAT 宏注入）
 * @param ...     与格式串对应的变参
 * @return 本应写入的字符数（不含终止符）；截断时返回完整长度，调用者据此判断是否溢出
 * @note 一般不应直接调用，统一通过 LIBFORMAT 宏（fmt 为字面量时免运行期 strlen）。
 */
int LibFormatEx(char *out, size_t cap, const char *fmt, size_t fmt_len, ...);

/**
 * @brief va_list 版本：供日志层 BSPLogV 等已持有 va_list 的实现调用
 * @param args 已 va_start 的变参列表（调用者负责 va_end）
 * @return 同 LibFormatEx
 */
int LibFormatV(char *out, size_t cap, const char *fmt, size_t fmt_len, va_list args);

/**
 * @brief 编译期固定格式串宏：fmt 为字面量时，sizeof(fmt)-1 在编译期求出，
 *       直接注入长度，格式串零运行期 strlen 开销。
 * @warning fmt 必须是字符串字面量，传入变量会因 sizeof(指针) 得到错误长度。
 * @note char buf[64];
 *       int n = LIBFORMAT(buf, sizeof(buf), "motor=%d speed=%u\n", id, speed);
 *       以上例子，如果格式化之后结果大于sizeof(buf)，后面消息将被截断
 */
#define LIBFORMAT(out, cap, fmt, ...) LibFormatEx(out, cap, fmt, sizeof(fmt) - 1, ##__VA_ARGS__)

#endif /* __LIB_FORMAT_H */

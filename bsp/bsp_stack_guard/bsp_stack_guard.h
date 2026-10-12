/**
 * @file bsp_stack_guard.h
 * @brief GCC 栈保护（-fstack-protector-strong）的运行期对接
 *
 * 背景：GCC 的栈保护是「编译期插检查 + 运行期两个符号」两截。arm-none-eabi 只装了
 *       前半截——工具链里没有 libssp，链接会报 cannot find -lssp。本模块把后半截
 *       自己实现，于是编译期可以完整保留 -fstack-protector-strong：
 *         ① cmake/no-ssp-libs.specs 覆盖驱动的 *link_ssp: 段，去掉 -lssp
 *         ② 本模块提供 __stack_chk_guard / __stack_chk_fail 两个符号
 *
 * 编译器生成的检查形态（每个受保护函数）：
 *     entry: r = __stack_chk_guard; 存到栈帧尾部
 *     exit : 取出栈帧尾部的值，与 __stack_chk_guard 比较
 *            不等 → bl __stack_chk_fail（且不再正常返回）
 * 因此 __stack_chk_fail 必须是"不返回"的。
 *
 * 覆盖范围与局限（重要）：
 *   - 只覆盖「局部数组/取地址局部量越界写穿栈帧、改到返回地址或 canary」这一类。
 *   - **不覆盖「整个任务栈被打穿」**（递归、深调用链）——那才是嵌入式里更常见的栈
 *     故障。那一类要靠 -fstack-usage 的静态用量统计 + FreeRTOS
 *     uxTaskGetStackHighWaterMark() 在线余量监控，两者互补、不冲突。
 *
 * 开关：JSF_AV_STACK_PROTECTOR（由顶层 CMakeLists.txt 定义，跟随
 *       JSF_AV_STACK_PROTECTOR 选项）。未定义时本模块编译为空实现。
 */

#ifndef __BSP_STACK_GUARD_H
#define __BSP_STACK_GUARD_H

#include <stdint.h>

/**
 * @brief 栈保护失败时的钩子（weak，默认空实现）
 *
 * @note 默认实现**什么都不做**，随后 __stack_chk_fail() 会直接 NVIC_SystemReset()。
 *       app 可以覆盖它做"停在安全态"（停电机 / 关输出等）——这是分层要求：
 *       安全动作是 app 的语义，bsp 只负责"发现并上报"。
 *
 * @warning 本钩子在**调用者栈帧已损坏**的上下文里被调用，可能来自任意任务甚至
 *          中断。里面不要用大栈帧、不要长时间阻塞、不要依赖被破坏的数据。
 */
void BSP_StackGuardFailHook(void);

#endif /* __BSP_STACK_GUARD_H */

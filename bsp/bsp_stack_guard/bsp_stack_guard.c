/**
 * @file bsp_stack_guard.c
 * @brief GCC 栈保护（-fstack-protector-strong）的运行期实现
 *        详见头文件说明；本文件只负责把两个符号补齐。
 */

/* 无论开关是否打开都包含：C 语言禁止空编译单元，
   本模块关闭时整个 #ifdef 块为空，靠这条声明保住"至少一个声明"。 */
#include "bsp_stack_guard.h"

#ifdef JSF_AV_STACK_PROTECTOR

#include "main.h" /* NVIC_SystemReset()，与 bsp_adc.c 的引法一致 */

/**
 * @brief 栈帧尾部金丝雀（编译器在每个受保护函数入口读它、出口比对它）
 *
 * @note 取值"最低字节为 0"：ARM 是小端，canary 的最低地址字节就是它的最低字节。
 *       strcpy/strcat 这类逐字节写、遇 NUL 才停的溢出写到这里会被立刻截断，
 *       于是攻击者/越界代码没法"连同 canary 一起伪造"。
 *
 * @note 这是个**编译期固定值**。对"局部数组意外越界"（本工程真正要防的）已经够用：
 *       只要是写穿就会改变 canary 的值，比对即失败。真正的抗攻击 canary 需要
 *       每次上电随机（MCU 唯一 ID / RNG），但那要求在任何受保护函数执行之前完成
 *       初始化、且要考虑中断上下文，代价与收益不成比例，故未采用。
 */
uintptr_t __stack_chk_guard = (uintptr_t)0xA5C3E1u << 8;

__attribute__((weak)) void BSP_StackGuardFailHook(void)
{
    /* 默认什么都不做：由 app 覆盖以"停在安全态"（停电机 / 关输出）。 */
}

/**
 * @brief 编译器在 canary 比对失败时调用的出口
 *
 * @note 必须是"不返回"的——GCC 生成的代码在 bl 之后不再走正常返回路径。
 *       这里给的是"先让 app 做安全动作，再复位"：对运动中的机构，带着被破坏的
 *       栈继续跑比复位危险得多，所以默认动作是复位而不是停机自旋。
 */
__attribute__((noreturn)) void __stack_chk_fail(void)
{
    BSP_StackGuardFailHook();

    NVIC_SystemReset();

    /* NVIC_SystemReset 不返回；兜底防止万一被裁剪成普通函数时跑飞。 */
    for (;;)
    {
    }
}

#endif /* JSF_AV_STACK_PROTECTOR */

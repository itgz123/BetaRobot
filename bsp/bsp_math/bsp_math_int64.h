/**
 * @file bsp_math_int64.h
 * @brief 64 位整数乘除工具（高半乘法 + 魔数除法，全程零除法指令、无库调用）
 *
 * @note 32 位 ARM（Cortex-M4/M7）无 128 位整数类型（__int128 会生成
 *       __multi3 库调用），也没有硬件 64 位除法指令——编译器对
 *       `uint64_t / 常量或变量` 一律生成软件除法库 __aeabi_uldivmod（逐位
 *       循环，慢）。本模块把这些 64 位乘除换成"定点小数字因子 + 高半乘法"
 *       的等价整数实现：
 *         - BSP_Math_UMulH(a,b) = (a*b)>>64：4 次 32×32→64 umull + 进位累加；
 *         - 除以除数 d 等价于"乘以因子 magic/2^64 再取高 64 位"，
 *           magic = ceil(2^64/d)（BSP_Math_Magic 预计算一次），
 *           商 = BSP_Math_U64Div(v, d, magic)，至多一次修正。
 *       全程 umull + 移位 + 减法，零除法指令、不调用 __aeabi_uldivmod。
 *       供 bsp_format（64 位十进制/十六进制）与 bsp_dwt（us 时间戳）共用。
 *
 * 验证（host，/tmp/test_math_int64.c）：BSP_Math_UMulH / BSP_Math_U64Div /
 *   BSP_Math_U64DivMod1000 与 __uint128、/ % 参照对比，小值域全遍历 +
 *   各比特位边界窗口 + 大量随机，全部一致。
 */

#ifndef __BSP_MATH_INT64_H
#define __BSP_MATH_INT64_H

#include <stdint.h>

/*============================================
 *              高半乘法
 *============================================*/

/**
 * @brief 64 位高半乘法：BSP_Math_UMulH(a,b) = (a*b)>>64
 * @note 按 2^32 基拆成 4 次 32×32→64 umull + 进位累加：
 *       a*b = m3*2^64 + (m1+m2)*2^32 + t
 *       高 64 位 = m3 + floor((m1+m2 + (t>>32))/2^32)。
 *       无符号加法溢出即模 2^64（C 标准定义），逐项模加结果正确。
 */
static inline uint64_t BSP_Math_UMulH(uint64_t a, uint64_t b)
{
    uint32_t al = (uint32_t)a;
    uint32_t ah = (uint32_t)(a >> 32);
    uint32_t bl = (uint32_t)b;
    uint32_t bh = (uint32_t)(b >> 32);

    uint64_t t = (uint64_t)al * bl;  /* 低×低 */
    uint64_t m1 = (uint64_t)ah * bl; /* 高×低 */
    uint64_t m2 = (uint64_t)al * bh; /* 低×高 */
    uint64_t m3 = (uint64_t)ah * bh; /* 高×高 */

    uint64_t mid = (t >> 32) + (uint32_t)m1 + (uint32_t)m2;
    return m3 + (m1 >> 32) + (m2 >> 32) + (mid >> 32);
}

/*============================================
 *              魔数除法（除以任意 32 位除数）
 *============================================*/

/**
 * @brief 预计算"除以 32 位除数 d"的魔数 = ceil(2^64/d)
 * @param d 除数（32 位）；d<=1 时返回 0，调用方须特判 d<=1 走直接返回
 * @note 仅初始化阶段调用一次，避免每次除法都算。
 *       ceil(2^64/d) = UINT64_MAX/d + 1：d 不整除 2^64 时 UINT64_MAX/d =
 *       floor(2^64/d)，+1 = ceil ✓；d 整除 2^64（d 为 2 的幂）时
 *       UINT64_MAX/d = 2^64/d - 1，+1 = 2^64/d = ceil ✓。
 *       本函数含一次 64 位除法（UINT64_MAX/d），仅此一处、仅执行一次。
 */
static inline uint64_t BSP_Math_Magic(uint32_t d)
{
    if (d <= 1u)
    {
        return 0u;
    }
    return UINT64_MAX / (uint64_t)d + 1u;
}

/**
 * @brief 64 位被除数除以 32 位除数 d（返回商），魔数法，零除法指令
 * @param v     被除数（64 位）
 * @param d     除数（32 位，须与 magic 对应，d>=2）
 * @param magic 除数 d 的魔数（BSP_Math_Magic(d) 预计算）
 * @note 定点小数字因子思路：因子 = magic/2^64，商 = floor(v×因子) =
 *       umulh(v, magic)。对 d 不整除 2^64，q0 ∈ {⌊v/d⌋, ⌊v/d⌋+1}
 *       （高估 ≤ 1：q0 = ⌊v/d + (r/d)·(d-rem)/d⌋ < q + 2，见 BSP_Math_UMulH
 *       注释思路），一次修正：q0·d 溢出（umulh(q0,d)≠0）或大于 v 则回退 1。
 *       全程 umull + 移位 + 减法，无除法指令、无 __aeabi_uldivmod。
 */
static inline uint64_t BSP_Math_U64Div(uint64_t v, uint32_t d, uint64_t magic)
{
    uint64_t q = BSP_Math_UMulH(v, magic);
    uint64_t prod = q * (uint64_t)d;
    if (BSP_Math_UMulH(q, (uint64_t)d) != 0u || prod > v)
    {
        q--;
    }
    return q;
}

/*============================================
 *              魔数除法（除以常量 1000）
 *============================================*/

/**
 * @brief 64 位除以常量 1000（返回商，余数写 *rem），零除法指令
 * @note v/1000 = (v/8)/125：
 *       - v8 = v>>3（< 2^61）；商 q0 = BSP_Math_UMulH(v8, floor(2^64/125))，
 *         魔数 0x20c49ba5e353f7c 使 q0 ∈ {⌊v8/125⌋, ⌊v8/125⌋-1}（低估量
 *         = (v8/125)·(2^64 mod 125)/2^64 ≤ 0.116 < 1），一次修正：
 *         r8 = v8 - q0·125，若 r8 ≥ 125 则 q0++、r8 -= 125；
 *       - 再由 v = 8·v8 + (v&7) = 1000·q0 + (8·r8 + v&7)，且
 *         8·r8 + v&7 ≤ 8·124 + 7 = 999 恒成立，商即 q0、余数即 8·r8 + (v&7)。
 *       全程 umull + 移位 + 减法，无除法指令、无 __aeabi_uldivmod。
 */
static inline uint64_t BSP_Math_U64DivMod1000(uint64_t v, uint64_t *rem)
{
    uint64_t v8 = v >> 3;
    uint64_t q0 = BSP_Math_UMulH(v8, 0x20c49ba5e353f7cull);
    uint64_t r8 = v8 - q0 * 125ull; /* q0 ≈ v8/125，q0·125 ≤ v8 + 125 < 2^64，不溢出 */
    if (r8 >= 125ull)
    {
        q0++;
        r8 -= 125ull;
    }
    *rem = (r8 << 3) + (v & 7u); /* = 8·r8 + (v&7) ≤ 999 */
    return q0;
}

#endif /* __BSP_MATH_INT64_H */

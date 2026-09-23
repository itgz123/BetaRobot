/**
 * @file test_format.c
 * @brief PC 端 lib_format 位数/参数槽检验程序（32 位 ABI）
 *
 * @note 用法（在 tools/ 目录下）：
 *       gcc -m32 -ffreestanding -nostdlib -nostartfiles -fno-stack-protector \
 *           -DLIB_FORMAT_STANDALONE -I .. -I ../../lib_math \
 *           -o test_format test_format.c ../lib_format.c
 *       ./test_format
 *       -DLIB_FORMAT_STANDALONE 与 lib_math_trig_lut 同一惯例：PC 端不引入 app_cfg.h。
 *
 *       必须用 -m32 编译：本文件检验的是 **32 位 ABI 下参数槽宽度**问题。
 *       64 位主机上 long 本身就是 64 位，%lX 读 64 位"碰巧"是对的，测不出问题。
 *       刻意不依赖 libc（freestanding + int $0x80 直接系统调用），
 *       因此无需安装 32 位 glibc 头，任一支持 -m32 的 gcc 都能跑。
 *
 * @note 检验内容（回归点）：
 *       - %l / %ll 的参数槽消耗数。曾把单个 l 也当 64 位处理，而 arm-none-eabi 的
 *         va_arg(uint64_t) 生成 ldrd（读 8 字节、推进 8 字节），于是给 %lX 传 32 位
 *         实参时会多吃一个参数槽：err 值高半为垃圾，且同一格式串后续所有转换全部错位。
 *         现场症状：bsp_spi 报 `err=0x80065BB00000010`（真实值是 0x10，高半是下一个参数）。
 *         修法：只有 ll 置 is64，l 按 32 位（与目标 ABI 一致）。
 *       - %llu / %llX 必须仍是 64 位（bsp_log 时间戳、drv_daemon/bsp_app 的 dt 依赖）。
 *
 * @note 退出码：任一用例不符则返回失败条数（非零）。
 */

typedef unsigned int       u32;
typedef int                i32;
typedef unsigned long long u64;
typedef long long          i64;

#include "lib_format.h"

/*---- 最小运行时（不依赖 libc）----*/

static void sys_write(const char *s, u32 n)
{
    __asm__ volatile("int $0x80" ::"a"(4), "b"(1), "c"(s), "d"(n) : "memory");
}

static void sys_exit(i32 code)
{
    __asm__ volatile("int $0x80" ::"a"(1), "b"(code));
    for (;;)
    {
    }
}

static u32 slen(const char *s)
{
    u32 n = 0;
    while (s[n] != '\0')
    {
        n++;
    }
    return n;
}

static void puts_(const char *s)
{
    sys_write(s, slen(s));
}

static int seq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

/*---- 用例判定 ----*/

static int s_fails;

static void chk(const char *what, const char *got, const char *want)
{
    int ok = (seq(got, want) == 0);

    if (!ok)
    {
        s_fails++;
    }
    puts_(ok ? "PASS  " : "FAIL  ");
    puts_(what);
    if (!ok)
    {
        puts_("\n        got : ");
        puts_(got);
        puts_("\n        want: ");
        puts_(want);
    }
    puts_("\n");
}

/* 用格式串字面量的精确长度调用（LibFormatEx 需显式 fmt_len；写错长度会污染判定） */
#define FMT_CALL(buf, fmt, ...) LibFormatEx((buf), sizeof(buf), (fmt), sizeof(fmt) - 1, ##__VA_ARGS__)

void _start(void)
{
    char b[160];
    u32 err = 0x10; /* 真实 ErrorCode：HAL_SPI_ERROR_DMA */

    /* ---- 回归点：%lX 只应吃掉一个 32 位参数槽，后续转换不得错位 ---- */

    /* bsp_spi.c:215 / bsp_usart.c:344 那一类格式串 */
    FMT_CALL(b, "code=0x%lX (MODF:%d OVR:%d FRE:%d DMA:%d)", err, 0, 1, 0, 1);
    chk("%lX 后跟 4 个 %d", b, "code=0x10 (MODF:0 OVR:1 FRE:0 DMA:1)");

    /* bsp_i2c.c:372 那一类格式串 */
    FMT_CALL(b, "code=0x%lX (BERR:%d ARLO:%d AF:%d OVR:%d DMA:%d TIMEOUT:%d)",
             err, 0, 0, 1, 0, 0, 1);
    chk("%lX 后跟 6 个 %d", b, "code=0x10 (BERR:0 ARLO:0 AF:1 OVR:0 DMA:0 TIMEOUT:1)");

    /* bsp_i2c.c:227 那一类格式串。
     * 注意期望值里 State=5 打印为 "0x5" 而非 "0x05"：lib_format 的宽度是
     * **最大**长度（超了从高位截断），不是最小宽度，%02X 不补零——见 lib_format.h。
     * 这里要验的是 %lX 之后的 lock=%d 没错位，不是补零。 */
    FMT_CALL(b, "state=0x%02X, err=0x%lX, lock=%d", 5u, err, 1);
    chk("%lX 后跟 1 个 %d", b, "state=0x5, err=0x10, lock=1");

    /* bsp_can 的 filter ID / Tx timeout 那一类格式串 */
    FMT_CALL(b, "id0=0x%lX id1=0x%lX", (unsigned long)0x201ul, (unsigned long)0x7FFul);
    chk("两个 %lX 并存", b, "id0=0x201 id1=0x7FF");

    /* bsp_spi.c 的启动失败日志（两个 %s 连用 + 5 个 %d + 1 个 %lX）。
     * 两个 %s 必须按顺序吃掉两个参数槽：错位的话后面 5 个 %d 与 %lX 全部对不上，
     * 会把 DMA 流状态打成别的字段值，比不打印更误导（本次 A.6 排查就靠这几个数）。 */
    FMT_CALL(b, "SPI %s %s (spi_e=%d, mode=%d, spi=%d, tx_dma=%d, rx_dma=%d, err=0x%lX)!",
             "transmit/receive", "start failed", 0, 2, 2, 3, 1, err);
    chk("SPI 启动失败日志（%s×2 + %d×5 + %lX）", b,
        "SPI transmit/receive start failed (spi_e=0, mode=2, spi=2, tx_dma=3, rx_dma=1, err=0x10)!");

    /* BLOCK 分支 + 该口没有 DMA 的现场（tx_state/rx_state 传 -1）。
     * 注意 `%d` 打的是十进制：日志里"没有这一路 DMA"显示为 -1（结构体里存的
     * 0xFF 只给调试器看，不进日志）。 */
    FMT_CALL(b, "SPI %s %s (spi_e=%d, mode=%d, spi=%d, tx_dma=%d, rx_dma=%d, err=0x%lX)!",
             "transmit", "failed", 0, 0, 1, -1, -1, err);
    chk("SPI BLOCK 失败日志（phase=failed / 无 DMA 口）", b,
        "SPI transmit failed (spi_e=0, mode=0, spi=1, tx_dma=-1, rx_dma=-1, err=0x10)!");

    /* ---- 回归点：%ld 按 32 位、%lld 按 64 位 ---- */

    FMT_CALL(b, "a=%ld b=%lld", (i32)-5, (i64)-1234567890123ll);
    chk("%ld 32 位 / %lld 64 位", b, "a=-5 b=-1234567890123");

    /* ---- 兼容性回归：ll 仍必须按 64 位读 ---- */

    FMT_CALL(b, "ts=%llu", (u64)1234567890123ull);
    chk("%llu 仍为 64 位", b, "ts=1234567890123");

    FMT_CALL(b, "h=%llX", (u64)0xAABBCCDDEEFFull);
    chk("%llX 仍为 64 位", b, "h=AABBCCDDEEFF");

    /* 无修饰符仍按 32 位（不受本次修改影响） */
    FMT_CALL(b, "x=%X", (u32)0xDEADBEEF);
    chk("%X 32 位", b, "x=DEADBEEF");

    puts_(s_fails ? "\n结果: 有失败\n" : "\n结果: 全部通过\n");
    sys_exit(s_fails);
}

/**
 * @file lib_format.c
 * @brief lib 层快速格式化实现（零除法整数转换）
 *
 * @note 支持整数 %d %u %x %X 与字符串 %s，均可带固定宽度（见 lib_format.h）。
 *       整数默认 32 位（int/uint32_t，8/16 位参数经默认整数提升可用）；
 *       带 ll 长度修饰符（%lld/%llu/%llx 等）按 64 位（int64_t/uint64_t）读取；
 *       单个 l 按 32 位（与目标 ABI 一致，见下方解析处的说明）。
 *       对外只暴露 LibFormatEx 一个函数（LIBFORMAT 宏内部调用）。
 *       十进制整数转换全程零除法指令：
 *       - 千分位分块查表：拆成最多 7 组 3 位数，每组各查一次
 *         flash 表 LIB_DEC3 拼出字符串，O(1) 恒定时间
 *       - 32 位拆组用 /1000 与 %1000：除数为常量时编译器生成魔数乘法+移位
 *         （umull/lsr/mls），不调用 __aeabi_uidiv、无 udiv 除法指令
 *       - 64 位拆组用 Lib_Math_U64DivMod1000（lib_math/lib_math_int64.h）：
 *         手写 64 位高半乘法（4 次 umull + 进位累加）+ 魔数 floor(2^64/125)，
 *         同样零除法指令、不调用 __aeabi_uldivmod
 *       十六进制：每 4 bit 查 16 字符表 + 右移，天然零除法。
 *       负数：判符号位后用 0u - (uint32_t)val 取绝对值，同样零除法。
 */

#include "lib_format.h"
#include "lib_math_int64.h"

/* 配置入口（同 lib_math_trig_lut 的惯例）：
 *   固件编译——include app_cfg.h（lib 层可包含的唯一 app 文件）取 LIB_FORMAT_USED；
 *   PC 独立检验（tools/test_format.c）——定义 LIB_FORMAT_STANDALONE 跳过 app_cfg.h
 *   （PC 端不引入工程配置）。 */
#ifdef LIB_FORMAT_STANDALONE
#define LIB_FORMAT_USED /* PC 独立检验：不引入 app_cfg.h，直接启用 */
#else
#include "app_cfg.h"
#endif

#ifdef LIB_FORMAT_USED

/*============================================
 *              查表常量
 *============================================*/

/* 十进制 3 位定宽表：LIB_DEC3[val] = {'0','0','0'}..{'9','9','9'}（tools/gen_dec_tbl.py 生成） */
static const char LIB_DEC3[1000][3] = {
#include "lib_format_dec3.inc"
}; // 2位或者3位表最合适，不缺flash，使用3位

/* 十六进制字符表（下标 = 0..15） */
static const char LIB_HEX_LO[] = "0123456789abcdef";
static const char LIB_HEX_UP[] = "0123456789ABCDEF";

/*============================================
 *              内部工具
 *============================================*/

/**
 * @brief uint32 转十进制（零除法指令），写入 buf，返回字符数
 * @param buf 至少 10 字节
 * @note 千分位分块查表：从低位每 3 位一组（%1000），存到临时数组；
 *       再按高位在前逐组查 LIB_DEC3 输出。除数为常量 1000，编译器
 *       生成魔数乘法+移位（umull/lsr/mls），无除法指令、无 __aeabi_uidiv。
 */
static int BSPU32ToDec(uint32_t val, char *buf)
{
    uint32_t g[4]; /* 千分组（低位在前），uint32 < 1000^4 */
    int n = 0;

    while (val >= 1000u)
    {
        g[n++] = val % 1000u;
        val /= 1000u;
    }
    g[n++] = val;

    int out = 0;
    for (int i = n - 1; i >= 0; i--)
    {
        const char *s = LIB_DEC3[g[i]];
        if (i == n - 1)
        {
            /* 最高组：剥离前导零（val==0 时保留一个 '0'） */
            int k = 0;
            while (k < 2 && s[k] == '0')
            {
                k++;
            }
            while (k < 3)
            {
                buf[out++] = s[k++];
            }
        }
        else
        {
            /* 其余组：3 位定宽直补（含中间零） */
            buf[out++] = s[0];
            buf[out++] = s[1];
            buf[out++] = s[2];
        }
    }
    return out;
}

/**
 * @brief uint32 转十六进制（位运算，零除法），写入 buf，返回字符数
 */
static int BSPU32ToHex(uint32_t val, char *buf, int upper)
{
    const char *tbl = upper ? LIB_HEX_UP : LIB_HEX_LO;
    char tmp[8];
    int i = 0;

    do
    {
        tmp[i++] = tbl[val & 0xFu];
        val >>= 4;
    } while (val != 0);

    /* tmp 是逆序，反转 */
    for (int j = 0; j < i; j++)
    {
        buf[j] = tmp[i - 1 - j];
    }
    return i;
}

/**
 * @brief uint64 转十进制（零除法指令），写入 buf，返回字符数
 * @param buf 至少 21 字节
 * @note 思路同 BSPU32ToDec：千分位分块查表，uint64 < 1000^7 ≈ 1e21，最多 7 组；
 *       拆组用 Lib_Math_U64DivMod1000（lib_math_int64.h 手写 64 位魔数除法，
 *       基于高半乘法 Lib_Math_UMulH），零除法指令、无库调用。
 */
static int BSPU64ToDec(uint64_t val, char *buf)
{
    uint64_t g[7]; /* 千分组（低位在前），uint64 < 1000^7 */
    int n = 0;

    while (val >= 1000u)
    {
        val = Lib_Math_U64DivMod1000(val, &g[n++]); /* 商写回 val，余数入 g */
    }
    g[n++] = val;

    int out = 0;
    for (int i = n - 1; i >= 0; i--)
    {
        const char *s = LIB_DEC3[(size_t)g[i]];
        if (i == n - 1)
        {
            /* 最高组：剥离前导零（val==0 时保留一个 '0'） */
            int k = 0;
            while (k < 2 && s[k] == '0')
            {
                k++;
            }
            while (k < 3)
            {
                buf[out++] = s[k++];
            }
        }
        else
        {
            /* 其余组：3 位定宽直补（含中间零） */
            buf[out++] = s[0];
            buf[out++] = s[1];
            buf[out++] = s[2];
        }
    }
    return out;
}

/**
 * @brief uint64 转十六进制（位运算，零除法），写入 buf，返回字符数
 * @param buf 至少 16 字节
 */
static int BSPU64ToHex(uint64_t val, char *buf, int upper)
{
    const char *tbl = upper ? LIB_HEX_UP : LIB_HEX_LO;
    char tmp[16];
    int i = 0;

    do
    {
        tmp[i++] = tbl[val & 0xFu];
        val >>= 4;
    } while (val != 0);

    /* tmp 是逆序，反转 */
    for (int j = 0; j < i; j++)
    {
        buf[j] = tmp[i - 1 - j];
    }
    return i;
}

/*============================================
 *              核心实现
 *============================================*/

/**
 * @brief 核心格式化（va_list 版本，仅 LibFormatEx 内部调用）
 */
static int BSPFmtV(char *out, size_t cap, const char *fmt, size_t fmt_len, va_list args)
{
    size_t pos = 0; /* 本应写入的字符数（不含终止符），截断时仍累加，供调用者判断溢出 */
    const char *f = fmt;
    const char *flim;

    if (out == NULL || cap == 0 || fmt == NULL)
    {
        return 0;
    }

    flim = fmt + fmt_len;

    while (f < flim)
    {
        char c = *f++;
        if (c != '%')
        {
            if (pos + 1 < cap)
            {
                out[pos] = c;
            }
            pos++;
            continue;
        }

        /* ---- 解析：宽度（可选） + 长度修饰符（l/ll 可选） + 转换说明 ---- */
        int width = 0;
        while (f < flim && *f >= '0' && *f <= '9')
        {
            width = width * 10 + (*f - '0');
            f++;
        }
        /* 长度修饰符（按 C 标准语义，与目标 ABI 一致）：
         *   ll → 64 位（int64_t/uint64_t，对应 long long）
         *   l  → 32 位（int32_t/uint32_t，本工程 arm-none-eabi 上 int/long 均为 32 位）
         * 其余修饰符(h/z/...)不识别，按字面输出。
         * 注意：曾把 l 也当 64 位，导致 %lX 在 32 位实参上多读一个参数槽——
         * arm-none-eabi 的 va_arg(uint64_t) 生成 ldrd（读 8 字节、推进 8 字节），
         * 结果是 err 值高半为垃圾、且同一格式串后续所有转换全部错位。 */
        int is64 = 0;
        if (f < flim && *f == 'l')
        {
            f++;
            if (f < flim && *f == 'l')
            {
                is64 = 1; /* 只有 ll 才是 64 位 */
                f++;
            }
        }
        if (f >= flim)
        {
            /* 格式串以单个 '%' 结尾（如 "duty=100%"）：按字面输出这个 %，
             * 别静默吞掉一个字符（正文里的 % 与 %% 一样常见）。 */
            if (pos + 1 < cap)
            {
                out[pos] = '%';
            }
            pos++;
            break;
        }
        char spec = *f++;

        char numbuf[21]; /* uint64 十进制最长 20 位（+ 负号在外部处理） */
        int numlen = 0;
        int neg = 0;

        switch (spec)
        {
        case 'd':
        {
            if (is64)
            {
                int64_t v = va_arg(args, int64_t);
                if (v < 0)
                {
                    neg = 1;
                    v = (int64_t)(0ull - (uint64_t)v); /* 取绝对值，正确处理 INT64_MIN */
                }
                numlen = BSPU64ToDec((uint64_t)v, numbuf);
            }
            else
            {
                int32_t v = va_arg(args, int32_t);
                if (v < 0)
                {
                    neg = 1;
                    v = (int32_t)(0u - (uint32_t)v); /* 取绝对值，正确处理 INT32_MIN */
                }
                numlen = BSPU32ToDec((uint32_t)v, numbuf);
            }
            break;
        }
        case 'u':
            if (is64)
            {
                numlen = BSPU64ToDec(va_arg(args, uint64_t), numbuf);
            }
            else
            {
                numlen = BSPU32ToDec(va_arg(args, uint32_t), numbuf);
            }
            break;
        case 'x':
        case 'X':
            if (is64)
            {
                numlen = BSPU64ToHex(va_arg(args, uint64_t), numbuf, (spec == 'X'));
            }
            else
            {
                numlen = BSPU32ToHex(va_arg(args, uint32_t), numbuf, (spec == 'X'));
            }
            break;
        case 's':
        {
            const char *s = va_arg(args, const char *);
            int n = 0;

            if (s == NULL)
            {
                s = "(null)"; /* NULL 安全 */
            }
            /* 宽度语义同数字：超过 width 保留前 width 个，0 表示无限制 */
            while (*s != '\0' && (width == 0 || n < width))
            {
                if (pos + 1 < cap)
                {
                    out[pos] = *s;
                }
                pos++;
                n++;
                s++;
            }
            continue; /* 不走数字输出段 */
        }
        case '%':
            /* 字面一个 %（与 C 标准一致）。必须独立成 case：下面 default 会消费一个
             * 参数槽，%% 落到那里就会凭空吃掉调用方的一个实参。 */
            if (pos + 1 < cap)
            {
                out[pos] = '%';
            }
            pos++;
            continue;
        default:
            /* 未支持的转换说明：**原样输出 '%' + 该说明符字符**（容错，不崩），
             * 这样日志里能直接看出调用方写了什么不支持的说明符，而不是凭空少一段。
             *
             * 另一半要决定的是**参数槽要不要消费**：只输出不消费的话，本次之后同一
             * 格式串里**所有**转换都会读到别人的实参，错位且不报错 —— bsp_adc 的
             * "handle=0x%p, channel=%lu" 就是这么把 handle 指针当成 channel 打出来的。
             * 判据取"spec 是不是字母"：
             *   是字母（%p %c %i %f %zu...) —— 几乎必然是调用方想写的转换说明，
             *     而且他为它传了实参 → 消费一个槽（按 is64 猜 32/64 位：这类说明符
             *     绝大多数是整数/指针类，一个槽就对得上；已知例外是浮点，double 按
             *     ABI 占两个槽，%f 之后仍会错位，本仓库的日志不使用 %f）；
             *   不是字母（%-5d 这种 flags 写法、或消息正文里的 "100% 占空比"）——
             *     这一档在字符层面分不开：'-'、' '、'+'、'#' 在 C 里是合法 flags，
             *     在消息正文里就是标点。统一按"正文里的 %"处理：不消费参数、原文照抄。
             *     对正文那半是对的（"100% 占空比" 的 % 本就不是转换说明）；代价是
             *     flags 写法会**整体错位**（它同样不会被当成一次转换，后续每个实参前移
             *     一格），不只是"宽度不生效"。本仓库不使用这种写法，真要用由
             *     lib/lib_format/tools/scan_fmt_specs.py 扫出来；动态宽度 %*d（要两个槽）
             *     与精度 .N 同理不处理。 */
            if ((spec >= 'a' && spec <= 'z') || (spec >= 'A' && spec <= 'Z'))
            {
                if (is64)
                {
                    (void)va_arg(args, uint64_t);
                }
                else
                {
                    (void)va_arg(args, uint32_t);
                }
            }
            if (pos + 1 < cap)
            {
                out[pos] = '%';
            }
            pos++;
            if (pos + 1 < cap)
            {
                out[pos] = spec;
            }
            pos++;
            continue;
        }

        /* ---- 输出：宽度作为最大长度，长了截断、短了不补 ----
         * 整个数字串（含负号）超过 width 个字符时，从高位保留 width 个；
         * 不超过则原样输出（不补空格/零）。width == 0 表示无限制。 */
        int outlen = numlen + (neg ? 1 : 0);
        if (width > 0 && outlen > width)
        {
            outlen = width;
        }

        if (neg && outlen > 0)
        {
            if (pos + 1 < cap)
            {
                out[pos] = '-';
            }
            pos++;
            outlen--;
        }

        for (int k = 0; k < numlen && k < outlen; k++)
        {
            if (pos + 1 < cap)
            {
                out[pos] = numbuf[k];
            }
            pos++;
        }
    }

    out[(pos < cap) ? pos : (cap - 1)] = '\0';
    return (int)pos;
}

int LibFormatV(char *out, size_t cap, const char *fmt, size_t fmt_len, va_list args)
{
    return BSPFmtV(out, cap, fmt, fmt_len, args);
}

int LibFormatEx(char *out, size_t cap, const char *fmt, size_t fmt_len, ...)
{
    va_list args;
    int n;

    va_start(args, fmt_len);
    n = LibFormatV(out, cap, fmt, fmt_len, args);
    va_end(args);
    return n;
}

#endif /* LIB_FORMAT_USED */

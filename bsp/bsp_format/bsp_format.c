/**
 * @file bsp_format.c
 * @brief BSP 层快速格式化实现（零除法整数转换）
 *
 * @note 仅支持整数转换说明 %d %u %x %X 与固定宽度（见 bsp_format.h）。
 *       对外只暴露 BSPFormatEx 一个函数（BSPFORMAT 宏内部调用）。
 *       十进制整数转换全程零除法指令：
 *       - 千分位分块查表：uint32 拆成最多 4 组 3 位数，每组各查一次
 *         flash 表 BSP_DEC3 拼出字符串，O(1) 恒定时间
 *       - 拆组用 /1000 与 %1000：除数为常量时编译器生成魔数乘法+移位
 *         （umull/lsr/mls），不调用 __aeabi_uidiv、无 udiv 除法指令
 *       十六进制：每 4 bit 查 16 字符表 + 右移，天然零除法。
 *       负数：判符号位后用 0u - (uint32_t)val 取绝对值，同样零除法。
 */

#include "bsp_format.h"

/*============================================
 *              查表常量
 *============================================*/

/* 十进制 3 位定宽表：BSP_DEC3[val] = {'0','0','0'}..{'9','9','9'}（scripts/gen_dec_tbl.py 生成） */
static const char BSP_DEC3[1000][3] = {
#include "bsp_format_dec3.inc"
}; // 2位或者3位表最合适，不缺flash，使用3位

/* 十六进制字符表（下标 = 0..15） */
static const char BSP_HEX_LO[] = "0123456789abcdef";
static const char BSP_HEX_UP[] = "0123456789ABCDEF";

/*============================================
 *              内部工具
 *============================================*/

/**
 * @brief uint32 转十进制（零除法指令），写入 buf，返回字符数
 * @param buf 至少 10 字节
 * @note 千分位分块查表：从低位每 3 位一组（%1000），存到临时数组；
 *       再按高位在前逐组查 BSP_DEC3 输出。除数为常量 1000，编译器
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
        const char *s = BSP_DEC3[g[i]];
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
    const char *tbl = upper ? BSP_HEX_UP : BSP_HEX_LO;
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

/*============================================
 *              核心实现
 *============================================*/

/**
 * @brief 核心格式化（va_list 版本，仅 BSPFormatEx 内部调用）
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

        /* ---- 解析：宽度（可选） + 转换说明 ---- */
        int width = 0;
        while (f < flim && *f >= '0' && *f <= '9')
        {
            width = width * 10 + (*f - '0');
            f++;
        }
        if (f >= flim)
        {
            break;
        }
        char spec = *f++;

        char numbuf[10];
        int numlen = 0;
        int neg = 0;

        switch (spec)
        {
        case 'd':
        {
            int32_t v = va_arg(args, int32_t);
            if (v < 0)
            {
                neg = 1;
                v = (int32_t)(0u - (uint32_t)v); /* 取绝对值，正确处理 INT32_MIN */
            }
            numlen = BSPU32ToDec((uint32_t)v, numbuf);
            break;
        }
        case 'u':
            numlen = BSPU32ToDec(va_arg(args, uint32_t), numbuf);
            break;
        case 'x':
        case 'X':
            numlen = BSPU32ToHex(va_arg(args, uint32_t), numbuf, (spec == 'X'));
            break;
        default:
            /* 未支持的转换说明：按字面输出该字符（容错，不崩） */
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

int BSPFormatEx(char *out, size_t cap, const char *fmt, size_t fmt_len, ...)
{
    va_list args;
    int n;

    va_start(args, fmt_len);
    n = BSPFmtV(out, cap, fmt, fmt_len, args);
    va_end(args);
    return n;
}

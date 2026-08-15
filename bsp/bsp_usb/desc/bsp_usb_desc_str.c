/**
 * @file bsp_usb_desc_str.c
 * @brief 字符串描述符：UTF-16LE 按需生成（对照 XRUSB DescriptorStrings::ToUTF16LE）
 *
 *   idx 0 = LANGID 表（0x0409 English US）
 *   idx 1/2/3 = Manufacturer / Product / Serial（"BetaRobot"）
 *   idx >= USB_STR_INTERFACE_BASE（4）= 接口字符串，由 core 在类 bind 后经
 *     USB_DescAddInterfaceString 注册到扁平表，运行时 UTF-8 → UTF-16LE 生成
 *     （对照 XRUSB GenerateInterfaceString 的扁平源字符串表 + to_utf16le 完整解码）。
 */

#include "bsp_usb_desc.h"
#include "bsp_usb_types.h"
#include "stddef.h" /* NULL */

/* 字符串描述符生成缓冲（[bLength][type][UTF-16LE...]；接口字符串最长约 63 字符） */
#define USB_STR_BUF_LEN 128

static uint8_t s_buf[USB_STR_BUF_LEN];

/* 接口字符串扁平表：索引 >= USB_STR_INTERFACE_BASE 时按序查表（对照 XRUSB
 * interface_strings_[]，core 在 USB_CoreRegisterInterfaceStrings 填充） */
static const char *s_itf_strings[USB_IF_STR_MAX];
static uint8_t s_itf_string_count;

uint8_t USB_DescAddInterfaceString(const char *str)
{
    if (str == NULL || str[0] == '\0')
    {
        return 0;
    }
    if (s_itf_string_count >= USB_IF_STR_MAX)
    {
        return 0; /* 表满 */
    }
    s_itf_strings[s_itf_string_count] = str;
    return (uint8_t)(USB_STR_INTERFACE_BASE + s_itf_string_count++);
}

void USB_DescClearInterfaceStrings(void)
{
    s_itf_string_count = 0;
}

/* UTF-8 → UTF-16LE 数据长度（字节）。与 ToUTF16LE 写入字节数严格一致：
 * 1/2/3 字节 UTF-8 序列各产 2 字节；4 字节序列（码点 > U+FFFF）不支持，跳过
 * （对照 XRUSB calc_utf16le_len_runtime） */
static uint16_t Utf16LELen(const char *str)
{
    uint16_t n = 0;
    const uint8_t *s = (const uint8_t *)str;
    while (*s != 0)
    {
        if (*s < 0x80)
        {
            n += 2;
            s += 1;
        }
        else if ((*s & 0xE0) == 0xC0)
        {
            n += 2;
            s += 2;
        }
        else if ((*s & 0xF0) == 0xE0)
        {
            n += 2;
            s += 3;
        }
        else
        {
            s += 4; /* 4 字节或非法序列：跳过 */
        }
    }
    return n;
}

/* UTF-8 字符串转 UTF-16LE（完整解码 1/2/3 字节码点；4 字节/非法序列跳过，
 * 对照 XRUSB to_utf16le）。返回写末尾指针 */
static uint8_t *ToUTF16LE(const char *str, uint8_t *out)
{
    const uint8_t *s = (const uint8_t *)str;
    while (*s != 0)
    {
        uint32_t codepoint;
        if (*s < 0x80)
        {
            codepoint = *s++;
        }
        else if ((*s & 0xE0) == 0xC0)
        {
            codepoint = ((uint32_t)(*s & 0x1F)) << 6;
            s++;
            codepoint |= (*s & 0x3F);
            s++;
        }
        else if ((*s & 0xF0) == 0xE0)
        {
            codepoint = ((uint32_t)(*s & 0x0F)) << 12;
            s++;
            codepoint |= ((uint32_t)(*s & 0x3F)) << 6;
            s++;
            codepoint |= (*s & 0x3F);
            s++;
        }
        else
        {
            s++; /* 4 字节首字节 / 非法字节：跳过（后续 continuation 逐个按非法跳过） */
            continue;
        }
        *out++ = (uint8_t)(codepoint & 0xFF);        /* UTF-16LE 低字节 */
        *out++ = (uint8_t)((codepoint >> 8) & 0xFF); /* 高字节 */
    }
    return out;
}

const uint8_t *USB_DescGetString(USBStrIdx_e idx, uint16_t *len)
{
    if (idx == USB_STR_LANGID)
    {
        /* bLength=4, type=3, langID[0]=0x0409 */
        static const uint8_t s_lang[4] = {4, USB_DESC_STRING, 0x09, 0x04};
        if (len != NULL)
        {
            *len = 4;
        }
        return s_lang;
    }

    const char *str;
    if (idx >= USB_STR_INTERFACE_BASE)
    {
        /* 接口字符串：查扁平表（对照 XRUSB GetStringDescriptor 的
         * string_index > SERIAL_NUMBER_STRING 分支） */
        uint8_t extra = (uint8_t)(idx - USB_STR_INTERFACE_BASE);
        if (extra >= s_itf_string_count)
        {
            return NULL;
        }
        str = s_itf_strings[extra];
    }
    else
    {
        switch (idx)
        {
        case USB_STR_MANUFACTURER:
            str = "BetaRobot";
            break;
        case USB_STR_PRODUCT:
            str = "BetaRobot";
            break;
        case USB_STR_SERIAL:
            str = "00000001";
            break;
        default:
            return NULL;
        }
    }

    /* 超缓冲防护：bLength 为单字节，且生成缓冲有限 */
    if (Utf16LELen(str) > (USB_STR_BUF_LEN - 2))
    {
        return NULL;
    }

    uint8_t *p = s_buf + 2;
    uint8_t *end = ToUTF16LE(str, p);
    uint16_t data_len = (uint16_t)(end - p);

    s_buf[0] = (uint8_t)(2 + data_len);
    s_buf[1] = USB_DESC_STRING;

    if (len != NULL)
    {
        *len = (uint16_t)(2 + data_len);
    }
    return s_buf;
}

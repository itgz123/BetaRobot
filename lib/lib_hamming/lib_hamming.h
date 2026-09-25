/**
 * @file lib_hamming.h
 * @brief lib层汉明码：标准 (n=2^m-1, k=n-m) 族，系统式（数据在前、校验在后），任意 bit 长度
 *
 * 设计（三层，从底到高）：
 *   1. 单块核心 LIB_Hamming_EncodeBlock / LIB_Hamming_DecodeBlock：把一块 k_s 位数据
 *      编成 k_s+m 位码字（前 k_s 位数据、后 m 位校验），并可由 syndrome 反查出错位置。
 *   2. 缩短：k_s < 满码数据位 k 即缩短码 —— 系统式构造下"缩短"只是少用 H 矩阵的后几列，
 *      标准满码就是 k_s == k 的特例，因此标准码与缩短码共用同一套块函数。
 *   3. 任意 bit 长度：数据按 k_s 位分块，每块独立编码，码字首尾相接；末块不足 k_s 位补零，
 *      解码按原始 data_bits 截断输出。整帧不超过 k 位时用 LIB_Hamming_Fit 取 k_s = 帧长，
 *      即整帧一个校验组（comm 首选形态）。
 *
 * 位序：数据与码字均为 MSB-first 位缓冲（uint8_t[]），长度以 bit 计，允许非字节对齐。
 *
 * @note 标准码只有单纠错（SEC）能力：检出双错不可靠（双错多被误纠成第三位），
 *       需要"纠 1 检 2"请用扩展缩短码（lib_hamming_ext.h，SECDED）。
 * @note 每块独立纠错：跨越两块的突发多位错不保证可纠；补零位是真实发送位，
 *       会参与纠错，仅在输出时截断丢弃。
 * @see lib_hamming_ext.h
 */

#ifndef __LIB_HAMMING_H
#define __LIB_HAMMING_H

#include <stdint.h>

/*============================================
 *                 类型定义
 *============================================*/

/* 校验位数上限：syndrome 用 uint8_t 承载（列值 ≤ 2^m-1 ≤ 127），且单块位缓冲 16 B 够用 */
#define LIB_HAMMING_M_MAX 7u

/**
 * @brief 汉明码参数
 *
 *   - m    校验位数，2..7（码字位置列值 ≤ 127，syndrome 用 uint8_t 承载）；
 *          满码 n = 2^m - 1，k = n - m。
 *   - k_s  每块数据位数，1..2^m-1-m；= k 为标准满码，< k 即缩短码。
 */
typedef struct
{
    uint8_t m;    /* 校验位数 2..7 */
    uint16_t k_s; /* 每块数据位数 1..2^m-1-m */
} LIB_Hamming_Cfg_t;

/**
 * @brief 编解码返回状态
 */
typedef enum
{
    LIB_HAMMING_OK = 0,         /* 无错 */
    LIB_HAMMING_CORRECTED = 1,  /* 检出并纠正单 bit 错 */
    LIB_HAMMING_DETECTED = 2,   /* 检出不可纠错（≥2 位错或落在缩短掉的列），数据未纠 */
    LIB_HAMMING_BAD_ARG = -1,   /* 参数非法（指针为空、m/k_s 越界、长度不匹配） */
    LIB_HAMMING_BUF_SMALL = -2, /* 输出缓冲不足（未写入任何数据） */
} LIB_Hamming_Status_t;

/**
 * @brief 整段编解码统计（缓冲层解码输出）
 */
typedef struct
{
    uint32_t blocks;       /* 总块数 */
    uint32_t corrected;    /* 纠正了单 bit 错的块数 */
    uint32_t detected;     /* 检出不可纠错的块数 */
    int32_t first_err_pos; /* 首个纠错位的绝对 bit 下标（码字内）；-1 = 无 */
} LIB_Hamming_Stat_t;

/*============================================
 *        编译期参数计算（宏，与运行期函数同一定义）
 *============================================*/

/**
 * 与 LIB_Hamming_Fit / LIB_Hamming_RequiredBits 完全同一定义的宏版本：把"分几块、每块多少位、
 * 码字共几位"全部变成整数常量表达式，供上层在编译期定缓冲大小与帧长（如 comm 协议的
 * PROTO_xxx_OVERHEAD 宏）。运行期函数仍是唯一权威，宏只用于编译期排布，二者由
 * tools/test_hamming.c 逐参数比对。
 *
 * @example
 *   // 一帧 payload_bits 位数据、m=7、扩展码，编译期算出码字字节数：
 *   #define CODE_BYTES LIB_HAMMING_CODE_BYTES(payload_bits, LIB_HAMMING_FIT_KS(payload_bits, 7), 7, 1)
 */

/* 满码数据位数 k = 2^m - 1 - m（m=7 → 120） */
#define LIB_HAMMING_FULL_K(m) ((uint32_t)((1u << (m)) - 1u - (m)))

/* 单块数据位数 k_s = min(data_bits, k)（与 LIB_Hamming_Fit 同一定义） */
#define LIB_HAMMING_FIT_KS(data_bits, m)                                                                               \
    (((data_bits) < LIB_HAMMING_FULL_K(m)) ? (uint32_t)(data_bits) : LIB_HAMMING_FULL_K(m))

/* 块数 B = ceil(data_bits / k_s) */
#define LIB_HAMMING_BLOCK_CNT(data_bits, k_s) (((uint32_t)(data_bits) + (k_s) - 1u) / (k_s))

/* 码字总位数 = B * (k_s + m + ext)，ext 非 0 时每块多 1 位总校验位 */
#define LIB_HAMMING_CODE_BITS(data_bits, k_s, m, ext)                                                                  \
    (LIB_HAMMING_BLOCK_CNT((data_bits), (k_s)) * ((uint32_t)(k_s) + (m) + ((ext) ? 1u : 0u)))

/* 码字总字节数 = ceil(码字总位数 / 8)，取整后尾部空位由编码函数清零 */
#define LIB_HAMMING_CODE_BYTES(data_bits, k_s, m, ext)                                                                 \
    ((LIB_HAMMING_CODE_BITS((data_bits), (k_s), (m), (ext)) + 7u) / 8u)

/**
 * @brief 编译期校验码参数合法（在文件作用域展开为 _Static_assert）
 * @param m 校验位数（2..LIB_HAMMING_M_MAX）
 * @param k_s 每块数据位数（1..k）
 * @note 用法：LIB_HAMMING_CHECK_CFG(m, k_s);
 */
#define LIB_HAMMING_CHECK_CFG(m, k_s)                                                                                  \
    _Static_assert((m) >= 2u && (m) <= LIB_HAMMING_M_MAX && (k_s) >= 1u &&                                             \
                       (uint32_t)(k_s) <= (uint32_t)((1u << (m)) - 1u - (m)),                                          \
                   "lib_hamming: m must be 2..7 and k_s must be 1..2^m-1-m")

/*---- 字节块编译期规划（数据按整字节收发的场景，如 comm 帧）----
 * 让 k_s 取整字节：满块数据字节数 S = k/8（m=7 → 15），块数 B = ceil(P/S) 取最少，
 * 再把 P 在 B 块间均分得每块 s = ceil(P/B) 字节 ⇒ 码字总长最小。
 * 例（m=7）：P=13 → B=1,s=13；P=16 → B=2,s=8；P=48 → B=4,s=12。
 * @note 仅当 k 为 8 的倍数时成立（m=7）。 */

/* 满块数据字节数 S = k/8 */
#define LIB_HAMMING_BYTE_FULL_BLK(m) (LIB_HAMMING_FULL_K(m) / 8u)

/* 块数 B = ceil(P / S)（最少块数；P 须 ≥ 1） */
#define LIB_HAMMING_BYTE_BLKS(payload_bytes, m)                                                                        \
    (((uint32_t)(payload_bytes) + LIB_HAMMING_BYTE_FULL_BLK(m) - 1u) / LIB_HAMMING_BYTE_FULL_BLK(m))

/* 每块数据字节数 s = ceil(P / B)（≤ S ⇒ k_s = 8s ≤ k，仍是合法缩短码） */
#define LIB_HAMMING_BYTE_BLK_DATA(payload_bytes, m)                                                                    \
    (((uint32_t)(payload_bytes) + LIB_HAMMING_BYTE_BLKS(payload_bytes, m) - 1u) /                                      \
     LIB_HAMMING_BYTE_BLKS(payload_bytes, m))

/*============================================
 *        位缓冲读写（MSB-first，内部共用）
 *============================================*/

/**
 * @brief 读位缓冲第 idx 位（MSB-first）
 * @param buf 位缓冲
 * @param idx 位下标（0 起）
 * @return 该位值 0/1
 */
static inline uint8_t LIB_Hamming_BitGet(const uint8_t *buf, uint32_t idx)
{
    return (uint8_t)((buf[idx >> 3] >> (7u - (idx & 7u))) & 0x01u);
}

/**
 * @brief 写位缓冲第 idx 位（MSB-first）
 * @param buf 位缓冲
 * @param idx 位下标（0 起）
 * @param val 写入值，非 0 置 1、0 清 0（不影响邻位）
 */
static inline void LIB_Hamming_BitSet(uint8_t *buf, uint32_t idx, uint8_t val)
{
    uint8_t mask = (uint8_t)(0x80u >> (idx & 7u));

    if (val)
        buf[idx >> 3] |= mask;
    else
        buf[idx >> 3] &= (uint8_t)~mask;
}

/*============================================
 *                 参数工具
 *============================================*/

/**
 * @brief 按数据位数选单块配置：k_s = min(data_bits, k)
 * @param data_bits 数据位数（须 > 0）
 * @param m 校验位数 2..7
 * @param[out] cfg 输出配置（m、k_s）
 * @return 0 成功；-1 参数非法
 * @note data_bits ≤ k 时 k_s = data_bits → 整帧一个校验组。
 */
int8_t LIB_Hamming_Fit(uint32_t data_bits, uint8_t m, LIB_Hamming_Cfg_t *cfg);

/**
 * @brief 计算编成码字所需的总位数
 * @param data_bits 数据位数（须 > 0）
 * @param cfg 码参数
 * @param ext 非 0 = 扩展码（每块多 1 位总校验位）
 * @return 码字总位数；参数非法或溢出返回 0
 */
uint32_t LIB_Hamming_RequiredBits(uint32_t data_bits, const LIB_Hamming_Cfg_t *cfg, uint8_t ext);

/*============================================
 *            单块核心（供扩展层复用）
 *============================================*/

/**
 * @brief 对一块数据编码（标准码，未扩展）
 * @param data 数据缓冲（k_s 位，MSB-first）
 * @param cfg 码参数
 * @param[out] code 输出码字（k_s+m 位；前 k_s 位 = 数据，后 m 位 = 校验）
 * @return OK 成功；BAD_ARG 参数非法
 */
LIB_Hamming_Status_t LIB_Hamming_EncodeBlock(const uint8_t *data, const LIB_Hamming_Cfg_t *cfg, uint8_t *code);

/**
 * @brief 对一块码字解码（标准码，单纠错）
 * @param code 码字缓冲（k_s+m 位）
 * @param cfg 码参数
 * @param[out] data 输出数据（k_s 位；可为 NULL）。不可纠时不作纠正，输出原始数据位
 * @param[out] syndrome 输出 syndrome（可为 NULL）
 * @param[out] err_pos 输出纠错位置（块内下标 0..k_s+m-1；无错或不可纠为 -1；可为 NULL）
 * @return OK 无错；CORRECTED 已纠单错；DETECTED 不可纠（数据未纠）；BAD_ARG 参数非法
 */
LIB_Hamming_Status_t LIB_Hamming_DecodeBlock(const uint8_t *code, const LIB_Hamming_Cfg_t *cfg, uint8_t *data,
                                             uint8_t *syndrome, int16_t *err_pos);

/**
 * @brief 由 syndrome 反查块内错误位置
 * @param syndrome syndrome 值（须非 0）
 * @param cfg 码参数
 * @param[out] pos 位置（0..k_s+m-1）；不可定位为 -1
 * @return OK 命中有效列；DETECTED 落在被缩短掉的数据列（只可能 ≥2 位错）；
 *         BAD_ARG 参数非法或 syndrome 为 0
 * @note 扩展层用它区分单错与多错，无需复制任何矩阵逻辑。
 */
LIB_Hamming_Status_t LIB_Hamming_PosFromSyndrome(uint8_t syndrome, const LIB_Hamming_Cfg_t *cfg, int16_t *pos);

/*============================================
 *          任意 bit 长度（标准码）
 *============================================*/

/**
 * @brief 编码任意 bit 长度数据（标准汉明码，按 k_s 分块）
 * @param data 数据缓冲（data_bits 位，MSB-first）
 * @param data_bits 数据位数（须 > 0，允许非字节对齐）
 * @param cfg 码参数
 * @param[out] code 输出码字缓冲
 * @param code_cap_bits code 容量（bit），须 ≥ LIB_Hamming_RequiredBits(data_bits, cfg, 0)
 * @param[out] code_bits 输出实际码字位数（仅成功时写）
 * @return 0 成功；-1 参数非法；-2 缓冲不足（不写缓冲）
 * @note 末块不足 k_s 位补零；code 尾部非整字节位会被清零。
 */
int8_t LIB_Hamming_Encode(const uint8_t *data, uint32_t data_bits, const LIB_Hamming_Cfg_t *cfg, uint8_t *code,
                          uint32_t code_cap_bits, uint32_t *code_bits);

/**
 * @brief 解码任意 bit 长度数据（标准汉明码）
 * @param code 码字缓冲
 * @param code_bits 码字位数，须等于 LIB_Hamming_RequiredBits(data_bits, cfg, 0)
 * @param data_bits 原始数据位数（用于确定块数与末块截断）
 * @param cfg 码参数
 * @param[out] data 输出数据缓冲（原始 data_bits 位；尾部置零）
 * @param[out] stat 统计（可为 NULL）
 * @return 0 成功；-1 参数非法（含 code_bits 不匹配）
 * @note 纠错能力有限，具体结果看 stat->corrected / stat->detected。
 */
int8_t LIB_Hamming_Decode(const uint8_t *code, uint32_t code_bits, uint32_t data_bits, const LIB_Hamming_Cfg_t *cfg,
                          uint8_t *data, LIB_Hamming_Stat_t *stat);

#endif /* __LIB_HAMMING_H */

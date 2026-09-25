/**
 * @file lib_hamming_ext.h
 * @brief 扩展缩短汉明码（SECDED）：标准码 + 1 位总校验位，纠 1 检 2，任意 bit 长度
 *
 * 构造（复用 lib_hamming.h 的标准码函数，不复制任何矩阵逻辑）：
 *   对每块 k_s 位数据：先按标准码编成 k_s+m 位码字（LIB_Hamming_EncodeBlock），
 *   再追加 1 位总校验位 p（该码字全部位的异或）→ 每块 k_s+m+1 位。
 *   缩短由 k_s < k 体现，与标准码共用同一套块函数。
 *
 * 解码判决（syndrome = 标准部分反查值，p = 收到码字全部位异或）：
 *   | syndrome | p | 判定                                              |
 *   |----------|---|---------------------------------------------------|
 *   |    0     | 0 | 无错                                              |
 *   |   != 0   | 1 | 单 bit 错，纠正                                    |
 *   |   != 0   | 1 | syndrome 落被缩短列 ⇒ ≥3 位奇数错，检出不可纠       |
 *   |   != 0   | 0 | ≥2 位错，检出不可纠                                |
 *   |    0     | 1 | 总校验位自身错，数据无错                            |
 *
 * @note 保证"纠 1 位、检 2 位"（最小距离 4）；≥3 位错可能误纠/漏检，属 SECDED 固有极限。
 * @note 每块独立纠错：跨越两块的突发多位错不保证可纠。
 * @see lib_hamming.h
 */

#ifndef __LIB_HAMMING_EXT_H
#define __LIB_HAMMING_EXT_H

#include "lib_hamming.h"

/*============================================
 *        编译期分块规划（宏，见 lib_hamming.h 同节）
 *============================================*/

/* 扩展码码字总字节数 E = B * (s + (m+1)/8)
 *   B、s 为 LIB_HAMMING_BYTE_BLKS / LIB_HAMMING_BYTE_BLK_DATA 的字节块规划，
 *   (m+1)/8 为每块的校验位字节数（m+1 位总校验 + 校验位，须为 8 的倍数，m=7 → 1 B）。
 * @note 要求 (m+1) 为 8 的倍数，否则码字按字节取整会有跨块余位，须改用
 *       LIB_HAMMING_CODE_BYTES(位) 版本自行按位排布。
 * @note E ≥ P，差值即汉明码膨胀；调用方据此在编译期定帧长与缓冲。 */
#define LIB_HAMMING_EXT_BYTES(payload_bytes, m)                                                                        \
    (LIB_HAMMING_BYTE_BLKS((payload_bytes), (m)) *                                                                     \
     (LIB_HAMMING_BYTE_BLK_DATA((payload_bytes), (m)) + (((m) + 1u) / 8u)))

/* 编译期校验扩展码字节块规划可用（(m+1) 须为 8 的倍数） */
#define LIB_HAMMING_EXT_CHECK_M(m)                                                                                     \
    _Static_assert(((m) + 1u) % 8u == 0u, "lib_hamming_ext: (m+1) must be a multiple of 8")

/*============================================
 *        任意 bit 长度（扩展缩短码）
 *============================================*/

/**
 * @brief 编码任意 bit 长度数据（扩展缩短汉明码，每块多 1 位总校验位）
 * @param data 数据缓冲（data_bits 位，MSB-first）
 * @param data_bits 数据位数（须 > 0，允许非字节对齐）
 * @param cfg 码参数（k_s < k 即缩短）
 * @param[out] code 输出码字缓冲
 * @param code_cap_bits code 容量（bit），须 ≥ LIB_Hamming_RequiredBits(data_bits, cfg, 1)
 * @param[out] code_bits 输出实际码字位数（仅成功时写）
 * @return 0 成功；-1 参数非法；-2 缓冲不足（不写缓冲）
 * @note 末块不足 k_s 位补零；code 尾部非整字节位会被清零。
 */
int8_t LIB_Hamming_ExtEncode(const uint8_t *data, uint32_t data_bits, const LIB_Hamming_Cfg_t *cfg, uint8_t *code,
                             uint32_t code_cap_bits, uint32_t *code_bits);

/**
 * @brief 解码任意 bit 长度数据（扩展缩短汉明码，纠 1 检 2）
 * @param code 码字缓冲
 * @param code_bits 码字位数，须等于 LIB_Hamming_RequiredBits(data_bits, cfg, 1)
 * @param data_bits 原始数据位数（用于确定块数与末块截断）
 * @param cfg 码参数
 * @param[out] data 输出数据缓冲（原始 data_bits 位；尾部置零）
 * @param[out] stat 统计（可为 NULL）
 * @return 0 成功；-1 参数非法（含 code_bits 不匹配）
 * @note 仅当确认为单 bit 错并纠正时才输出纠错后的数据；检出不可纠的块输出原始数据位。
 */
int8_t LIB_Hamming_ExtDecode(const uint8_t *code, uint32_t code_bits, uint32_t data_bits, const LIB_Hamming_Cfg_t *cfg,
                             uint8_t *data, LIB_Hamming_Stat_t *stat);

#endif /* __LIB_HAMMING_EXT_H */

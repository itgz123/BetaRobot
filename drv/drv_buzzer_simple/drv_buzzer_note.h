/**
 * @file drv_buzzer_note.h
 * @brief 音名 / 简谱 → 频率（编译期宏 + 运行时查表）
 *
 * @note 两套入口，分工不同：
 *       - **编译期宏**（BUZZER_DEG_HZ 等）：曲谱数组是 static const，初始化必须是常量表达式，
 *         只能用宏在编译期算。曲谱里写 `J(5)`（= 简谱 5）、`JH(1)`（= 高音 1），
 *         数组就能和简谱逐音对照；换调只改 `#define JIANPU_BASE`（1 的频率）。
 *       - **运行时查表**（BuzzerNoteHz / BuzzerDegreeHz）：频率表 s_note_hz[八度][音名]
 *         是绝对音高的唯一数据源（A4 = 440 Hz），用于运行时移调/生成旋律。
 *       两套都基于同一组十二平均律比值 BUZZER_RATIO_*，结果最多差 1 Hz（整数取整）。
 *
 * @note 曲谱约定：`JIANPU_BASE` 是简谱「1」的频率，例如 1=C5 就是 523。
 *       宏的参数必须能折成整数常量（度数写 1~7 的字面量、base 写字面量/宏）。
 *
 * @note 门控：实现在 drv_buzzer_note.c，需在 app_cfg.h 定义 DRV_BUZZER_USED。
 */

#ifndef __DRV_BUZZER_NOTE_H
#define __DRV_BUZZER_NOTE_H

#include <stdint.h>

/* 频率表覆盖的八度范围（中央 C 在 4 八度，A4 = 440 Hz） */
#define BUZZER_NOTE_OCTAVE_MIN 0u
#define BUZZER_NOTE_OCTAVE_MAX 8u

/* 十二平均律音名（升号拼写），同时是二维频率表的列索引 */
typedef enum : uint8_t
{
    BUZZER_NOTE_C = 0,
    BUZZER_NOTE_C_SHARP,
    BUZZER_NOTE_D,
    BUZZER_NOTE_D_SHARP,
    BUZZER_NOTE_E,
    BUZZER_NOTE_F,
    BUZZER_NOTE_F_SHARP,
    BUZZER_NOTE_G,
    BUZZER_NOTE_G_SHARP,
    BUZZER_NOTE_A,
    BUZZER_NOTE_A_SHARP,
    BUZZER_NOTE_B,
    BUZZER_NOTE_NUM,
} BuzzerNote_e;

/*============================================
 *      编译期：简谱 → 频率（曲谱数组用）
 *============================================*/
/* 十二平均律相对主音的频率比，单位 1e-5：2^(n/12) × 100000，n = 半音数 0~11 */
#define BUZZER_RATIO_0 100000u
#define BUZZER_RATIO_1 105946u
#define BUZZER_RATIO_2 112246u
#define BUZZER_RATIO_3 118920u
#define BUZZER_RATIO_4 125992u
#define BUZZER_RATIO_5 133484u
#define BUZZER_RATIO_6 141421u
#define BUZZER_RATIO_7 149831u
#define BUZZER_RATIO_8 158740u
#define BUZZER_RATIO_9 168179u
#define BUZZER_RATIO_10 178180u
#define BUZZER_RATIO_11 188775u

/* 简谱度数 → 相对主音的半音数（自然大调）：1 2 3 4 5 6 7 = 0 2 4 5 7 9 11 */
#define BUZZER_DEG_SEMITONE(d) ((d) == 1u ? 0u : (d) == 2u ? 2u : (d) == 3u ? 4u : (d) == 4u ? 5u : (d) == 5u ? 7u : (d) == 6u ? 9u : 11u)

/* 半音数 → 频率比（1e-5） */
#define BUZZER_SEMITONE_RATIO(s)                                                                                                                               \
    ((s) == 0u    ? BUZZER_RATIO_0                                                                                                                             \
     : (s) == 1u  ? BUZZER_RATIO_1                                                                                                                             \
     : (s) == 2u  ? BUZZER_RATIO_2                                                                                                                             \
     : (s) == 3u  ? BUZZER_RATIO_3                                                                                                                             \
     : (s) == 4u  ? BUZZER_RATIO_4                                                                                                                             \
     : (s) == 5u  ? BUZZER_RATIO_5                                                                                                                             \
     : (s) == 6u  ? BUZZER_RATIO_6                                                                                                                             \
     : (s) == 7u  ? BUZZER_RATIO_7                                                                                                                             \
     : (s) == 8u  ? BUZZER_RATIO_8                                                                                                                             \
     : (s) == 9u  ? BUZZER_RATIO_9                                                                                                                             \
     : (s) == 10u ? BUZZER_RATIO_10                                                                                                                            \
                  : BUZZER_RATIO_11)

/* 简谱度数 → 频率比（1e-5） */
#define BUZZER_DEG_RATIO(d) BUZZER_SEMITONE_RATIO(BUZZER_DEG_SEMITONE(d))

/* 编译期算频率（Hz，四舍五入）：1 = base_hz */
#define BUZZER_DEG_HZ(base_hz, d) ((uint16_t)(((uint32_t)(base_hz) * BUZZER_DEG_RATIO(d) + 50000u) / 100000u))

/* 高音点（上加点）/ 低音点（下加点）：简谱 1~7 的八度偏移 */
#define BUZZER_DEG_HZ_HI(base_hz, d) ((uint16_t)(((uint32_t)(base_hz) * BUZZER_DEG_RATIO(d) * 2u + 50000u) / 100000u))
#define BUZZER_DEG_HZ_LO(base_hz, d) ((uint16_t)(((uint32_t)(base_hz) * BUZZER_DEG_RATIO(d) + 100000u) / 200000u))

/*============================================
 *      运行时：音名 / 简谱 → 频率（查表）
 *============================================*/
/**
 * @brief 查表得到音高频率（Hz）
 * @param note  音名（C~B）
 * @param octave 八度（0~8，越大越高）
 * @return 频率（Hz）；参数越界返回 0
 */
uint16_t BuzzerNoteHz(BuzzerNote_e note, uint8_t octave);

/**
 * @brief 简谱度数 → 相对主音的半音数（自然大调）
 * @param degree 简谱数字 1~7
 * @return 半音数（1→0、2→2、3→4、4→5、5→7、6→9、7→11）；非法返回 -1
 */
int8_t BuzzerDegreeSemitone(uint8_t degree);

/**
 * @brief 简谱 → 频率：以「(tonic_note, tonic_octave) 这个音」为 1
 * @param tonic_note   主音音名（简谱 1 的音名）
 * @param tonic_octave 主音八度
 * @param degree       简谱数字 1~7（1 = 主音本身）
 * @param octave_shift 八度偏移：+1 = 高音点、-1 = 低音点、0 = 与主音同八度
 * @return 频率（Hz）；参数越界（度数非法 / 超出八度 0~8）返回 0
 *
 * @note 例：1=C5 时 BuzzerDegreeHz(BUZZER_NOTE_C, 5, 5, 0) = G5 784；
 *       1=♭B4 时 BuzzerDegreeHz(BUZZER_NOTE_A_SHARP, 4, 5, 0) = F5 698。
 *       ♭B 用等音 A♯ 表示（频率相同）。
 */
uint16_t BuzzerDegreeHz(BuzzerNote_e tonic_note, uint8_t tonic_octave, uint8_t degree, int8_t octave_shift);

#endif /* __DRV_BUZZER_NOTE_H */

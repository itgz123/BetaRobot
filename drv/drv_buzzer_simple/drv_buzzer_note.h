/**
 * @file drv_buzzer_note.h
 * @brief 音名 / 简谱 → 频率，几分音符 → 毫秒（纯编译期宏，无 .c）
 *
 * @note 曲谱数组是 `static const`，初始化必须是常量表达式，所以全部用宏：
 *       - `BUZZER_NOTE_HZ(base, d, oct)` 三个输入 = 简谱 1 的实际发声音名、
 *         简谱数字、上下加点八度偏移；内部先算绝对半音偏移（C4 = 0、
 *         A4 = +9 = 440 Hz），再查生成的宏表得到 Hz。
 *       - 音名 token → 偏移用 `BUZZER_PITCH(name)` 宏拼接，例如
 *         `BUZZER_PITCH(BB4) = 10`；频率表在 drv_buzzer_note_table.h。
 *       - `BUZZER_NOTE_MS(bpm, num, den)` 把「几分音符」换算成 ms，
 *         例如 BUZZER_NOTE_MS(80, 1, 4) = 四分音符 750 ms。
 *
 * @note `SONG_BASE` 写简谱「1」的实际发声音名 token（BB4/C5/...），不是频率数字。
 * @note 本头文件只有宏，曲谱模块由 app_cfg.h 的 LIB_MUSIC_USED 控制是否编入。
 */

#ifndef __DRV_BUZZER_NOTE_H
#define __DRV_BUZZER_NOTE_H

#include <stdint.h>

/*============================================
 *      编译期：音名 / 简谱 → 频率
 *============================================*/
/* 音名 token → 相对 C4 的半音偏移；表由 tools/gen_note_table.py 生成。
 * 例：BUZZER_PITCH(C4)=0、BUZZER_PITCH(A4)=9、BUZZER_PITCH(BB4)=10。 */
#define BUZZER_PITCH(name)   BUZZER_PITCH_I(name)
#define BUZZER_PITCH_I(name) BUZZER_PITCH_##name
#include "drv_buzzer_note_table.h"

/* 简谱度数 → 相对主音的半音数（自然大调）：1 2 3 4 5 6 7 = 0 2 4 5 7 9 11 */
#define BUZZER_DEG_SEMITONE(d) \
    ((d) == 1u ? 0u : (d) == 2u ? 2u : (d) == 3u ? 4u : (d) == 4u ? 5u : (d) == 5u ? 7u : (d) == 6u ? 9u : 11u)

/* 绝对半音偏移（C4 = 0）：
 *   base = 简谱 1 的实际发声音名 token（BB4/C5/...）
 *   d    = 简谱数字 1~7（自然大调）
 *   oct  = 上下加点：+1 高八度、-1 低八度、+2 两点 */
#define BUZZER_NOTE_OFFSET(base, d, oct) \
    ((int32_t)(BUZZER_PITCH(base)) + (int32_t)BUZZER_DEG_SEMITONE(d) + 12 * (int32_t)(oct))

/* 直接给半音偏移，用于临时记号/半音阶 */
#define BUZZER_NOTE_OFFSET_SEMI(base, semi, oct) \
    ((int32_t)(BUZZER_PITCH(base)) + (int32_t)(semi) + 12 * (int32_t)(oct))

/* 三个输入 → Hz（超出 C0~B8 返回 0） */
#define BUZZER_NOTE_HZ(base, d, oct) BUZZER_HZ_AT(BUZZER_NOTE_OFFSET(base, d, oct))
#define BUZZER_NOTE_HZ_SEMI(base, semi, oct) BUZZER_HZ_AT(BUZZER_NOTE_OFFSET_SEMI(base, semi, oct))

/* 编译期自检：基准与常用音 */
_Static_assert(BUZZER_HZ_AT(0) == 262u, "C4 must be 262 Hz");
_Static_assert(BUZZER_HZ_AT(9) == 440u, "A4 must be 440 Hz");
_Static_assert(BUZZER_NOTE_HZ(BB4, 5, 0) == 698u, "1=bB4, degree 5 must be F5");

/*============================================
 *      编译期：音符时值（几分音符 → ms）
 *============================================*/
/* 全音符 = 4 拍 = 240000 / bpm ms；音符时值 = 全音符 × num/den。
 * num/den 是音符时值相对全音符的比例：
 *   1/4 四分、1/8 八分、1/16 十六分、1/2 二分、1/1 全音符；
 *   3/8 附点四分、3/4 附点二分、1/12 八分三连音、1/6 四分三连音。
 * 例：BUZZER_NOTE_MS(80, 1, 4) = 750（四分音符）；1/16 = 188（四舍五入）。 */
#define BUZZER_NOTE_MS(bpm, num, den)                                                               \
    ((uint16_t)(((uint32_t)240000u * (uint32_t)(num) + ((uint32_t)(bpm) * (uint32_t)(den)) / 2u) / \
                ((uint32_t)(bpm) * (uint32_t)(den))))

#define BUZZER_WHOLE_MS(bpm)          BUZZER_NOTE_MS(bpm, 1u, 1u)
#define BUZZER_HALF_MS(bpm)           BUZZER_NOTE_MS(bpm, 1u, 2u)
#define BUZZER_QUARTER_MS(bpm)        BUZZER_NOTE_MS(bpm, 1u, 4u)
#define BUZZER_EIGHTH_MS(bpm)         BUZZER_NOTE_MS(bpm, 1u, 8u)
#define BUZZER_SIXTEENTH_MS(bpm)      BUZZER_NOTE_MS(bpm, 1u, 16u)
#define BUZZER_THIRTY_SECOND_MS(bpm)  BUZZER_NOTE_MS(bpm, 1u, 32u)
#define BUZZER_DOTTED_HALF_MS(bpm)    BUZZER_NOTE_MS(bpm, 3u, 4u)
#define BUZZER_DOTTED_QUARTER_MS(bpm) BUZZER_NOTE_MS(bpm, 3u, 8u)
#define BUZZER_DOTTED_EIGHTH_MS(bpm)  BUZZER_NOTE_MS(bpm, 3u, 16u)
#define BUZZER_TRIPLET_EIGHTH_MS(bpm) BUZZER_NOTE_MS(bpm, 1u, 12u)
#define BUZZER_TRIPLET_QUARTER_MS(bpm) BUZZER_NOTE_MS(bpm, 1u, 6u)

#endif /* __DRV_BUZZER_NOTE_H */

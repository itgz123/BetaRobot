/* See You Again（吉他谱）—— 严格按同目录 see_you_again吉他谱.md 逐格转录
 * 原曲：Wiz Khalifa / Charlie Puth《See You Again》
 * 谱面 1=G、bB 调（夹三品），实际发声音高按 1 = bB4 = 466 Hz；4/4 拍、80 BPM
 * 一格 = 16 分音符（187.5 ms），一行 = 1 小节（16 格），共 38 小节
 * 记号：J(d)=简谱 d，JH(d)=上加点（+i），CELLS(n)=连续 n 格的时值
 *      同音连音符合并成一个长音；异音连音符按滑音连奏（仍起新音）
 *      前倚音 1?2 忽略 1、按 2 发声；0=休止，空格=延续前一个音
 * 258 个脉冲，约 114.0 s
 * 门控：LIB_MUSIC_USED */
#include "see_you_again_guitar.h"
#include "app_cfg.h"
#include "drv_buzzer_note.h"

#ifdef LIB_MUSIC_USED

#define JIANPU_BASE 466u                                        /* 1 = bB4（谱面 1=G、bB 调，按发声音高） */
#define J(d) BUZZER_DEG_HZ(JIANPU_BASE, (d))                    /* 简谱 1~7 */
#define JH(d) BUZZER_DEG_HZ_HI(JIANPU_BASE, (d))                /* +i：上加点，高八度 */
#define CELLS(n) ((uint16_t)(((uint32_t)(n) * 375u + 1u) / 2u)) /* n 个 16 分音符（80 BPM） */

static const MusicPulse_t s_see_you_again_guitar[258] = {
    /* ---------------- 第 1 小节 ---------------- */
    {J(5), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {J(5), CELLS(4), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    {JH(3), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    /* ---------------- 第 2 小节 ---------------- */
    {J(5), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {J(5), CELLS(4), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    {JH(3), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    /* ---------------- 第 3 小节 ---------------- */
    {J(5), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {J(5), CELLS(4), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    {JH(3), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(2), CELLS(1), 0u},
    /* ---------------- 第 4 小节 ---------------- */
    {J(5), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {J(5), CELLS(4), 0u},
    {J(1), CELLS(2), 0u},
    {J(3), CELLS(2), 0u},
    {J(5), CELLS(2), 0u},
    /* ---------------- 第 5 小节 ---------------- */
    {J(6), CELLS(6), 0u},
    {J(5), CELLS(6), 0u},
    {MUSIC_NOTE_REST, CELLS(3), 0u},
    {J(1), CELLS(1), 0u},
    /* ---------------- 第 6 小节 ---------------- */
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(3), CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(2), 0u},
    {J(3), CELLS(1), 0u},
    {J(5), CELLS(1), 0u},
    /* ---------------- 第 7 小节 ---------------- */
    {J(6), CELLS(2), 0u},
    {J(7), CELLS(2), 0u},
    {J(6), CELLS(2), 0u},
    {J(5), CELLS(2), 0u},
    {J(3), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    /* ---------------- 第 8 小节 ---------------- */
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(1), 0u},
    {J(1), CELLS(1), 0u},
    {J(3), CELLS(1), 0u},
    {J(5), CELLS(1), 0u},
    /* ---------------- 第 9 小节 ---------------- */
    {J(6), CELLS(6), 0u},
    {J(5), CELLS(6), 0u},
    {MUSIC_NOTE_REST, CELLS(3), 0u},
    {J(1), CELLS(1), 0u},
    /* ---------------- 第 10 小节 ---------------- */
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    {J(3), CELLS(2), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {J(2), CELLS(1), 0u},
    {J(3), CELLS(1), 0u},
    {J(5), CELLS(2), 0u},
    /* ---------------- 第 11 小节 ---------------- */
    {J(6), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {J(5), CELLS(1), 0u},
    {J(6), CELLS(1), 0u},
    {JH(1), CELLS(2), 0u},
    /* ---------------- 第 12 小节 ---------------- */
    {JH(3), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {J(5), CELLS(1), 0u},
    {J(6), CELLS(1), 0u},
    {JH(1), CELLS(2), 0u},
    /* ---------------- 第 13 小节 ---------------- */
    {JH(3), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 14 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 15 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 16 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 17 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 18 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 19 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 20 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 21 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 22 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(3), CELLS(2), 0u},
    /* ---------------- 第 23 小节 ---------------- */
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {J(6), CELLS(3), 0u},
    {JH(3), CELLS(1), 0u},
    /* ---------------- 第 24 小节 ---------------- */
    {JH(1), CELLS(2), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {J(6), CELLS(3), 0u},
    {J(6), CELLS(1), 0u},
    /* ---------------- 第 25 小节 ---------------- */
    {JH(3), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(1), 0u},
    {JH(1), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    {J(3), CELLS(2), 0u},
    {J(5), CELLS(2), 0u},
    /* ---------------- 第 26 小节 ---------------- */
    {J(6), CELLS(6), 0u},
    {J(5), CELLS(6), 0u},
    {MUSIC_NOTE_REST, CELLS(3), 0u},
    {J(1), CELLS(1), 0u},
    /* ---------------- 第 27 小节 ---------------- */
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(3), CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(2), 0u},
    {J(3), CELLS(1), 0u},
    {J(5), CELLS(1), 0u},
    /* ---------------- 第 28 小节 ---------------- */
    {J(6), CELLS(2), 0u},
    {J(7), CELLS(2), 0u},
    {J(6), CELLS(2), 0u},
    {J(5), CELLS(2), 0u},
    {J(3), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    /* ---------------- 第 29 小节 ---------------- */
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(1), 0u},
    {J(1), CELLS(1), 0u},
    {J(3), CELLS(1), 0u},
    {J(5), CELLS(1), 0u},
    /* ---------------- 第 30 小节 ---------------- */
    {J(6), CELLS(6), 0u},
    {J(5), CELLS(6), 0u},
    {MUSIC_NOTE_REST, CELLS(3), 0u},
    {J(1), CELLS(1), 0u},
    /* ---------------- 第 31 小节 ---------------- */
    {J(2), CELLS(2), 0u},
    {J(2), CELLS(2), 0u},
    {J(1), CELLS(2), 0u},
    {J(3), CELLS(2), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {J(2), CELLS(1), 0u},
    {J(3), CELLS(1), 0u},
    {J(5), CELLS(2), 0u},
    /* ---------------- 第 32 小节 ---------------- */
    {J(6), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {JH(1), CELLS(2), 0u},
    {J(5), CELLS(1), 0u},
    {J(6), CELLS(1), 0u},
    {JH(1), CELLS(2), 0u},
    /* ---------------- 第 33 小节 ---------------- */
    {JH(3), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {J(5), CELLS(1), 0u},
    {J(6), CELLS(1), 0u},
    {JH(1), CELLS(2), 0u},
    /* ---------------- 第 34 小节 ---------------- */
    {JH(3), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(3), CELLS(2), 0u},
    {JH(2), CELLS(2), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {JH(1), CELLS(2), 0u},
    {J(7), CELLS(2), 0u},
    /* ---------------- 第 35 小节 ---------------- */
    {J(6), CELLS(6), 0u},
    {J(5), CELLS(6), 0u},
    {JH(1), CELLS(2), 0u},
    {J(7), CELLS(2), 0u},
    /* ---------------- 第 36 小节 ---------------- */
    {J(6), CELLS(3), 0u},
    {J(7), CELLS(1), 0u},
    {J(6), CELLS(2), 0u},
    {J(5), CELLS(2), 0u},
    {J(3), CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 37 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    /* ---------------- 第 38 小节 ---------------- */
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
    {MUSIC_NOTE_REST, CELLS(4), 0u},
};

const MusicSequence_t MUSIC_SEE_YOU_AGAIN_GUITAR = {s_see_you_again_guitar, MUSIC_PULSE_COUNT(s_see_you_again_guitar)};

#endif /* LIB_MUSIC_USED */

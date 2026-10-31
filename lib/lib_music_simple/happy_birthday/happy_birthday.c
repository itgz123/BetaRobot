/* 祝你生日快乐（1 = C5，3/4 拍，BPM 100，弱起两拍）
 *
 *   5 5 | 6 5 1' | 7 - - |        祝 你 生 日 快 乐
 *   5 5 | 6 5 2' | 1' - - |       祝 你 生 日 快 乐
 *   5 5 | 5' 3' 1' | 7 6 - |      祝 你 生 日 快 乐
 *   4' 4' | 3' 1' 2' | 1' - - ||  祝 你 生 日 快 乐
 *
 * 节奏：3/4 拍，每句以"弱起"`5 5`（两个八分音符，合 1 拍）起头，
 *       正拍第一小节 3 个四分音符，第二小节由末音延长成整小节（3 拍）。
 * 音高：数组里 J(1)~J(7) = 简谱数字，JH(1)~JH(5) = 上加点的高音（1'、2' …）。
 * 25 个脉冲，约 16.8 s（28 拍 x 600 ms）
 * 每音线性衰减（decay_ms）由 BuzzerConfig 决定，见同目录 readme.md */
#include "happy_birthday.h"
#include "app_cfg.h"
#include "drv_buzzer_note.h"

#ifdef LIB_MUSIC_USED

#define JIANPU_BASE 523u          /* 1 = C5 */
#define J(d) BUZZER_DEG_HZ(JIANPU_BASE, (d))
#define JH(d) BUZZER_DEG_HZ_HI(JIANPU_BASE, (d)) /* 高音点 */
#define NOTE(freq, dur) {(freq), (dur), 0u}

static const MusicPulse_t s_happy_birthday[25] = {
    /* 祝你生日快乐（一）5 5 | 6 5 1' | 7 - - */
    NOTE(J(5), 300u), NOTE(J(5), 300u),
    NOTE(J(6), 600u), NOTE(J(5), 600u), NOTE(JH(1), 600u),
    NOTE(J(7), 1800u),
    /* 祝你生日快乐（二）5 5 | 6 5 2' | 1' - - */
    NOTE(J(5), 300u), NOTE(J(5), 300u),
    NOTE(J(6), 600u), NOTE(J(5), 600u), NOTE(JH(2), 600u),
    NOTE(JH(1), 1800u),
    /* 祝你生日快乐（三）5 5 | 5' 3' 1' | 7 6 - */
    NOTE(J(5), 300u), NOTE(J(5), 300u),
    NOTE(JH(5), 600u), NOTE(JH(3), 600u), NOTE(JH(1), 600u),
    NOTE(J(7), 600u), NOTE(J(6), 1200u),
    /* 祝你生日快乐（四）4' 4' | 3' 1' 2' | 1' - - */
    NOTE(JH(4), 300u), NOTE(JH(4), 300u),
    NOTE(JH(3), 600u), NOTE(JH(1), 600u), NOTE(JH(2), 600u),
    NOTE(JH(1), 1800u),
};

const MusicSequence_t MUSIC_HAPPY_BIRTHDAY = {s_happy_birthday, MUSIC_PULSE_COUNT(s_happy_birthday)};

#endif /* LIB_MUSIC_USED */

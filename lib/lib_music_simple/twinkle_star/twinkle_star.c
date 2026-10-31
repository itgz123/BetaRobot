/* 小星星（1 = C5，4/4 拍，BPM 100）
 *
 *   1 1 5 5 | 6 6 5 - | 4 4 3 3 | 2 2 1 - |
 *   5 5 4 4 | 3 3 2 - | 5 5 4 4 | 3 3 2 - |
 *   1 1 5 5 | 6 6 5 - | 4 4 3 3 | 2 2 1 - |
 *
 * 节奏：一个记号 = 一拍（600 ms），'-' 再延长一拍。
 * 音高：数组里 J(1)~J(6) 就是上面的简谱数字，换调只改 JIANPU_BASE。
 */
#include "twinkle_star.h"
#include "app_cfg.h"
#include "drv_buzzer_note.h"

#ifdef LIB_MUSIC_USED

#define BEAT 600u        /* 一拍（BPM 100） */
#define JIANPU_BASE 523u /* 1 = C5 */
#define J(d) BUZZER_DEG_HZ(JIANPU_BASE, (d))
#define NOTE(freq, dur) {(freq), (dur), 0u}

static const MusicPulse_t s_twinkle_star[] = {
    /* 1 1 5 5 | 6 6 5 - */
    NOTE(J(1), BEAT),
    NOTE(J(1), BEAT),
    NOTE(J(5), BEAT),
    NOTE(J(5), BEAT),
    NOTE(J(6), BEAT),
    NOTE(J(6), BEAT),
    NOTE(J(5), 2u * BEAT),
    /* 4 4 3 3 | 2 2 1 - */
    NOTE(J(4), BEAT),
    NOTE(J(4), BEAT),
    NOTE(J(3), BEAT),
    NOTE(J(3), BEAT),
    NOTE(J(2), BEAT),
    NOTE(J(2), BEAT),
    NOTE(J(1), 2u * BEAT),
    /* 5 5 4 4 | 3 3 2 - */
    NOTE(J(5), BEAT),
    NOTE(J(5), BEAT),
    NOTE(J(4), BEAT),
    NOTE(J(4), BEAT),
    NOTE(J(3), BEAT),
    NOTE(J(3), BEAT),
    NOTE(J(2), 2u * BEAT),
    /* 5 5 4 4 | 3 3 2 - */
    NOTE(J(5), BEAT),
    NOTE(J(5), BEAT),
    NOTE(J(4), BEAT),
    NOTE(J(4), BEAT),
    NOTE(J(3), BEAT),
    NOTE(J(3), BEAT),
    NOTE(J(2), 2u * BEAT),
    /* 1 1 5 5 | 6 6 5 - */
    NOTE(J(1), BEAT),
    NOTE(J(1), BEAT),
    NOTE(J(5), BEAT),
    NOTE(J(5), BEAT),
    NOTE(J(6), BEAT),
    NOTE(J(6), BEAT),
    NOTE(J(5), 2u * BEAT),
    /* 4 4 3 3 | 2 2 1 - */
    NOTE(J(4), BEAT),
    NOTE(J(4), BEAT),
    NOTE(J(3), BEAT),
    NOTE(J(3), BEAT),
    NOTE(J(2), BEAT),
    NOTE(J(2), BEAT),
    NOTE(J(1), 2u * BEAT),
};

const MusicSequence_t MUSIC_TWINKLE_STAR = {s_twinkle_star, MUSIC_PULSE_COUNT(s_twinkle_star)};

#endif /* LIB_MUSIC_USED */

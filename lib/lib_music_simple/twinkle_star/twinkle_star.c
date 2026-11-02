/* 小星星（1 = C5，4/4 拍，BPM 100）
 *
 *   1 1 5 5 | 6 6 5 - | 4 4 3 3 | 2 2 1 - |
 *   5 5 4 4 | 3 3 2 - | 5 5 4 4 | 3 3 2 - |
 *   1 1 5 5 | 6 6 5 - | 4 4 3 3 | 2 2 1 - |
 *
 * 节奏：Q = 四分音符（一拍），H = 二分音符（两拍）。
 * 音高：数组里 J(1)~J(6) 就是上面的简谱数字，换调只改 SONG_BASE。
 */
#include "twinkle_star.h"
#include "app_cfg.h"
#include "drv_buzzer_note.h"

#ifdef LIB_MUSIC_USED

#define BPM 100u
#define SONG_BASE C5 /* 1 = C5 */
#define J(d) BUZZER_NOTE_HZ(SONG_BASE, (d), 0)
#define Q BUZZER_QUARTER_MS(BPM) /* 四分音符（一拍） */
#define H BUZZER_HALF_MS(BPM)    /* 二分音符（两拍） */
#define NOTE(freq, dur) {(freq), (dur), 0u}

static const MusicPulse_t s_twinkle_star[] = {
    /* 1 1 5 5 | 6 6 5 - */
    NOTE(J(1), Q),
    NOTE(J(1), Q),
    NOTE(J(5), Q),
    NOTE(J(5), Q),
    NOTE(J(6), Q),
    NOTE(J(6), Q),
    NOTE(J(5), H),
    /* 4 4 3 3 | 2 2 1 - */
    NOTE(J(4), Q),
    NOTE(J(4), Q),
    NOTE(J(3), Q),
    NOTE(J(3), Q),
    NOTE(J(2), Q),
    NOTE(J(2), Q),
    NOTE(J(1), H),
    /* 5 5 4 4 | 3 3 2 - */
    NOTE(J(5), Q),
    NOTE(J(5), Q),
    NOTE(J(4), Q),
    NOTE(J(4), Q),
    NOTE(J(3), Q),
    NOTE(J(3), Q),
    NOTE(J(2), H),
    /* 5 5 4 4 | 3 3 2 - */
    NOTE(J(5), Q),
    NOTE(J(5), Q),
    NOTE(J(4), Q),
    NOTE(J(4), Q),
    NOTE(J(3), Q),
    NOTE(J(3), Q),
    NOTE(J(2), H),
    /* 1 1 5 5 | 6 6 5 - */
    NOTE(J(1), Q),
    NOTE(J(1), Q),
    NOTE(J(5), Q),
    NOTE(J(5), Q),
    NOTE(J(6), Q),
    NOTE(J(6), Q),
    NOTE(J(5), H),
    /* 4 4 3 3 | 2 2 1 - */
    NOTE(J(4), Q),
    NOTE(J(4), Q),
    NOTE(J(3), Q),
    NOTE(J(3), Q),
    NOTE(J(2), Q),
    NOTE(J(2), Q),
    NOTE(J(1), H),
};

const MusicSequence_t MUSIC_TWINKLE_STAR = {s_twinkle_star, MUSIC_PULSE_COUNT(s_twinkle_star)};

#endif /* LIB_MUSIC_USED */

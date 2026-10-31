/**
 * @file music_custom.c
 * @brief 常用提示音与报警音效实现
 */

#include "music_custom.h"

#include "app_cfg.h"

#ifdef LIB_MUSIC_USED

/* 板载蜂鸣器的谐振点（CubeMX 给 TIM_BUZZER 配的 ~4 kHz）：提示音用它最响 */
#define MUSIC_HZ_BUZZER 4000u

/* 音名 → Hz（摘自 drv_buzzer_simple/drv_buzzer_note.c 的频率表）：
 * C5 523、E5 659、G4 392、G5 784、C6 1047 */

/* ---------------- 基础音效 ---------------- */

static const MusicPulse_t s_effect_short[] = {
    {MUSIC_HZ_BUZZER, 80u, 100u},
    {MUSIC_NOTE_REST, 120u, 0u},
};

static const MusicPulse_t s_effect_long[] = {
    {MUSIC_HZ_BUZZER, 400u, 100u},
    {MUSIC_NOTE_REST, 200u, 0u},
};

static const MusicPulse_t s_effect_alarm[] = {
    {MUSIC_HZ_BUZZER, 100u, 100u}, {MUSIC_NOTE_REST, 80u, 0u},    {MUSIC_HZ_BUZZER, 100u, 100u},
    {MUSIC_NOTE_REST, 80u, 0u},    {MUSIC_HZ_BUZZER, 100u, 100u}, {MUSIC_NOTE_REST, 400u, 0u},
};

static const MusicPulse_t s_effect_startup[] = {
    {523u, 120u, 80u},  // C5
    {659u, 120u, 80u},  // E5
    {784u, 120u, 80u},  // G5
    {1047u, 200u, 80u}, // C6
    {MUSIC_NOTE_REST, 100u, 0u},
};

/* ---------------- 常用提示音（单次播放，末尾不留休止） ---------------- */

static const MusicPulse_t s_effect_tick[] = {
    {MUSIC_HZ_BUZZER, 30u, 100u},
};

static const MusicPulse_t s_effect_ok[] = {
    {523u, 70u, 80u},  // C5
    {784u, 120u, 80u}, // G5
};

static const MusicPulse_t s_effect_err[] = {
    {392u, 150u, 90u}, // G4
    {392u, 220u, 90u}, // G4
};

static const MusicPulse_t s_effect_notify[] = {
    {659u, 80u, 85u},   // E5
    {784u, 80u, 85u},   // G5
    {1047u, 140u, 85u}, // C6
};

static const MusicPulse_t s_effect_poweroff[] = {
    {1047u, 100u, 80u}, // C6
    {784u, 100u, 80u},  // G5
    {523u, 180u, 80u},  // C5
};

/* ---------------- 错误音（从轻到重） ---------------- */

static const MusicPulse_t s_effect_err_minor[] = {
    {MUSIC_HZ_BUZZER, 60u, 100u},
};

static const MusicPulse_t s_effect_err_major[] = {
    {MUSIC_HZ_BUZZER, 80u, 100u},
    {MUSIC_HZ_BUZZER, 80u, 100u},
};

static const MusicPulse_t s_effect_err_critical[] = {
    {MUSIC_HZ_BUZZER, 80u, 100u},
    {MUSIC_HZ_BUZZER, 80u, 100u},
    {MUSIC_HZ_BUZZER, 80u, 100u},
};

static const MusicPulse_t s_effect_err_fatal[] = {
    {MUSIC_HZ_BUZZER, 500u, 100u},
    {MUSIC_HZ_BUZZER, 80u, 100u},
};

static const MusicPulse_t s_effect_err_emergency[] = {
    {MUSIC_HZ_BUZZER, 120u, 100u}, {2000u, 120u, 100u}, {MUSIC_HZ_BUZZER, 120u, 100u}, {2000u, 120u, 100u}, {MUSIC_HZ_BUZZER, 120u, 100u}, {2000u, 120u, 100u},
};

/* ---------------- 调试 ---------------- */

static const MusicPulse_t s_demo_scale[] = {
    {523u, 150u, 0u},  // C5
    {587u, 150u, 0u},  // D5
    {659u, 150u, 0u},  // E5
    {698u, 150u, 0u},  // F5
    {784u, 150u, 0u},  // G5
    {880u, 150u, 0u},  // A5
    {988u, 150u, 0u},  // B5
    {1047u, 300u, 0u}, // C6
    {MUSIC_NOTE_REST, 300u, 0u},
};

/* ---------------- 曲目结构（一首音效 = 一个结构体） ---------------- */

const MusicSequence_t MUSIC_EFFECT_SHORT = {s_effect_short, MUSIC_PULSE_COUNT(s_effect_short)};
const MusicSequence_t MUSIC_EFFECT_LONG = {s_effect_long, MUSIC_PULSE_COUNT(s_effect_long)};
const MusicSequence_t MUSIC_EFFECT_ALARM = {s_effect_alarm, MUSIC_PULSE_COUNT(s_effect_alarm)};
const MusicSequence_t MUSIC_EFFECT_STARTUP = {s_effect_startup, MUSIC_PULSE_COUNT(s_effect_startup)};

const MusicSequence_t MUSIC_EFFECT_TICK = {s_effect_tick, MUSIC_PULSE_COUNT(s_effect_tick)};
const MusicSequence_t MUSIC_EFFECT_OK = {s_effect_ok, MUSIC_PULSE_COUNT(s_effect_ok)};
const MusicSequence_t MUSIC_EFFECT_ERR = {s_effect_err, MUSIC_PULSE_COUNT(s_effect_err)};
const MusicSequence_t MUSIC_EFFECT_NOTIFY = {s_effect_notify, MUSIC_PULSE_COUNT(s_effect_notify)};
const MusicSequence_t MUSIC_EFFECT_POWEROFF = {s_effect_poweroff, MUSIC_PULSE_COUNT(s_effect_poweroff)};

const MusicSequence_t MUSIC_EFFECT_ERR_MINOR = {s_effect_err_minor, MUSIC_PULSE_COUNT(s_effect_err_minor)};
const MusicSequence_t MUSIC_EFFECT_ERR_MAJOR = {s_effect_err_major, MUSIC_PULSE_COUNT(s_effect_err_major)};
const MusicSequence_t MUSIC_EFFECT_ERR_CRITICAL = {s_effect_err_critical, MUSIC_PULSE_COUNT(s_effect_err_critical)};
const MusicSequence_t MUSIC_EFFECT_ERR_FATAL = {s_effect_err_fatal, MUSIC_PULSE_COUNT(s_effect_err_fatal)};
const MusicSequence_t MUSIC_EFFECT_ERR_EMERGENCY = {s_effect_err_emergency, MUSIC_PULSE_COUNT(s_effect_err_emergency)};

const MusicSequence_t MUSIC_DEMO_SCALE = {s_demo_scale, MUSIC_PULSE_COUNT(s_demo_scale)};

#endif /* LIB_MUSIC_USED */

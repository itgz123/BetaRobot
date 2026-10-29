/**
 * @file lib_music_simple.c
 * @brief 音频脉冲数组实现（纯数据，无逻辑）
 */

#include "lib_music_simple.h"

#include "app_cfg.h"

#ifdef LIB_MUSIC_USED

/* 默认音效统一用蜂鸣器谐振点（提示音最响），音量给满；
 * 每个序列末尾的休止符让上层"离线期间循环触发"时自然产生间隔。 */

const MusicPulse_t MUSIC_EFFECT_SHORT[] = {
    {MUSIC_NOTE_BUZZER, 80u, 100u},
    {MUSIC_NOTE_REST, 120u, 0u},
};

const MusicPulse_t MUSIC_EFFECT_LONG[] = {
    {MUSIC_NOTE_BUZZER, 400u, 100u},
    {MUSIC_NOTE_REST, 200u, 0u},
};

const MusicPulse_t MUSIC_EFFECT_ALARM[] = {
    {MUSIC_NOTE_BUZZER, 100u, 100u}, {MUSIC_NOTE_REST, 80u, 0u},      {MUSIC_NOTE_BUZZER, 100u, 100u},
    {MUSIC_NOTE_REST, 80u, 0u},      {MUSIC_NOTE_BUZZER, 100u, 100u}, {MUSIC_NOTE_REST, 400u, 0u},
};

const MusicPulse_t MUSIC_EFFECT_STARTUP[] = {
    {MUSIC_NOTE_C5, 120u, 80u}, {MUSIC_NOTE_E5, 120u, 80u}, {MUSIC_NOTE_G5, 120u, 80u}, {MUSIC_NOTE_C6, 200u, 80u}, {MUSIC_NOTE_REST, 100u, 0u},
};

/* ---------------- 常用提示音（单次播放，末尾不留休止） ---------------- */

const MusicPulse_t MUSIC_EFFECT_TICK[] = {
    {MUSIC_NOTE_BUZZER, 30u, 100u},
};

const MusicPulse_t MUSIC_EFFECT_OK[] = {
    {MUSIC_NOTE_C5, 70u, 80u},
    {MUSIC_NOTE_G5, 120u, 80u},
};

const MusicPulse_t MUSIC_EFFECT_ERR[] = {
    {MUSIC_NOTE_G4, 150u, 90u},
    {MUSIC_NOTE_G4, 220u, 90u},
};

const MusicPulse_t MUSIC_EFFECT_NOTIFY[] = {
    {MUSIC_NOTE_E5, 80u, 85u},
    {MUSIC_NOTE_G5, 80u, 85u},
    {MUSIC_NOTE_C6, 140u, 85u},
};

const MusicPulse_t MUSIC_EFFECT_POWEROFF[] = {
    {MUSIC_NOTE_C6, 100u, 80u},
    {MUSIC_NOTE_G5, 100u, 80u},
    {MUSIC_NOTE_C5, 180u, 80u},
};

/* ---------------- 错误音（从轻到重） ---------------- */

const MusicPulse_t MUSIC_EFFECT_ERR_MINOR[] = {
    {MUSIC_NOTE_BUZZER, 60u, 100u},
};

const MusicPulse_t MUSIC_EFFECT_ERR_MAJOR[] = {
    {MUSIC_NOTE_BUZZER, 80u, 100u},
    {MUSIC_NOTE_BUZZER, 80u, 100u},
};

const MusicPulse_t MUSIC_EFFECT_ERR_CRITICAL[] = {
    {MUSIC_NOTE_BUZZER, 80u, 100u},
    {MUSIC_NOTE_BUZZER, 80u, 100u},
    {MUSIC_NOTE_BUZZER, 80u, 100u},
};

const MusicPulse_t MUSIC_EFFECT_ERR_FATAL[] = {
    {MUSIC_NOTE_BUZZER, 500u, 100u},
    {MUSIC_NOTE_BUZZER, 80u, 100u},
};

const MusicPulse_t MUSIC_EFFECT_ERR_EMERGENCY[] = {
    {MUSIC_NOTE_BUZZER, 120u, 100u}, {2000u, 120u, 100u}, {MUSIC_NOTE_BUZZER, 120u, 100u}, {2000u, 120u, 100u},
    {MUSIC_NOTE_BUZZER, 120u, 100u}, {2000u, 120u, 100u},
};

const MusicPulse_t MUSIC_DEMO_SCALE[] = {
    {MUSIC_NOTE_C5, 150u, 0u}, {MUSIC_NOTE_D5, 150u, 0u}, {MUSIC_NOTE_E5, 150u, 0u}, {MUSIC_NOTE_F5, 150u, 0u},   {MUSIC_NOTE_G5, 150u, 0u},
    {MUSIC_NOTE_A5, 150u, 0u}, {MUSIC_NOTE_B5, 150u, 0u}, {MUSIC_NOTE_C6, 300u, 0u}, {MUSIC_NOTE_REST, 300u, 0u},
};

#endif /* LIB_MUSIC_USED */

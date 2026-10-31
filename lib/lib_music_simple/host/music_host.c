/**
 * @file music_host.c
 * @brief 主机端曲目/音效清单（PC 工具专用，不参与固件构建）
 */

#include "music_host.h"

#include <stddef.h>

#include "happy_birthday.h"
#include "music_custom.h"
#include "see_you_again_guitar.h"
#include "twinkle_star.h"

typedef struct
{
    const char *name;
    const MusicSequence_t *seq;
} MusicHostEntry_t;

static const MusicHostEntry_t s_entries[] = {
    /* 曲目 */
    {"MUSIC_TWINKLE_STAR", &MUSIC_TWINKLE_STAR},
    {"MUSIC_HAPPY_BIRTHDAY", &MUSIC_HAPPY_BIRTHDAY},
    {"MUSIC_SEE_YOU_AGAIN_GUITAR", &MUSIC_SEE_YOU_AGAIN_GUITAR},

    /* 基础音效 */
    {"MUSIC_EFFECT_SHORT", &MUSIC_EFFECT_SHORT},
    {"MUSIC_EFFECT_LONG", &MUSIC_EFFECT_LONG},
    {"MUSIC_EFFECT_ALARM", &MUSIC_EFFECT_ALARM},
    {"MUSIC_EFFECT_STARTUP", &MUSIC_EFFECT_STARTUP},

    /* 常用提示音 */
    {"MUSIC_EFFECT_TICK", &MUSIC_EFFECT_TICK},
    {"MUSIC_EFFECT_OK", &MUSIC_EFFECT_OK},
    {"MUSIC_EFFECT_ERR", &MUSIC_EFFECT_ERR},
    {"MUSIC_EFFECT_NOTIFY", &MUSIC_EFFECT_NOTIFY},
    {"MUSIC_EFFECT_POWEROFF", &MUSIC_EFFECT_POWEROFF},

    /* 报警等级（从轻到重） */
    {"MUSIC_EFFECT_ERR_MINOR", &MUSIC_EFFECT_ERR_MINOR},
    {"MUSIC_EFFECT_ERR_MAJOR", &MUSIC_EFFECT_ERR_MAJOR},
    {"MUSIC_EFFECT_ERR_CRITICAL", &MUSIC_EFFECT_ERR_CRITICAL},
    {"MUSIC_EFFECT_ERR_FATAL", &MUSIC_EFFECT_ERR_FATAL},
    {"MUSIC_EFFECT_ERR_EMERGENCY", &MUSIC_EFFECT_ERR_EMERGENCY},

    /* 调试 */
    {"MUSIC_DEMO_SCALE", &MUSIC_DEMO_SCALE},
};

#define MUSIC_HOST_ENTRY_COUNT ((uint32_t)(sizeof(s_entries) / sizeof(s_entries[0])))

uint32_t MusicHostCount(void)
{
    return MUSIC_HOST_ENTRY_COUNT;
}

const char *MusicHostName(uint32_t index)
{
    return (index < MUSIC_HOST_ENTRY_COUNT) ? s_entries[index].name : NULL;
}

const MusicSequence_t *MusicHostSequence(uint32_t index)
{
    return (index < MUSIC_HOST_ENTRY_COUNT) ? s_entries[index].seq : NULL;
}

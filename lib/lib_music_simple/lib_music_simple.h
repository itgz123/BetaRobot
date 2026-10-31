/**
 * @file lib_music_simple.h
 * @brief 音频脉冲数组与曲目结构（纯数据，不依赖任何硬件）
 *
 * @note 分层：本模块只提供「频率 + 时长 + 音量」的脉冲与一首曲子的结构体；
 *       节拍推进、音调/音量落到 PWM 由 drv_buzzer_simple 负责。
 *       这样换蜂鸣器/换驱动时，曲谱与音效数组不用跟着改。
 *
 * @note 目录约定（同目录）：
 *       - music_custom/：常用提示音/报警音效（非曲目）
 *       - twinkle_star/、happy_birthday/、see_you_again/see_you_again_guitar/：一首歌一个文件夹
 *       - 音名 → 频率的换算在 drv_buzzer_simple/drv_buzzer_note.{h,c}
 *         （曲谱数组用编译期宏 BUZZER_DEG_HZ；查表函数用于运行时换算/移调）
 *
 * @note 门控：需在 app_cfg.h 定义 LIB_MUSIC_USED。
 */

#ifndef __LIB_MUSIC_SIMPLE_H
#define __LIB_MUSIC_SIMPLE_H

#include <stdint.h>

/*============================================
 *              音频脉冲
 *============================================*/
/**
 * @brief 一个音频脉冲（一拍）
 * @note freq_hz == 0 表示休止符（静音），时长照常占用节拍。
 * @note volume_pct == 0 表示用 buzzer 实例的默认音量；1~100 覆盖为本拍音量。
 */
typedef struct
{
    uint16_t freq_hz;     // 频率（Hz），0 = 休止符
    uint16_t duration_ms; // 这一拍的时长（ms）
    uint8_t volume_pct;   // 音量百分比（1~100），0 = 用实例默认音量
} MusicPulse_t;

/* 休止符（数据层语义：freq_hz == 0） */
#define MUSIC_NOTE_REST 0u

/* 数组元素个数（结果为 uint16_t，与 MusicSequence_t.count 一致） */
#define MUSIC_PULSE_COUNT(arr) ((uint16_t)(sizeof(arr) / sizeof((arr)[0])))

/*============================================
 *              曲目（一首歌 = 一个结构体）
 *============================================*/
/**
 * @brief 一首曲目/音效：脉冲数组 + 长度
 * @note 数组与长度绑在一起传给 BuzzerPlay，避免"数组改了、长度忘了改"。
 * @note 结构体本身是 const，可直接作为 BuzzerPlay 的实参（取地址）。
 */
typedef struct
{
    const MusicPulse_t *pulses; // 脉冲数组
    uint16_t count;             // 脉冲个数
} MusicSequence_t;

#endif /* __LIB_MUSIC_SIMPLE_H */

/**
 * @file lib_music_simple.h
 * @brief 音频脉冲数组（纯数据，不依赖任何硬件）
 *
 * @note 分层：本模块只提供「频率 + 时长 + 音量」的脉冲数组与常用音名常量；
 *       节拍推进、音调/音量落到 PWM 由 drv_buzzer_simple 负责。
 *       这样换蜂鸣器/换驱动时，曲谱与音效数组不用跟着改。
 *
 * @note 门控：实现在 lib_music_simple.c，需在 app_cfg.h 定义 LIB_MUSIC_USED。
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

/* 数组元素个数（结果为 uint16_t，与 BuzzerPlay 的 num 参数一致） */
#define MUSIC_PULSE_COUNT(arr) ((uint16_t)(sizeof(arr) / sizeof((arr)[0])))

/*============================================
 *              常用音名（十二平均律，A4 = 440 Hz）
 *============================================*/
#define MUSIC_NOTE_REST 0u

#define MUSIC_NOTE_C4 262u
#define MUSIC_NOTE_CS4 277u
#define MUSIC_NOTE_D4 294u
#define MUSIC_NOTE_DS4 311u
#define MUSIC_NOTE_E4 330u
#define MUSIC_NOTE_F4 349u
#define MUSIC_NOTE_FS4 370u
#define MUSIC_NOTE_G4 392u
#define MUSIC_NOTE_GS4 416u
#define MUSIC_NOTE_A4 440u
#define MUSIC_NOTE_AS4 466u
#define MUSIC_NOTE_B4 494u

#define MUSIC_NOTE_C5 523u
#define MUSIC_NOTE_CS5 554u
#define MUSIC_NOTE_D5 587u
#define MUSIC_NOTE_DS5 622u
#define MUSIC_NOTE_E5 659u
#define MUSIC_NOTE_F5 698u
#define MUSIC_NOTE_FS5 740u
#define MUSIC_NOTE_G5 784u
#define MUSIC_NOTE_GS5 831u
#define MUSIC_NOTE_A5 880u
#define MUSIC_NOTE_AS5 932u
#define MUSIC_NOTE_B5 988u

#define MUSIC_NOTE_C6 1047u
#define MUSIC_NOTE_CS6 1109u
#define MUSIC_NOTE_D6 1175u
#define MUSIC_NOTE_DS6 1245u
#define MUSIC_NOTE_E6 1319u
#define MUSIC_NOTE_F6 1397u
#define MUSIC_NOTE_FS6 1480u
#define MUSIC_NOTE_G6 1568u
#define MUSIC_NOTE_GS6 1661u
#define MUSIC_NOTE_A6 1760u
#define MUSIC_NOTE_AS6 1865u
#define MUSIC_NOTE_B6 1976u

/* 板载蜂鸣器的谐振点（CubeMX 给 TIM_BUZZER 配的 ~4 kHz）：
 * 提示音用它最响，旋律用上面的音名（会明显小声，属于器件特性） */
#define MUSIC_NOTE_BUZZER 4000u

/*============================================
 *              默认音效（末尾留静音，上层离线期间循环触发才有间隙）
 *============================================*/
/* 长度显式写在声明里：定义在 .c 中、声明在头里，若写成 []，
 * 别处的 MUSIC_PULSE_COUNT 会对不完整类型取 sizeof（编译错误） */
extern const MusicPulse_t MUSIC_EFFECT_SHORT[2];   // 短叫：一声
extern const MusicPulse_t MUSIC_EFFECT_LONG[2];    // 长叫：一声长音
extern const MusicPulse_t MUSIC_EFFECT_ALARM[6];   // 断续报警：三短音
extern const MusicPulse_t MUSIC_EFFECT_STARTUP[5]; // 开机音：上行琶音
extern const MusicPulse_t MUSIC_DEMO_SCALE[9];     // 调试用：C5 音阶

/* 常用提示音（单次播放；相邻同音调靠 drv 的衰减包络分开，见 Buzzer_Config_s.decay_ms） */
extern const MusicPulse_t MUSIC_EFFECT_TICK[1];     // 按键/单次提示：一声短促
extern const MusicPulse_t MUSIC_EFFECT_OK[2];       // 完成/确认：上行两音 "叮咚"
extern const MusicPulse_t MUSIC_EFFECT_ERR[2];      // 失败/错误：低音两声
extern const MusicPulse_t MUSIC_EFFECT_NOTIFY[3];   // 提醒：上行三连音
extern const MusicPulse_t MUSIC_EFFECT_POWEROFF[3]; // 关机：下行三连音

/* 错误音（按报警等级从轻到重；统一用蜂鸣器谐振点，保证最响） */
extern const MusicPulse_t MUSIC_EFFECT_ERR_MINOR[1];     // 轻微：一声短促
extern const MusicPulse_t MUSIC_EFFECT_ERR_MAJOR[2];     // 一般：两声
extern const MusicPulse_t MUSIC_EFFECT_ERR_CRITICAL[3];  // 严重：三声
extern const MusicPulse_t MUSIC_EFFECT_ERR_FATAL[2];     // 致命：长鸣 + 短促
extern const MusicPulse_t MUSIC_EFFECT_ERR_EMERGENCY[6]; // 紧急：高低交替三组

#endif /* __LIB_MUSIC_SIMPLE_H */

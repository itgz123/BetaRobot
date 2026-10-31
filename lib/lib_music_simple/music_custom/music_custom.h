/**
 * @file music_custom.h
 * @brief 常用提示音与报警音效（非完整曲目）
 *
 * @note 一首音效 = 一个 MusicSequence_t；直接取地址传给 BuzzerPlay。
 * @note 报警音效统一用板载蜂鸣器的谐振点（~4 kHz，CubeMX 给 TIM_BUZZER 配的频率），
 *       提示音才用音名（旋律频段会明显小声，属于器件特性）。
 * @note 末尾留休止符的几个音效，是为了在 daemon「离线期间每拍重复触发」时
 *       自然产生循环间隔；单次播放的音效末尾不留休止。
 *
 * @note 门控：实现在 music_custom.c，需在 app_cfg.h 定义 LIB_MUSIC_USED。
 */

#ifndef __MUSIC_CUSTOM_H
#define __MUSIC_CUSTOM_H

#include "lib_music_simple.h"

/* ---------------- 基础音效 ---------------- */
extern const MusicSequence_t MUSIC_EFFECT_SHORT;   // 短叫：一声
extern const MusicSequence_t MUSIC_EFFECT_LONG;    // 长叫：一声长音
extern const MusicSequence_t MUSIC_EFFECT_ALARM;   // 断续报警：三短音
extern const MusicSequence_t MUSIC_EFFECT_STARTUP; // 开机音：上行琶音

/* ---------------- 常用提示音（单次播放） ---------------- */
extern const MusicSequence_t MUSIC_EFFECT_TICK;     // 按键/单次提示：一声短促
extern const MusicSequence_t MUSIC_EFFECT_OK;       // 完成/确认：上行两音 "叮咚"
extern const MusicSequence_t MUSIC_EFFECT_ERR;      // 失败/错误：低音两声
extern const MusicSequence_t MUSIC_EFFECT_NOTIFY;   // 提醒：上行三连音
extern const MusicSequence_t MUSIC_EFFECT_POWEROFF; // 关机：下行三连音

/* ---------------- 错误音（按报警等级从轻到重） ---------------- */
extern const MusicSequence_t MUSIC_EFFECT_ERR_MINOR;     // 轻微：一声短促
extern const MusicSequence_t MUSIC_EFFECT_ERR_MAJOR;     // 一般：两声
extern const MusicSequence_t MUSIC_EFFECT_ERR_CRITICAL;  // 严重：三声
extern const MusicSequence_t MUSIC_EFFECT_ERR_FATAL;     // 致命：长鸣 + 短促
extern const MusicSequence_t MUSIC_EFFECT_ERR_EMERGENCY; // 紧急：高低交替三组

/* ---------------- 调试 ---------------- */
extern const MusicSequence_t MUSIC_DEMO_SCALE; // 音阶：C5~C6（验证音高）

#endif /* __MUSIC_CUSTOM_H */

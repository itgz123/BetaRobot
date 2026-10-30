/* 小星星（1 = C5，4/4 拍，BPM 100）
 * 42 个脉冲，约 28.8 s；曲谱见 see_you_again 同级的源文件注释
 * 用途：简单旋律，便于验证蜂鸣器音高/衰减包络 */
#ifndef __MUSIC_TWINKLE_STAR_H
#define __MUSIC_TWINKLE_STAR_H

#include "lib_music_simple.h"

extern const MusicPulse_t MUSIC_TWINKLE_STAR[42];
#define MUSIC_TWINKLE_STAR_COUNT 42u /* = MUSIC_PULSE_COUNT(MUSIC_TWINKLE_STAR) */

#endif /* __MUSIC_TWINKLE_STAR_H */

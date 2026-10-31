/**
 * @file lib_music_simple.c
 * @brief 音频脉冲数组的布局自检（数据本体在 music_custom/ 与各曲目目录）
 *
 * @note 曲谱数组按 {freq_hz, duration_ms, volume_pct} 的内存布局初始化，
 *       （生成脚本/手写数据都假定 6 字节、无填充）。布局一旦变化，
 *       所有已生成的曲谱必须重新生成，这里用编译期断言把它挡在构建阶段。
 */

#include "lib_music_simple.h"

typedef char lib_music_simple_pulse_layout_check[(sizeof(MusicPulse_t) == 6u) ? 1 : -1];

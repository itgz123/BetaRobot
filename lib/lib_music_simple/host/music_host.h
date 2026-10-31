/**
 * @file music_host.h
 * @brief 主机端枚举 lib_music_simple 曲目/音效的 C 接口
 *
 * @note 这是 PC 工具（gcc -shared）用的包装层，不参与固件构建。
 *       每个条目返回真实的 MusicSequence_t 指针，Python(ctypes) 直接读取，
 *       不重新转写曲谱，保证验证的就是固件里那份数组。
 */

#ifndef __MUSIC_HOST_H
#define __MUSIC_HOST_H

#include <stdint.h>

#include "lib_music_simple.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief 可用曲目/音效条目数 */
    uint32_t MusicHostCount(void);

    /**
     * @brief 条目名称（如 "MUSIC_TWINKLE_STAR"）
     * @param index 0 ~ MusicHostCount()-1
     * @return 名称；越界返回 NULL
     */
    const char *MusicHostName(uint32_t index);

    /**
     * @brief 条目对应的曲目结构
     * @param index 0 ~ MusicHostCount()-1
     * @return 曲目结构（pulses + count）；越界返回 NULL
     */
    const MusicSequence_t *MusicHostSequence(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* __MUSIC_HOST_H */

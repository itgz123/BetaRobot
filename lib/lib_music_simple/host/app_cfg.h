/**
 * @file app_cfg.h
 * @brief 主机端（PC）编译用的 app_cfg 桩，仅本目录使用
 *
 * @note 曲谱源码会 #include "app_cfg.h"，只检查 LIB_MUSIC_USED。
 *       这里不定义任何硬件宏，也**不要**把这个目录加进固件的 include 路径。
 */

#ifndef __MUSIC_HOST_APP_CFG_H
#define __MUSIC_HOST_APP_CFG_H

#define LIB_MUSIC_USED

#endif /* __MUSIC_HOST_APP_CFG_H */

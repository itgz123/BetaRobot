/**
 * @file drv_vofa_lite.h
 * @brief VOFA+ JustFloat 协议简化版驱动（无任务，外部调用）
 *
 * @note 通过宏 VOFA_LITE_USED 控制是否启用，未启用时所有函数为空实现
 *       编译时固定通道数，无动态注册，适合高频调用场景（500Hz-1kHz）
 *       ch=0 固定为 DWT 时间戳（自动填充），用户数据从 ch=1 开始
 */

#ifndef __DRV_VOFA_LITE_H
#define __DRV_VOFA_LITE_H

#include <stdint.h>
#include "app_cfg.h"

/*------------- 配置宏（可被 app_cfg.h 覆盖）--------------*/

// 用户通道数量（不含时间戳，编译时固定）
#ifndef VOFA_LITE_CHANNELS
#define VOFA_LITE_CHANNELS 15
#endif

// UART 选择（独立于 drv_vofa.c）
#ifndef VOFA_LITE_UART
#if DEVELOPMENT_BOARD == DM_MC02
#define VOFA_LITE_UART UART_7
#elif DEVELOPMENT_BOARD == DJI_C
#error "Please define VOFA_LITE_UART for this board"
#elif DEVELOPMENT_BOARD == DJI_A
#error "Please define VOFA_LITE_UART for this board"
#else
#error "Please define VOFA_LITE_UART for this board"
#endif
#endif // !VOFA_LITE_UART

/*------------- API 声明 --------------*/

/**
 * @brief 初始化 VOFA Lite 驱动
 * @note 注册串口实例，初始化数据缓冲区
 */
void VofaLiteInit(void);

/**
 * @brief 设置单个通道数据
 * @param ch    通道编号（1 ~ VOFA_LITE_CHANNELS）
 *              ch=0 为时间戳通道，自动填充，用户无需设置
 * @param value 数据值
 */
void VofaLiteSetChannel(uint8_t ch, float value);

/**
 * @brief 发送一帧数据
 * @note 在控制任务中调用，发送前检查串口状态，忙则跳过
 *       ch=0 自动填充 DWT 时间戳，用户数据从 ch=1 开始
 */
void VofaLiteSend(void);

#endif // __DRV_VOFA_LITE_H
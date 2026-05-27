/**
 * @file drv_vofa.h
 * @brief VOFA+ JustFloat 协议示波器驱动（单例模式）
 *
 * @note 通过宏 VOFA_USED 控制是否启用，未启用时所有函数为空实现
 *       使用单例模式，内部维护唯一实例，用户只需调用 VofaInit() 和 VofaAddChannel()
 *       根据 VOFA_USE_DWT_TIME_STAMP 宏决定是否将 DWT 时间戳作为第一通道
 */

#ifndef __DRV_VOFA_H
#define __DRV_VOFA_H

#include <stdint.h>
#include "app_cfg.h"

/*------------- 配置宏（可被 app_cfg.h 覆盖）--------------*/

#ifndef VOFA_FREQ_MS
#define VOFA_FREQ_MS 1 // 发送周期 1ms (1000Hz)
#endif

#ifndef VOFA_STACK_SIZE
#define VOFA_STACK_SIZE 256 // 任务栈大小（字）
#endif

// 921600下，1ms发送92.16字节，92.16/4=23.04通道，减1个结束通道，还有22.04通道
// 若启用时间戳，还需减1个通道
#ifndef VOFA_MAX_CHANNEL
#define VOFA_MAX_CHANNEL 20 // 最大用户通道数
#endif

#ifndef VOFA_TASK_PRIORITY
#define VOFA_TASK_PRIORITY 0 // 任务优先级
#endif

#ifndef VOFA_USE_DWT_TIME_STAMP
#define VOFA_USE_DWT_TIME_STAMP 1 // 使用 DWT 时间戳作为第一通道
#endif

#ifndef VOFA_UART
#if DEVELOPMENT_BOARD == DM_MC02
#define VOFA_UART
#elif DEVELOPMENT_BOARD == DJI_C
#error "Please define VOFA_LITE_UART for this board"
#elif DEVELOPMENT_BOARD == DJI_A
#error "Please define VOFA_LITE_UART for this board"
#elif DEVELOPMENT_BOARD == STM32F407VET6
#error "Please define VOFA_LITE_UART for this board"
#endif
#endif // !VOFA_UART

/*------------- API 声明 --------------*/

/**
 * @brief 初始化 VOFA 驱动
 * @note 内部创建单例实例、注册串口、创建周期发送任务
 *       若 VOFA_USE_DWT_TIME_STAMP=1，时间戳通道自动添加为第一通道
 */
void VofaInit(void);

/**
 * @brief 添加数据通道
 * @param data_ptr  数据指针（指向 float 变量）
 * @param name      通道名称（可为 NULL）
 * @return 通道编号（0 ~ VOFA_MAX_CHANNEL-1），失败返回 -1
 * @note 若启用时间戳，用户通道从编号 0 开始，时间戳不计入用户通道编号
 */
int8_t VofaAddChannel(float *data_ptr, const char *name);

#endif // __DRV_VOFA_H
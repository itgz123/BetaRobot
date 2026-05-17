#ifndef __DRV_DAEMON_H
#define __DRV_DAEMON_H

#include <stdint.h>
#include "app_cfg.h"

#ifdef DAEMON_USED

#define USE_DAEMON

#ifndef DAEMON_FREQ_MS
#define DAEMON_FREQ_MS 1
#endif

#ifndef DAEMON_STACK_SIZE
#define DAEMON_STACK_SIZE 512
#endif

#ifndef DAEMON_MX_CNT
#define DAEMON_MX_CNT 64
#endif

/* 模块离线处理函数指针 */
typedef void (*offline_callback)(void *);

/* daemon结构体定义 */
typedef struct daemon_ins
{
    uint16_t reload_count;     // 重载值
    offline_callback callback; // 异常处理函数,当模块发生异常时会被调用
    uint16_t temp_count;       // 当前值,减为零说明模块离线或异常
    void *owner_id;            // daemon实例的地址,初始化的时候填入
    uint8_t is_online;         // 当前在线状态,用于检测状态转换
} DaemonInstance;

/* daemon初始化配置 */
typedef struct
{
    uint16_t reload_count;     // 喂狗重载值
    uint16_t init_count;       // 上线等待时间
    offline_callback callback; // 异常处理函数,当模块发生异常时会被调用
    void *owner_id;            // id取拥有daemon的实例的地址,如DJIMotorInstance*,cast成void*类型
} Daemon_Init_Config_s;

void DaemonRegister(DaemonInstance *inst, Daemon_Init_Config_s *config);
void DaemonReload(DaemonInstance *instance);
uint8_t DaemonIsOnline(DaemonInstance *instance);
void DaemonTask(void);
void DaemonInit(uint32_t priority);

#endif // DAEMON_USED

#endif // !__DRV_DAEMON_H

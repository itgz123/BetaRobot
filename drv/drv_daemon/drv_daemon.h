#ifndef __DRV_DAEMON_H
#define __DRV_DAEMON_H

#include <stdint.h>
#include "app_cfg.h"

#ifndef DAEMON_FREQ_MS
#define DAEMON_FREQ_MS 1
#endif

#ifndef DAEMON_STACK_SIZE
#define DAEMON_STACK_SIZE 512
#endif

#ifndef DAEMON_MX_CNT
#define DAEMON_MX_CNT 64
#endif

#ifndef DAEMON_TASK_PRIORITY
#define DAEMON_TASK_PRIORITY 0
#endif

/* 离线故障动作枚举（8种, 占uint8_t） */
typedef enum
{
    DAEMON_FAULT_NONE = 0,         // 无动作
    DAEMON_FAULT_BUZZER_SHORT = 1, // 蜂鸣器短叫
    DAEMON_FAULT_BUZZER_LONG = 2,  // 蜂鸣器长叫
    DAEMON_FAULT_LIGHT_SHORT = 3,  // 故障灯短闪
    DAEMON_FAULT_LIGHT_LONG = 4,   // 故障灯长闪
    DAEMON_FAULT_RESERVED_5 = 5,   // 预留
    DAEMON_FAULT_RESERVED_6 = 6,   // 预留
    DAEMON_FAULT_RESERVED_7 = 7,   // 预留
} DaemonFaultAction_e;

/* 模块离线处理函数指针 */
typedef void (*offline_callback)(void *);

/* daemon结构体定义 */
typedef struct daemon_ins
{
    uint16_t reload_count;     // 重载值
    uint8_t fault_action;      // 离线故障动作, 见 DaemonFaultAction_e
    offline_callback callback; // 异常处理函数,当模块发生异常时会被调用
    uint16_t temp_count;       // 当前值,减为零说明模块离线或异常
    void *owner_id;            // daemon实例的地址,初始化的时候填入
    uint8_t is_online;         // 当前在线状态,用于检测状态转换
} DaemonInstance;

/*------------- 配置结构体 --------------*/

typedef struct
{
    uint16_t reload_count;     // 重载值（喂狗超时阈值）
    uint8_t fault_action;      // 离线故障动作, 见 DaemonFaultAction_e
    offline_callback callback; // 异常处理函数（可为NULL）
    void *owner_id;            // 所属模块实例指针
} Daemon_Init_Config_s;

/*------------- 实例定义宏 --------------*/

#define DAEMON_INSTANCE_DEF(name)          \
    static DaemonInstance name = {         \
        .reload_count = 0,                 \
        .fault_action = DAEMON_FAULT_NONE, \
        .temp_count = 0,                   \
        .is_online = 1,                    \
        .callback = NULL,                  \
        .owner_id = NULL,                  \
    }

void DaemonRegister(DaemonInstance *inst, const Daemon_Init_Config_s *config);
void DaemonReload(DaemonInstance *instance);
uint8_t DaemonIsOnline(DaemonInstance *instance);
void DaemonTask(void);
void DaemonInit(void);

#endif // !__DRV_DAEMON_H

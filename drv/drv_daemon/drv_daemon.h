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

#define DAEMON_INSTANCE_DEF(name, reload) \
    static DaemonInstance name = {        \
        .reload_count = reload,           \
        .temp_count = reload,             \
        .is_online = 1,                   \
    }

void DaemonRegister(DaemonInstance *inst);
void DaemonReload(DaemonInstance *instance);
uint8_t DaemonIsOnline(DaemonInstance *instance);
void DaemonTask(void);
void DaemonInit(uint32_t priority);

#endif // !__DRV_DAEMON_H

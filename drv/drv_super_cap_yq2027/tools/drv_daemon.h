/**
 * @file drv_daemon.h
 * @brief 离线测试用的 daemon API 桩
 */

#ifndef __TEST_DRV_DAEMON_H
#define __TEST_DRV_DAEMON_H

#include <stdint.h>

typedef enum : uint8_t
{
    DAEMON_FAULT_NONE = 0,
    DAEMON_FAULT_BUZZER_SHORT = 1,
    DAEMON_FAULT_BUZZER_LONG = 2,
    DAEMON_FAULT_LIGHT_SHORT = 3,
    DAEMON_FAULT_LIGHT_LONG = 4,
    DAEMON_FAULT_RESERVED_5 = 5,
    DAEMON_FAULT_RESERVED_6 = 6,
    DAEMON_FAULT_RESERVED_7 = 7,
    DAEMON_FAULT_NUM,
} DaemonFaultAction_e;

typedef void (*offline_callback)(void *);

typedef struct daemon_ins
{
    uint16_t reload_count;
    DaemonFaultAction_e fault_action;
    offline_callback callback;
    uint16_t temp_count;
    void *owner_id;
    uint8_t is_online;
    uint64_t last_reload_us;
} DaemonInstance;

typedef struct
{
    uint16_t reload_count;
    DaemonFaultAction_e fault_action;
    offline_callback callback;
    void *owner_id;
} Daemon_Config_s;

#define DAEMON_INSTANCE_DEF(name) static DaemonInstance name = {.reload_count = 0, .fault_action = DAEMON_FAULT_NONE, .is_online = 1}

/* 测试记录 */
typedef struct
{
    int register_calls;
    int config_calls;
    int reload_calls;
    Daemon_Config_s last_config;
    uint8_t online_ret;
} TestDaemonState;

extern TestDaemonState g_daemon;

void DaemonConfig(DaemonInstance *inst, const Daemon_Config_s *config);
void DaemonRegister(DaemonInstance *inst);
void DaemonReload(DaemonInstance *instance);
uint8_t DaemonIsOnline(DaemonInstance *instance);

#endif /* __TEST_DRV_DAEMON_H */

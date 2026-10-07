/**
 * @file drv_daemon.h
 * @brief PC 端测试桩（仅供 drv/drv_referee2026/tools 的 gcc 测试使用）
 *
 * 类型与签名逐字抄自 drv/drv_daemon/drv_daemon.h（只省掉注释），实现是
 * test_referee2026.c 里的记录桩：不跑任务、不做超时判断，只记下"配了什么、喂过几次"。
 * 看门狗**逻辑**不是本模块的东西，这里不重复实现。**不要**把本目录加进固件的 include 路径。
 */

#ifndef __DRV_DAEMON_PC_STUB_H
#define __DRV_DAEMON_PC_STUB_H

#include <stddef.h>
#include <stdint.h>

/** 离线故障动作枚举（真值见 drv_daemon.h） */
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

/** 模块离线处理函数指针 */
typedef void (*offline_callback)(void *);

/** daemon 结构体定义（字段与真的一致） */
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

/** daemon 配置结构体 */
typedef struct
{
    uint16_t reload_count;
    DaemonFaultAction_e fault_action;
    offline_callback callback;
    void *owner_id;
} Daemon_Config_s;

/** 静态定义 daemon 实例 */
#define DAEMON_INSTANCE_DEF(name)                                                                                      \
    static DaemonInstance name = {                                                                                     \
        .reload_count = 0,                                                                                             \
        .fault_action = DAEMON_FAULT_NONE,                                                                             \
        .temp_count = 0,                                                                                               \
        .is_online = 1,                                                                                                \
        .callback = NULL,                                                                                              \
        .owner_id = NULL,                                                                                              \
        .last_reload_us = 0,                                                                                           \
    }

void DaemonConfig(DaemonInstance *inst, const Daemon_Config_s *config);
void DaemonRegister(DaemonInstance *inst);
void DaemonReload(DaemonInstance *instance);
uint8_t DaemonIsOnline(DaemonInstance *instance);

#endif /* __DRV_DAEMON_PC_STUB_H */

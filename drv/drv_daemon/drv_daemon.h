#ifndef __DRV_DAEMON_H
#define __DRV_DAEMON_H

#include <stdint.h>
#include "app_cfg.h"

/* app 侧 daemon 任务的建议调用周期（ms）。本模块不创建任务，由 app 决定实际周期 */
#ifndef DAEMON_FREQ_MS
#define DAEMON_FREQ_MS 1
#endif

#ifndef DAEMON_MX_CNT
#define DAEMON_MX_CNT 64
#endif

/* 离线故障动作枚举：5 种蜂鸣器报警等级（从轻到重，占 uint8_t） */
typedef enum : uint8_t
{
    DAEMON_FAULT_NONE = 0,             // 无动作
    DAEMON_FAULT_BUZZER_MINOR = 1,     // 轻微：一声短促
    DAEMON_FAULT_BUZZER_MAJOR = 2,     // 一般：两声
    DAEMON_FAULT_BUZZER_CRITICAL = 3,  // 严重：三声
    DAEMON_FAULT_BUZZER_FATAL = 4,     // 致命：长鸣 + 短促
    DAEMON_FAULT_BUZZER_EMERGENCY = 5, // 紧急：高低交替三组
    DAEMON_FAULT_NUM,                  // 动作数量
} DaemonFaultAction_e;

/* 模块离线处理函数指针 */
typedef void (*offline_callback)(void *);

/**
 * @brief 故障动作执行函数指针（无参数、无返回值）
 * @note daemon 只认识这个签名，不认识蜂鸣器/灯；
 *       app 自己写无参函数、在函数体里访问自己的蜂鸣器实例
 *       （如 `static void AlarmMinor(void) { BuzzerPlay(&g_buzzer, ...); }`）。
 */
typedef void (*daemon_action_fn)(void);

/**
 * @brief 故障动作表，按 DaemonFaultAction_e 索引
 * @note 元素为 NULL 表示该故障不产生动作；
 *       daemon 不引用任何具体设备模块，具体实现由 app 填。
 */
typedef struct
{
    daemon_action_fn action[DAEMON_FAULT_NUM];
} Daemon_ActionTable_s;

/* daemon结构体定义 */
typedef struct daemon_ins
{
    uint16_t reload_count;            // 喂狗超时阈值（毫秒）；0 = 禁用（不监控，等效恒在线）
    DaemonFaultAction_e fault_action; // 离线故障动作, 见 DaemonFaultAction_e
    offline_callback callback;        // 异常处理函数,当模块发生异常时会被调用
    uint16_t temp_count;              // 剩余量(ms，向上取整)：由 DaemonTask 按实耗时间换算。
                                      // == 0 ⟺ 距上次喂狗已满 reload_count 毫秒（与 DaemonTask 的离线判定等价）
    void *owner_id;                   // daemon实例的地址,初始化的时候填入
    uint8_t is_online;                // 当前在线状态,用于检测状态转换
    uint64_t last_reload_us;          // 上次喂狗时间戳 (us)，由 DWT 获取；离线判据的基准
} DaemonInstance;

/*------------- 配置结构体 --------------*/

typedef struct
{
    uint16_t reload_count;            // 喂狗超时阈值（毫秒）
    DaemonFaultAction_e fault_action; // 离线故障动作, 见 DaemonFaultAction_e
    offline_callback callback;        // 异常处理函数（可为NULL）
    void *owner_id;                   // 所属模块实例指针
} Daemon_Config_s;

/*------------- 实例定义宏 --------------*/

#define DAEMON_INSTANCE_DEF(name)                                                                                                                              \
    static DaemonInstance name = {                                                                                                                             \
        .reload_count = 0,                                                                                                                                     \
        .fault_action = DAEMON_FAULT_NONE,                                                                                                                     \
        .temp_count = 0,                                                                                                                                       \
        .is_online = 1,                                                                                                                                        \
        .callback = NULL,                                                                                                                                      \
        .owner_id = NULL,                                                                                                                                      \
        .last_reload_us = 0,                                                                                                                                   \
    }

void DaemonConfig(DaemonInstance *inst, const Daemon_Config_s *config);
void DaemonRegister(DaemonInstance *inst);
void DaemonReload(DaemonInstance *instance);

/**
 * @brief 查询模块是否在线（非阻塞）
 * @param instance daemon 实例
 * @retval 1 在线：reload_count 毫秒内喂过狗（见 temp_count 的等价说明）
 * @retval 0 离线：距上次喂狗已满 reload_count 毫秒；或 instance == NULL
 *
 * @note reload_count == 0（禁用监控）恒返回 1（等效恒在线），与"没监控就不该拦控制"的
 *       调用惯例一致（如 app_chassis 的 GimbalCommIsOnline）；DAEMON_USED 未启用时同样返回 1。
 * @note 判据与 DaemonTask 的离线判定严格等价（同一阈值、同一时间基准），不会出现
 *       "查询说离线、故障动作还没触发"的错位。
 */
uint8_t DaemonIsOnline(DaemonInstance *instance);

/**
 * @brief daemon 周期主体：遍历所有实例判离线、触发故障动作/回调
 * @note 本模块不创建任务（drv 层不依赖 FreeRTOS）。由 app 自己的周期任务调用，
 *       建议周期 DAEMON_FREQ_MS（1ms）。
 * @note 判离线用的是"距上次喂狗的实耗时间"，与调用次数无关：app 任务偶尔落后
 *       一两拍不会连带把刚喂过狗的实例误判离线，只是最坏检测时延随周期增大。
 */
void DaemonTask(void);

/**
 * @brief 初始化 daemon，并登记故障动作表
 * @param table 动作表（需静态存储期，本模块只保存指针、不拷贝），可为 NULL
 * @note daemon 不知道蜂鸣器/灯的存在：具体动作由 app 填无参函数决定，
 *       因此 daemon 侧不依赖 bsp_tim，也不依赖任何 drv 设备模块。
 * @note 在 app 初始化流程里、调度器启动前调用；**不创建任何任务**，
 *       周期调用 DaemonTask() 由 app 自行注册任务完成。
 */
void DaemonInit(const Daemon_ActionTable_s *table);

#endif // !__DRV_DAEMON_H

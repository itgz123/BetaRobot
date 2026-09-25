/**
 * @file comm_media.h
 * @brief 通信框架-硬件层（Media）基类
 *
 * 职责：把物理介质抽象为"可发送任意长度数据单元"的统一通道。
 *   - 发送：vtable->send(data)，data 任意长度数据单元
 *   - 接收：后端适配钩子把收到的数据单元直接交给 comm 层接收入口
 *           （CommMediaRxHook，经 parent 反查），media 基类不做接收分发
 *   - 归属：一个 media 只属于一个 comm 实例（经 parent 反查）
 *   - 看门狗：基类持有 daemon 指针（DEF 宏内嵌 name##_daemon），监控链路对端是否持续发帧——
 *             收到完整合法帧即在 comm 层接收入口（CommMediaRxHook）喂狗；阈值由 CommConfig
 *             统一配置（后端挂了 offline 钩子时，配 0 会被提升为默认值——离线自恢复就搭在
 *             这个回调上；没挂钩子的后端配 0 即字面意义上的不监控）。
 * 基类不解析内容（SOF/CRC/comm_id 属协议层），不感知介质物理限制（属后端）。
 */

#ifndef COMM_MEDIA_H
#define COMM_MEDIA_H

#include <stdint.h>
#include "app_cfg.h"
#include "drv_daemon.h" /* DaemonInstance：链路对端看门狗（drv 层共享，无环） */
/* 依赖单向：drv_comm.h → comm_media.h；本头自包含，勿 include drv_comm.h（防环） */

/* 介质类型枚举（COMM_DEF / 后端 Config 用） */
typedef enum : uint8_t
{
    MEDIA_CAN = 0, /* 旧占位：无实现；保留为 0 使未初始化 media_type 落 default 大声失败 */
    MEDIA_USART,
    MEDIA_USB,
    MEDIA_USB_SIMPLE, /* USB(CDC) 短帧免序号版：整帧 ≤ 63B 免分包序号单包透传；> 63B 行为同 MEDIA_USB */
    MEDIA_CAN_PKT0,   /* 经典 CAN 8B / FD 64B 分包（按 mode）：data[0]=分包序号，data[1..n]=数据片 */
    MEDIA_CAN_IDSEQ,  /* 经典 CAN 8B / FD 64B 分包（按 mode）：CAN ID 段内偏移为分包序号（id = base_id + seq） */
} MediaType_e;

typedef struct CommMedia CommMedia;

/* 虚函数表（派生结构体首成员为 vtable，drv_motor_base 约定） */
typedef struct
{
    int8_t (*send)(CommMedia *self, const uint8_t *data); /* 发送任意长度数据单元 */

    /* 链路离线自恢复（可为 NULL）：daemon 判离线时在 DaemonTask（任务上下文）被调用，
     * 参数为 CommConfig 写入的 owner_id（comm 实例）。
     * 用途：介质收到数据才喂狗，故"离线"是介质唯一能拿到的任务上下文周期时基。
     * 各后端据此在**任务上下文**做那些 ISR 里做不了的事：
     *   - USART：重启停摆的接收（ISR 里不能重启，见 bsp_usart 的 Abort 约束）；
     *   - USB / USB_SIMPLE：发送卡死收尾 + 接收重新武装。
     * 后端须自行判"是链路没数据（接收正常）还是接收停摆"，并对重复调用限频。
     * @note **挂上钩子 = 放弃 `daemon_reload = 0` 这个退出通道**：CommConfig 对挂了钩子的
     *       后端会把 0 提升为默认值（禁用监控就等于把自恢复一起禁掉）。故只应在"自恢复
     *       的判据就是本实例的收发状态"时挂它。
     * @note **两个 CAN 后端（CAN_PKT0 / CAN_IDSEQ）刻意不挂**：它们的自恢复动作 CANRecover
     *       作用于**整条总线**（取消该总线上所有实例的在途帧、必要时重停外设），而本钩子的
     *       触发条件是"这条链路没收到帧"= **实例级**证据。证据的作用域必须与动作的作用域
     *       对齐，否则对端不发 / 滤波器不匹配 / 流量被同总线别的实例挤掉，都会误伤整条总线。
     *       它们改把自己的发送入口当作任务上下文时基（每帧必经、且"发不出去"正是要治的病），
     *       总线级判据由 bsp 在 CANRecover 入口内部自证——见 comm_media_can_pkt0.c 的
     *       ProbeBus 与 bsp_can.h 的 CANRecover 契约。 */
    offline_callback offline;
} CommMediaVTable_s;

/* 介质基类（派生结构体内嵌作首成员） */
struct CommMedia
{
    const CommMediaVTable_s *vtable; /* 必须首成员 */
    void *parent;                    /* 指向 comm 实例（接收分发经此反查） */
    void *media;                     /* 指向 bsp 实例 */
    DaemonInstance *daemon;          /* 链路对端看门狗实例（DEF 宏内嵌 name##_daemon 并绑定）：
                                      * 收到完整合法帧即在 CommMediaRxHook 喂狗；阈值由 CommConfig
                                      * 统一配置（挂了 vtable->offline 时配 0 会被提升为默认值，
                                      * 见 CommConfig）。NULL 表示不监控 */
};

/* 公共接口 */
int8_t MediaSend(CommMedia *media, const uint8_t *data); /* 统一发送分发（vtable 转发；发送缓冲/长度由各后端自持：USART 拷入自持 staging，USB 直接引用 data 分包） */

#endif /* COMM_MEDIA_H */

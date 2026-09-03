#ifndef DRV_COMM_H
#define DRV_COMM_H

#include <stdint.h>
#include "app_cfg.h"
//
#include "comm_media.h"
#include "comm_proto.h"

/* 协议解包位置枚举（接收路径分流：ISR 直解 / 任务解包，COMM_DEF 编译期写入） */
typedef enum : uint8_t
{
    UNPACK_IN_ISR = 0,  /* 在 ISR 上下文直接解包（低延迟，回调须短小） */
    UNPACK_IN_TASK = 1, /* 搬入接收队列，由 RX 任务解包（不阻塞中断） */
} UnpackMode_e;

/**
 * @brief media 接收统一入口（media 后端适配钩子直接调用）
 * @param media CommMedia 指针（经 media->parent 反查所属 CommInstance）
 * @param data  收到的完整数据单元指针（长度已由后端校验，协议层拿到的总是完整一帧）
 * @note 跳过 media 基类分发：接收链 = bsp → media 子类钩子 → 本函数 → rx_proto unpack。
 */
void CommMediaRxHook(CommMedia *media, const uint8_t *data);

typedef struct
{
    MediaType_e media_type;       // 介质类型（定义时写入）
    ProtocolType_e rx_proto_type; // 接收协议类型（定义时写入）
    ProtocolType_e tx_proto_type; // 发送协议类型（定义时写入）
    UnpackMode_e unpack_mode;     // 接收解包位置（定义时写入）
    void *media;                  // 介质派生实例指针（首成员为 CommMedia 基类）
    void *rx_proto;               // 接收协议派生实例指针（首成员为 CommProto 基类）
    void *tx_proto;               // 发送协议派生实例指针（首成员为 CommProto 基类）
    uint8_t *tx_buff;             // 发送打包缓冲（COMM_DEF 静态定义，协议分包写入；大小 = tx_size + 协议开销）
    uint8_t inited;               // 初始化标志（CommRegister 置位）
} CommInstance;

/**
 * @brief comm 运行时配置（CommConfig 传入；可重入，可反复调用修改）
 */
typedef struct
{
    void *media_cfg;                  /* 介质后端配置指针（USART → USART_Config_s*；NULL 跳过介质配置） */
    ProtoFrameCallback on_frame;      /* 出帧消费回调（NULL 表示不修改） */
    uint16_t daemon_reload;           /* 链路对端看门狗重载值（单位：daemon 周期，默认 ms；0 = 禁用不监控，恒在线） */
    DaemonFaultAction_e daemon_fault; /* 链路对端离线故障动作（见 DaemonFaultAction_e） */
} CommConfig_s;

/**
 * @brief 注册 comm 实例（不可重入：仅可调用一次）
 * @param inst CommInstance 指针（COMM_DEF 定义）
 * @retval 0 成功；-1 失败（参数非法 / 后端注册失败 / 类型未支持）
 *
 * @note 完成：1) media 后端注册（USART 内部做 USARTRegister，防重复注册）；
 *       2) 接收/发送协议后端 Init（挂 vtable）；3) 接线：media 接管接收钩子 +
 *       建立 media→comm 反向指针（接收时经 media->parent 反查 rx_proto 解包）；
 *       4) 登记链路对端看门狗（media->daemon，DEF 宏内嵌；armed 由 CommConfig 决定）。
 *       介质参数、看门狗参数与出帧回调由 CommConfig 配置。
 */
int8_t CommRegister(CommInstance *inst);

/**
 * @brief 配置 comm 实例（可重入：可反复调用改介质参数 / 看门狗 / 出帧回调）
 * @param inst CommInstance 指针（须先 CommRegister）
 * @param cfg  CommConfig_s 配置（media_cfg / on_frame 均可为 NULL，NULL 则跳过对应项；
 *            daemon_reload/daemon_fault 为标量，每次调用都会写入，0 表示禁用监控）
 * @retval 0 成功；-1 失败（参数非法 / 未注册 / 配置失败 / 类型未支持）
 *
 * @note media_cfg 经 media 后端下发（USART → MediaUsartConfig → bsp USARTConfig），
 *       运行中可再次调用以切换波特率/发送模式等；daemon_reload>0 启用链路对端看门狗
 *       （收到完整合法帧喂狗，见 CommIsOnline）；on_frame 直接覆盖挂到接收协议
 *       rx_proto->on_frame，可运行期修改消费逻辑。
 */
int8_t CommConfig(CommInstance *inst, const CommConfig_s *cfg);

/**
 * @brief 统一发送：协议打包到 comm 打包缓冲 → MediaSend 拷入 media 缓冲发出
 * @param inst    CommInstance 指针（须已 CommRegister）
 * @param payload 待发送 payload 指针（长度 = tx_payload_size，编译期确定）
 * @retval 0 成功；-1 失败（参数非法 / 打包或发送失败）
 *
 * @note 分包在 comm 层完成（vtable->pack 写 inst->tx_buff）；MediaSend 把
 *       打包缓冲交给 media 后端发出（USART 拷入自持 staging 缓冲 DMA 发送；
 *       USB 直接引用 data 分包，无拷贝）。
 */
int8_t CommSend(CommInstance *inst, const uint8_t *payload);

/**
 * @brief 静态定义一条通信（对话）：双向，收发协议与内容可不同
 * @param name            实例名称
 * @param media_type_     介质类型（token 拼接 COMM_##media_type_##_DEF）
 * @param rx_proto_       接收协议名 token：拼接 COMM_##rx_proto_##_DEF（协议 DEF 宏）、
 *                        PROTO_##rx_proto_（协议类型 id）、PROTO_##rx_proto_##_OVERHEAD（开销）
 * @param tx_proto_       发送协议名 token（同上三件套）
 * @param rx_type_        接收 payload 结构体类型（线协议字节布局契约）
 * @param rx_size         接收 payload 约定长度（协议文档值，非 sizeof(rx_type_)；
 *                        宏内 _Static_assert 校验 sizeof(rx_type_) == rx_size，不符即编译报错）
 * @param tx_type_        发送 payload 结构体类型（线协议字节布局契约）
 * @param tx_size         发送 payload 约定长度（协议文档值，非 sizeof(tx_type_)；
 *                        宏内 _Static_assert 校验 sizeof(tx_type_) == tx_size）
 * @param unpack_mode_    接收解包位置（UNPACK_IN_ISR 直解 / UNPACK_IN_TASK 入队解包）
 *
 * @note 跨板通信的布局一致性只能靠"每端各自把本地 sizeof(结构体) 与双方约定的线长常量
 *       比对"来保证（编译期无法跨两块板比较）。故 rx_size/tx_size 必须是写在协议文档里的
 *       固定值（如 48/55），而不是 sizeof(...)：否则 _Static_assert 变成恒等式、失去检查意义。
 *       任一端增删字段/改动压缩方式导致 sizeof 偏离约定值，该端编译即失败。
 * @note 协议名 token 内置为 RAW / CUSTOM；app 自定义协议（id >= PROTO_USER）需：
 *       1) app 层定义协议头（DEF 宏 + 开销宏 + 类型 id + Init + 后端描述符）；
 *       2) CommRegister 前调用 CommProtoRegisterBackend 登记；3) 此处传自定义 token。
 * @example
 *   COMM_DEF(vis_comm, MEDIA_USB_SIMPLE, VISUAL, VISUAL,
 *            vision_recv_t, 48, vision_send_t, 55, UNPACK_IN_ISR);
 *   // 48/55 为与对端约定的线长（协议文档值）；若结构与约定不符，内部 _Static_assert 编译期报错
 */
#define COMM_DEF(name, media_type_, rx_proto_, tx_proto_, rx_type_, rx_size, tx_type_, tx_size, unpack_mode_) \
    _Static_assert(sizeof(rx_type_) == (rx_size),                                                             \
                   "COMM rx: sizeof(" #rx_type_ ") == " #rx_size " FAILED, layout != wire-protocol len");     \
    _Static_assert(sizeof(tx_type_) == (tx_size),                                                             \
                   "COMM tx: sizeof(" #tx_type_ ") == " #tx_size " FAILED, layout != wire-protocol len");     \
    COMM_##media_type_##_DEF(name##_media,                                                                    \
                             (rx_size) + PROTO_##rx_proto_##_OVERHEAD,                                        \
                             (tx_size) + PROTO_##tx_proto_##_OVERHEAD);                                       \
    COMM_PROTO_##rx_proto_##_DEF(name##_rx_proto, name##_media, rx_size);                                     \
    COMM_PROTO_##tx_proto_##_DEF(name##_tx_proto, name##_media, tx_size);                                     \
    static uint8_t name##_tx_buff[(tx_size) + PROTO_##tx_proto_##_OVERHEAD] = {0};                            \
    CommInstance name = {                                                                                     \
        .media_type = media_type_,                                                                            \
        .rx_proto_type = PROTO_##rx_proto_,                                                                   \
        .tx_proto_type = PROTO_##tx_proto_,                                                                   \
        .unpack_mode = unpack_mode_,                                                                          \
        .media = &name##_media,                                                                               \
        .rx_proto = &name##_rx_proto,                                                                         \
        .tx_proto = &name##_tx_proto,                                                                         \
        .tx_buff = name##_tx_buff,                                                                            \
        .inited = 0,                                                                                          \
    }

#endif /* DRV_COMM_H */

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
    uint8_t *tx_buff;             // 发送打包缓冲（COMM_DEF 静态定义，协议分包写入）
    uint16_t tx_buff_size;        // 发送打包缓冲大小（= tx_size + 协议开销）
    uint8_t inited;               // 初始化标志（CommRegister 置位）
} CommInstance;

/**
 * @brief comm 运行时配置（CommConfig 传入；可重入，可反复调用修改）
 */
typedef struct
{
    void *media_cfg;             /* 介质后端配置指针（USART → USART_Config_s*；NULL 跳过介质配置） */
    ProtoFrameCallback on_frame; /* 出帧消费回调（NULL 表示不修改） */
} CommConfig_s;

/**
 * @brief 注册 comm 实例（不可重入：仅可调用一次）
 * @param inst CommInstance 指针（COMM_DEF 定义）
 * @retval 0 成功；-1 失败（参数非法 / 后端注册失败 / 类型未支持）
 *
 * @note 完成三步：1) media 后端注册（USART 内部做 USARTRegister，防重复注册）；
 *       2) 接收/发送协议后端 Init（挂 vtable）；3) 接线：media 接管接收钩子 +
 *       建立 media→comm 反向指针（接收时经 media->parent 反查 rx_proto 解包）。
 *       介质参数与出帧回调由 CommConfig 配置。
 */
int8_t CommRegister(CommInstance *inst);

/**
 * @brief 配置 comm 实例（可重入：可反复调用改介质参数 / 出帧回调）
 * @param inst CommInstance 指针（须先 CommRegister）
 * @param cfg  CommConfig_s 配置（media_cfg / on_frame 均可为 NULL，NULL 则跳过对应项）
 * @retval 0 成功；-1 失败（参数非法 / 未注册 / 配置失败 / 类型未支持）
 *
 * @note media_cfg 经 media 后端下发（USART → MediaUsartConfig → bsp USARTConfig），
 *       运行中可再次调用以切换波特率/发送模式等；on_frame 直接覆盖挂到
 *       接收协议 rx_proto->on_frame，可运行期修改消费逻辑。
 */
int8_t CommConfig(CommInstance *inst, const CommConfig_s *cfg);

/**
 * @brief 统一发送：协议打包到 comm 打包缓冲 → MediaSend 拷入 media 缓冲发出
 * @param inst    CommInstance 指针（须已 CommRegister）
 * @param payload 待发送 payload 指针（长度 = tx_payload_size，编译期确定）
 * @retval 0 成功；-1 失败（参数非法 / 打包或发送失败）
 *
 * @note 分包在 comm 层完成（vtable->pack 写 inst->tx_buff）；MediaSend 把
 *       打包缓冲拷入 media 常驻发送缓冲（DMA 异步发送期间不失效）后发出。
 *       分包后端（CAN/USB）用状态机标志 + 发送完成回调续发。
 */
int8_t CommSend(CommInstance *inst, const uint8_t *payload);

/**
 * @brief 静态定义一条通信（对话）：双向，收发协议与内容可不同
 * @param name            实例名称
 * @param media_type_     介质类型（token 拼接 COMM_##media_type_##_DEF）
 * @param rx_proto_type_  接收协议类型（token 拼接 COMM_##rx_proto_type_##_DEF）
 * @param tx_proto_type_  发送协议类型（token 拼接 COMM_##tx_proto_type_##_DEF）
 * @param rx_size         接收 payload 大小（media rx 缓冲 = rx_size + 协议开销）
 * @param tx_size         发送 payload 大小（media tx 缓冲 = tx_size + 协议开销）
 * @param unpack_mode_    接收解包位置（UNPACK_IN_ISR 直解 / UNPACK_IN_TASK 入队解包）
 *
 * @note 只定义编译期必须确定的量；运行期参数（介质配置 / 回调）
 *       由 CommRegister / CommConfig 配置。
 * @example
 *   COMM_DEF(cmd_comm, MEDIA_USART, PROTO_RAW, PROTO_RAW, 8, 16, UNPACK_IN_ISR);
 */
#define COMM_DEF(name, media_type_, rx_proto_type_, tx_proto_type_, rx_size, tx_size, unpack_mode_) \
    COMM_##media_type_##_DEF(name##_media,                                                          \
                             (rx_size) + COMM_PROTO_OVERHEAD(rx_proto_type_),                       \
                             (tx_size) + COMM_PROTO_OVERHEAD(tx_proto_type_));                      \
    COMM_##rx_proto_type_##_DEF(name##_rx_proto, name##_media, rx_size);                            \
    COMM_##tx_proto_type_##_DEF(name##_tx_proto, name##_media, tx_size);                            \
    static uint8_t name##_tx_buff[(tx_size) + COMM_PROTO_OVERHEAD(tx_proto_type_)] = {0};           \
    CommInstance name = {                                                                           \
        .media_type = media_type_,                                                                  \
        .rx_proto_type = rx_proto_type_,                                                            \
        .tx_proto_type = tx_proto_type_,                                                            \
        .unpack_mode = unpack_mode_,                                                                \
        .media = &name##_media,                                                                     \
        .rx_proto = &name##_rx_proto,                                                               \
        .tx_proto = &name##_tx_proto,                                                               \
        .tx_buff = name##_tx_buff,                                                                  \
        .tx_buff_size = (tx_size) + COMM_PROTO_OVERHEAD(tx_proto_type_),                            \
        .inited = 0,                                                                                \
    }

#endif /* DRV_COMM_H */

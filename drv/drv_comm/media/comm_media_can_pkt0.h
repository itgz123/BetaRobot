/**
 * @file comm_media_can_pkt0.h
 * @brief 通信框架-硬件层（Media）CAN 后端 - 第一字节分包（PKT0）
 *
 * 把 bsp_can 包装成统一"任意长度数据单元"通道，并在 media 层做分包收发：
 *   - 发送：整帧（协议帧）按 7B/片切分，每片前加 1B 分包序号（该片在整个帧的第几包，
 *           0 起递增），每包 = [pkt_idx][数据片 ≤ 7B]（≤8B，经典 CAN 单帧上限）经 CANTransmit 发出；
 *           整帧 ≤ 7B 时单包（pkt_idx=0，DLC=len+1）
 *   - 接收：bsp 收包（≤8B）→ 适配钩子 → 按分包序号连续重组整帧
 *           → 错位/丢包则丢帧重同步 → CommMediaRxHook（comm 层接收入口）
 * 整帧 = 协议帧（rx/tx size + 协议开销），分包序号不进入协议内容。
 *
 * 适用：通用 bsp_can，经典 CAN 8B 分片，BxCAN(F4) 与 FDCAN(H7) 经典模式均可；
 *      收发两端编译期约定帧长（固定帧长累积，无末包标志），序号纯递增。
 *
 * @note COMM_DEF 通过 token 拼接 COMM_##media_type_##_DEF 分发到本宏。
 * @note 同 CAN 总线上每个 comm 实例须分配互不重叠的接收 ID（bsp 分发命中首实例即 break）。
 */

#ifndef COMM_MEDIA_CAN_PKT0_H
#define COMM_MEDIA_CAN_PKT0_H

#include "comm_media.h"

#ifdef DRV_COMM_USED

#include "bsp_can.h"

/* 经典 CAN 单帧数据上限（8 字节）；#ifndef 保护避免与 IDSEQ 头重复定义 */
#ifndef CAN_MEDIA_FRAME_MAX
#define CAN_MEDIA_FRAME_MAX 8
#endif

/* 发送超时默认值（毫秒；上层可用 CommMediaCanPkt0Config_s.timeout_ms 覆盖） */
#ifndef CAN_MEDIA_TX_TIMEOUT_MS
#define CAN_MEDIA_TX_TIMEOUT_MS 10
#endif

/* 单包数据片上限 = 8 - 1（分包序号） */
#define CAN_MEDIA_PKT0_PAYLOAD_PER_PKT (CAN_MEDIA_FRAME_MAX - 1) /* 7 */

/* 序号空间上限：序号 0..255（1B） × 每包 7B = 1792 */
#define CAN_MEDIA_PKT0_MAX_FRAME ((uint16_t)(CAN_MEDIA_PKT0_PAYLOAD_PER_PKT * 256u))

/**
 * @brief CAN PKT0 后端运行期配置（CommConfig 的 media_cfg 指向）
 * @note 收发共用 id_type（标准/扩展帧须一致）；tx_id/rx_id 可设 CAN_ID_UNUSED 表示不发送/不接收
 */
typedef struct
{
    BoardCAN_e can_e;         /* 板载 CAN 枚举 */
    uint32_t tx_id;           /* 发送 ID；CAN_ID_UNUSED(-1) 表示不发送 */
    uint32_t rx_id;           /* 接收 ID（LIST 精确单 ID）；CAN_ID_UNUSED(-1) 表示不接收 */
    CANFrameIdType_e id_type; /* 标准/扩展（收发共用），0=标准 */
    uint32_t timeout_ms;      /* CANTransmit 超时；0 使用默认 CAN_MEDIA_TX_TIMEOUT_MS */
} CommMediaCanPkt0Config_s;

/* CAN 介质派生结构体（首成员必须为 CommMedia 基类，vtable 约定） */
typedef struct
{
    CommMedia base;        /* 基类（首成员；发送不持 staging 缓冲，MediaCanPkt0Send 直接引用 comm 打包缓冲 data） */
    uint8_t *rx_buff;      /* 接收累积缓冲（完整协议帧，不含分包序号；DEF 宏静态绑定，大小 = rx_buff_sz） */
    uint16_t rx_frame_len; /* 完整协议帧长（不含分包序号）= rx_buff_sz（DEF 宏写入；接收累积目标） */
    uint16_t tx_frame_len; /* 完整协议帧长（不含分包序号）= tx_buff_sz（DEF 宏写入；发送分包依据） */
    uint16_t rx_cnt;       /* 已累积字节数（0..rx_frame_len，上交后归零） */
    uint8_t rx_expect_pkt; /* 期望接收的下一分包序号（帧内 0 起递增；错位说明丢包，丢帧重同步） */
    uint32_t lost_frames;  /* 丢帧计数（分包错位/帧中途丢包累加） */
    uint32_t timeout_ms;   /* CANTransmit 超时（Config 写入；0 使用默认） */
} CommMediaCanPkt0;

/**
 * @brief 静态定义 CAN PKT0 介质实例
 * @param name        实例名称
 * @param rx_buff_sz  协议帧长（= rx_size + 协议开销，COMM_DEF 传入；接收累积缓冲 = rx_buff_sz）
 * @param tx_buff_sz  协议帧长（= tx_size + 协议开销；发送分包依据，写入 tx_frame_len）
 *
 * @note 展开定义 name##_can（CANInstance）、name##_rx_buff（完整协议帧接收缓冲，
 *       不含分包序号）与 name（CommMediaCanPkt0），并绑定 base.media。发送不持
 *       staging 缓冲：MediaCanPkt0Send 直接引用 comm 打包缓冲（data 在本函数运行期间
 *       有效）。缓冲放普通 RAM（CAN 无 DMA）。
 *
 * @example
 *   COMM_MEDIA_CAN_PKT0_DEF(can_comm_media, 16, 16); 协议帧 16B，帧长 > 7B 时自动分包
 */
#define COMM_MEDIA_CAN_PKT0_DEF(name, rx_buff_sz, tx_buff_sz) \
    CAN_INSTANCE_DEF(name##_can);                             \
    static uint8_t name##_rx_buff[(rx_buff_sz)] = {0};        \
    static CommMediaCanPkt0 name = {                          \
        .base.media = &name##_can,                            \
        .rx_buff = name##_rx_buff,                            \
        .rx_frame_len = (rx_buff_sz),                         \
        .tx_frame_len = (tx_buff_sz)}

/**
 * @brief 注册 CAN PKT0 介质后端（不可重入：仅可调用一次）
 * @param media CommMediaCanPkt0 实例指针（COMM_MEDIA_CAN_PKT0_DEF 定义）
 * @retval 0 成功；-1 参数非法 / 帧长超序号空间 / bsp 注册失败
 *
 * @note 完成 bsp CANRegister（防重复注册）+ 挂 vtable + 建立 can↔media 反向指针
 *       + 清接收累积与序列状态。接收回调由 MediaCanPkt0Config 挂接。
 */
int8_t MediaCanPkt0Register(CommMediaCanPkt0 *media);

/**
 * @brief 配置 CAN PKT0 介质后端（可重入：可反复调用改参数）
 * @param media CommMediaCanPkt0 实例指针（须先 MediaCanPkt0Register）
 * @param cfg   CommMediaCanPkt0Config_s*（can_e/tx_id/rx_id/id_type/timeout_ms；不可为 NULL）
 * @retval 0 成功；-1 参数非法 / 未注册 / 配置失败
 *
 * @note 组装 CAN_Config_s 调 bsp CANConfig（LIST 精确单 ID 滤波 + CLASSIC 帧 + tx_len=8，
 *       每包 DLC 由发送路径运行时改写），并强制接管 rx_callback=MediaCanPkt0RxHook、
 *       parent=media（反向指针），保证接收统一进 comm 层接收入口
 *       （CommMediaRxHook）。
 */
int8_t MediaCanPkt0Config(CommMediaCanPkt0 *media, CommMediaCanPkt0Config_s *cfg);

#endif /* DRV_COMM_USED */
#endif /* COMM_MEDIA_CAN_PKT0_H */

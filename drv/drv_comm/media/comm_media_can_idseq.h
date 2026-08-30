/**
 * @file comm_media_can_idseq.h
 * @brief 通信框架-硬件层（Media）CAN 后端 - ID 分包（IDSEQ）
 *
 * 把 bsp_can 包装成统一"任意长度数据单元"通道，并在 media 层做分包收发：
 *   - 发送：整帧（协议帧）按数据片切分（CLASSIC 8B / FD 64B），序号编码进 CAN ID
 *           段内偏移（id = base_id + seq，0 起递增），每包 = 整帧全为数据片（序号在 ID，
 *           不占数据字节），末包短帧不补零（接收端用实际 len 量度）经 CANTransmit 发出
 *   - 接收：bsp 收包 → 适配钩子 → 从 pack->id 提取序号（seq = id - base_id）
 *           → 按序号连续重组整帧 → 错位/丢包则丢帧重同步 → CommMediaRxHook（comm 层接收入口）
 * 整帧 = 协议帧（rx/tx size + 协议开销），分包序号在 CAN ID 中，不进入协议内容。
 *
 * 适用：通用 bsp_can，按 mode 支持经典 CAN 8B 分片（BxCAN(F4) 与 FDCAN(H7) 经典模式均可）
 *      与 FD 64B 分片（FDCAN 硬件，FrameFormat 须匹配 FD/FD_BRS）；
 *      收发两端编译期约定帧长（固定帧长累积，无末包标志），序号纯递增。
 *
 * @note COMM_DEF 通过 token 拼接 COMM_##media_type_##_DEF 分发到本宏。
 * @note 同 CAN 总线上每个 comm 实例须分配互不重叠的 ID 段：本后端占
 *       [base_id, base_id + 分包数)，分包数 = ceil(帧长/数据片长)，Config 按帧长自动定段
 *       （RANGE 过滤，无需 2 的幂对齐，任意 base_id 起点均可）。
 */

#ifndef COMM_MEDIA_CAN_IDSEQ_H
#define COMM_MEDIA_CAN_IDSEQ_H

#include "comm_media.h"

#ifdef DRV_COMM_USED

#include "bsp_can.h"

/* 经典 CAN 单帧数据上限（8 字节）；#ifndef 保护避免与 PKT0 头重复定义 */
#ifndef CAN_MEDIA_FRAME_MAX
#define CAN_MEDIA_FRAME_MAX 8
#endif

/* FD 单帧数据上限（64 字节；FDCAN 硬件）；#ifndef 保护避免与 PKT0 头重复定义 */
#ifndef CAN_MEDIA_FRAME_MAX_FD
#define CAN_MEDIA_FRAME_MAX_FD 64
#endif

/**
 * @brief CAN IDSEQ 后端运行期配置（CommConfig 的 media_cfg 指向）
 * @note 收发共用同一 ID 段（base_id + seq）；Config 校验：frame_type 数据帧、mode 三种合法值、
 *       base_id ≤ ID 上限、ID 段 [base_id, base_id + 分包数) 不越上限
 *       （分包数 = ceil(帧长/数据片长)，CLASSIC 8B / FD 64B）。
 */
typedef struct
{
    BoardCAN_e can_e;            /* 板载 CAN 枚举 */
    uint32_t base_id;            /* 收发基址（ID 段起点）：分包序号加到基址上（id = base_id + seq） */
    CAN_Frame_Type_e frame_type; /* 帧类型：仅标准/扩展数据帧（收发共用，须一致） */
    CAN_Mode_Type_e mode;        /* CAN 帧格式：CLASSIC(8B/帧)/FD/FD_BRS(64B/帧) */
    uint32_t timeout_ms;         /* CANTransmit 超时（ms；0 = 不等待资源立即返回失败） */
} CommMediaCanIdseqConfig_s;

/* CAN IDSEQ 介质派生结构体（首成员必须为 CommMedia 基类，vtable 约定） */
typedef struct
{
    CommMedia base;         /* 基类（首成员） */
    uint8_t *rx_buff;       /* 接收累积缓冲（完整协议帧，不含分包序号；DEF 宏静态绑定，大小 = rx_buff_sz） */
    uint8_t *tx_buff;       /* 发送 staging 缓冲（完整协议帧，不含分包序号；DEF 宏静态绑定，大小 = tx_buff_sz；
                             * MediaCanIdseqSend 先整帧拷入此处，再逐包异步发出） */
    uint16_t rx_frame_len;  /* 完整协议帧长（不含分包序号）= rx_buff_sz（DEF 宏写入；接收累积目标） */
    uint16_t tx_frame_len;  /* 完整协议帧长（不含分包序号）= tx_buff_sz（DEF 宏写入；发送分包依据） */
    uint16_t rx_cnt;        /* 已累积字节数（0..rx_frame_len，上交后归零） */
    uint16_t tx_sent;       /* 已发送字节位置（0..tx_frame_len；异步分包推进依据，发完一帧回到 tx_frame_len） */
    uint8_t tx_active;      /* 异步分包发送进行中（1 = 上一帧尚未全部发出，拒绝新 Send 重入） */
    uint32_t rx_expect_pkt; /* 期望接收的下一分包序号（序号段大小可 >255，用 uint32_t） */
    uint32_t lost_frames;   /* 丢帧计数（分包错位/帧中途丢包累加） */
    uint32_t timeout_ms;    /* CANTransmit 超时（Config 写入） */
    uint32_t base_id;       /* 收发基址（ID 段起点；Config 写入） */

    /* CAN 收发参数（Config 写入） */
    CAN_Filter_s can_filter;     /* 接收过滤器（每实例一份，MASK 段匹配；bsp 为指针存储，须常驻实例） */
    CAN_Frame_Type_e frame_type; /* 帧类型（标准/扩展数据帧；收发共用） */
    CAN_Mode_Type_e mode;        /* CAN 帧格式：CLASSIC(8B/帧)/FD/FD_BRS(64B/帧)；分包片长与接收防御按此 */
} CommMediaCanIdseq;

/**
 * @brief 静态定义 CAN IDSEQ 介质实例
 * @param name        实例名称
 * @param rx_buff_sz  协议帧长（= rx_size + 协议开销，COMM_DEF 传入；接收累积缓冲 = rx_buff_sz）
 * @param tx_buff_sz  协议帧长（= tx_size + 协议开销；发送分包依据，写入 tx_frame_len）
 *
 * @note 展开定义 name##_can（CANInstance）、name##_rx_buff（完整协议帧接收缓冲，
 *       不含分包序号）、name##_tx_buff（完整协议帧发送 staging 缓冲，异步分包期间
 *       保数据不失效）与 name（CommMediaCanIdseq），并绑定 base.media。
 *       MediaCanIdseqSend 先整帧拷入 tx_buff，发第一包后返回，剩余包在
 *       CAN 发送完成回调（bsp tx_complete_callback）中逐包续发。
 *       缓冲放普通 RAM（CAN 无 DMA）。base_id/frame_type/mode 由 Config 写入。
 *
 * @example
 *   COMM_MEDIA_CAN_IDSEQ_DEF(can_comm_media, 16, 16); 协议帧 16B，帧长 > 8B 时自动分包
 */
#define COMM_MEDIA_CAN_IDSEQ_DEF(name, rx_buff_sz, tx_buff_sz) \
    CAN_INSTANCE_DEF(name##_can);                              \
    static uint8_t name##_rx_buff[(rx_buff_sz)] = {0};         \
    static uint8_t name##_tx_buff[(tx_buff_sz)] = {0};         \
    static CommMediaCanIdseq name = {                          \
        .base.media = &name##_can,                             \
        .rx_buff = name##_rx_buff,                             \
        .tx_buff = name##_tx_buff,                             \
        .rx_frame_len = (rx_buff_sz),                          \
        .tx_frame_len = (tx_buff_sz)}

/**
 * @brief 注册 CAN IDSEQ 介质后端（不可重入：仅可调用一次）
 * @param media CommMediaCanIdseq 实例指针（COMM_MEDIA_CAN_IDSEQ_DEF 定义）
 * @retval 0 成功；-1 参数非法 / bsp 注册失败
 *
 * @note 完成 bsp CANRegister（防重复注册）+ 挂 vtable + 建立 can↔media 反向指针
 *       + 清接收累积与序列状态。ID 段/帧长校验放 Config（mode 未知）。
 *       接收回调由 MediaCanIdseqConfig 挂接。
 */
int8_t MediaCanIdseqRegister(CommMediaCanIdseq *media);

/**
 * @brief 配置 CAN IDSEQ 介质后端（可重入：可反复调用改参数）
 * @param media CommMediaCanIdseq 实例指针（须先 MediaCanIdseqRegister）
 * @param cfg   CommMediaCanIdseqConfig_s*（can_e/base_id/frame_type/mode/timeout_ms；不可为 NULL）
 * @retval 0 成功；-1 参数非法 / 未注册 / 配置失败
 *
 * @note 校验 frame_type 数据帧、mode 三种合法值、base_id ≤ ID 上限、ID 段不越上限；
 *       按帧长自动定段（分包数 = ceil(帧长/数据片长)），组装 CAN_Config_s 调 bsp CANConfig
 *       （RANGE 段过滤 id0=段下限/id1=段上限 + mode 透传，每包 ID/len 由发送路径运行时构造），
 *       BxCAN 非 CLASSIC / FDCAN FrameFormat 不匹配由 bsp 返回 -1。
 *       接收经 MediaCanIdseqRxHook 保证统一进 comm 层接收入口（CommMediaRxHook）。
 */
int8_t MediaCanIdseqConfig(CommMediaCanIdseq *media, CommMediaCanIdseqConfig_s *cfg);

#endif /* DRV_COMM_USED */
#endif /* COMM_MEDIA_CAN_IDSEQ_H */

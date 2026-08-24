/**
 * @file comm_media_can_idseq.c
 * @brief 通信框架-硬件层（Media）CAN 后端 - ID 分包（IDSEQ）实现
 *
 * 发送：整帧按 8B/片分包，分包序号编码进 CAN ID 低 seq_bits 位（base_id + seq，0 起递增），
 *       每包 = 8B 全为数据片（末包短帧不补零）经 CANTransmit 发出。
 * 接收：bsp CAN 中断 → 分发（匹配 ID 后设 rx_id_matched/rx_len/rx_buff）
 *       → 适配钩子 MediaCanIdseqRxHook（从 ID 提取序号，连续重组，错位丢帧重同步）
 *       → CommMediaRxHook（comm 层接收入口）。
 *
 * @note bsp 在接收回调返回后才释放缓冲，但回调返回后缓冲可能被下一包覆盖，
 *       因此接收处理必须在回调上下文内同步 memcpy 累积，不能延迟引用 can->rx_buff。
 * @note 分包序号在 CAN ID 中，不进入协议内容；接收重组到固定协议帧长
 *       （rx_frame_len）即完成一帧（无末包标志，收发编译期约定帧长）。
 * @note 发送为异步分包：MediaCanIdseqSend 先整帧拷入自持 staging 缓冲 m->tx_buff，
 *       发送第一包后即返回（tx_sent 记录已发位置），后续包由 CAN 发送完成回调
 *       （bsp tx_complete_callback → MediaCanIdseqTxHook）逐包续发，发完清 tx_active。
 *       CANTransmit 同步等 mailbox/Tx FIFO 空间并拷入外设缓冲，故回调内续发不会覆盖已排队帧。
 */

#include "comm_media_can_idseq.h"
#include "drv_comm.h" /* CommMediaRxHook：comm 层接收入口 */
#include <string.h>

#ifdef DRV_COMM_USED

static int8_t MediaCanIdseqSend(CommMedia *media, const uint8_t *data);
static int8_t MediaCanIdseqSendNext(CommMediaCanIdseq *m);
static void MediaCanIdseqTxHook(CANInstance *can);
static void MediaCanIdseqRxHook(CANInstance *can);
static void MediaCanIdseqFrameComplete(CommMediaCanIdseq *m);

/* 发送前改写 DLC（经典 CAN ≤8B：DLC 数值即字节数；后端独立，不复用共享头） */
static inline void CommMediaCanIdseqSetDlc(CANInstance *can, uint8_t len)
{
#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    can->tx_header.DataLength = (uint32_t)len;
#else
    can->tx_header.DLC = len;
#endif
}

/* 发送前改写 ID（IDSEQ 每包 ID 不同 = base_id + seq）
 * @note tx_header 的 ID/类型在 CANConfig 时已按 base_id 填充，此处按每包序号改写 */
static inline void CommMediaCanIdseqSetId(CANInstance *can, uint32_t id, CANFrameIdType_e type)
{
#if BSP_CAN_IP == BSP_CAN_IP_FDCAN
    can->tx_header.Identifier = id;
    can->tx_header.IdType = (type == CAN_FRAME_ID_EXT) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
#else
    can->tx_header.StdId = id;
    can->tx_header.ExtId = id;
    can->tx_header.IDE = (type == CAN_FRAME_ID_EXT) ? CAN_ID_EXT : CAN_ID_STD;
#endif
}

static const CommMediaVTable_s s_can_idseq_vtable = {
    .send = MediaCanIdseqSend,
};

/* vtable 发送实现：整帧协议帧按 8B/片异步分包发送，每包 ID = base_id + seq。
 * 1) 整帧拷入自持 staging 缓冲 m->tx_buff（comm 打包缓冲 data 在 CommSend 返回后即失效，
 *    而后续分包在 CAN 发送完成回调中续发，必须拷贝保数据）
 * 2) 发第一包（8B，seq=0）后返回，tx_sent 记录已发位置
 * 3) 剩余包由 MediaCanIdseqTxHook（bsp tx_complete_callback）逐包续发
 * @note tx_active=1 表示上一帧尚未发完，此时拒绝新 Send（丢帧不覆盖）。
 * @note 中途某包发送失败则中止：已发部分由接收端序号错位丢帧重同步。 */
static int8_t MediaCanIdseqSend(CommMedia *media, const uint8_t *data)
{
    CommMediaCanIdseq *m = (CommMediaCanIdseq *)media;
    CANInstance *can;

    /* 先判空再解引用（m==NULL 时不能先访问 m->base.media） */
    if (m == NULL || data == NULL || m->tx_frame_len == 0 || m->tx_buff == NULL)
        return -1;
    can = (CANInstance *)m->base.media;
    if (can == NULL)
        return -1;

    if (m->tx_active)
        return -1; /* 上一帧仍在异步分包发送中，拒绝重入 */

    memcpy(m->tx_buff, data, m->tx_frame_len); /* 整帧拷入自持缓冲（异步续发期间不失效） */
    m->tx_sent = 0;
    m->tx_active = 1;

    if (MediaCanIdseqSendNext(m) != 0)
    {
        m->tx_active = 0; /* 首包发送失败：中止 */
        return -1;
    }
    return 0;
}

/* 提交下一分包：从 tx_sent 起切 ≤8B 一片，ID = base_id + seq（seq = 已发片数）。
 * @retval 0 成功提交一片；1 整帧已发完（无待发数据）；-1 发送失败/超时 */
static int8_t MediaCanIdseqSendNext(CommMediaCanIdseq *m)
{
    CANInstance *can = (CANInstance *)m->base.media;
    uint16_t remain;
    uint8_t chunk;
    uint32_t pkt_idx;

    remain = m->tx_frame_len - m->tx_sent;
    if (remain == 0)
        return 1; /* 整帧已发完 */

    chunk = (remain > CAN_MEDIA_FRAME_MAX) ? CAN_MEDIA_FRAME_MAX : (uint8_t)remain;
    pkt_idx = m->tx_sent / CAN_MEDIA_FRAME_MAX; /* 该片在整个帧的第几包（0 起） */

    memcpy(can->tx_buff, &m->tx_buff[m->tx_sent], chunk);
    can->tx_id = m->base_id + pkt_idx; /* 过 CANTransmit 的 tx_id 校验 */
    CommMediaCanIdseqSetId(can, m->base_id + pkt_idx, m->id_type);
    CommMediaCanIdseqSetDlc(can, chunk);
    if (CANTransmit(can, m->timeout_ms) != 1)
        return -1; /* 发送失败或超时：中止分包（已发部分接收端丢帧重同步） */

    m->tx_sent += chunk; /* 记录发送到哪个位置 */
    return 0;
}

/* bsp 发送完成适配钩子（tx_complete_callback）：续发下一分包，直至整帧发完清 tx_active。
 * @note 在 CAN 中断上下文执行；CANTransmit 同步等 mailbox/FIFO 空间（上一片已发完必有空间）。 */
static void MediaCanIdseqTxHook(CANInstance *can)
{
    CommMediaCanIdseq *m = (CommMediaCanIdseq *)can->parent; /* media 层设置的反向指针 */

    if (m == NULL)
        return;
    if (MediaCanIdseqSendNext(m) != 0)
        m->tx_active = 0; /* 发完(1) 或失败(-1)：结束本轮异步发送 */
}

/* 重组出一整帧（rx_cnt 已达 rx_frame_len）：上交 comm 层并复位累积 */
static void MediaCanIdseqFrameComplete(CommMediaCanIdseq *m)
{
    /* 跳过 media 基类，直连 comm 层接收入口；m->rx_buff 为完整协议帧（无分包序号）
     * @warning UNPACK_IN_ISR 下 payload 指向累积缓冲，回调返回后即被下一包覆盖，
     *          on_frame 必须同步消费（解析/拷贝） */
    CommMediaRxHook(&m->base, m->rx_buff);
    m->rx_cnt = 0;
    m->rx_expect_pkt = 0;
}

/* bsp 接收适配钩子：每包 ID = base_id + seq，按序号连续重组整帧。
 * 一包 ≤ 8B（经典 CAN 单帧），序号从 ID 低 seq_bits 位提取，错位说明丢包 → 丢帧重同步。 */
static void MediaCanIdseqRxHook(CANInstance *can)
{
    CommMediaCanIdseq *m = (CommMediaCanIdseq *)can->parent; /* media 层设置的反向指针 */
    uint32_t seq;
    uint16_t data_len;

    /* rx_len < 1 防御空帧；rx_len > 8 防御 FD 帧（本后端仅经典 CAN 8B 分包） */
    if (m == NULL || can->rx_len < 1 || can->rx_len > CAN_MEDIA_FRAME_MAX)
        return;

    /* 序号 = 实际匹配 ID 相对 base_id 的偏移，取低 seq_bits 位 */
    seq = (can->rx_id_matched - m->base_id) & ((1u << m->seq_bits) - 1u);
    data_len = can->rx_len; /* 8B 全为数据 */

    /* 分包序号校验：期望连续。错位说明丢包/错乱 → 丢弃当前帧累积，重新同步 */
    if (seq != m->rx_expect_pkt)
    {
        if (m->rx_cnt > 0)
            m->lost_frames++; /* 帧中途丢包 */
        m->rx_cnt = 0;
        m->rx_expect_pkt = 0;
        if (seq != 0)
        {
            m->lost_frames++; /* 非新帧首包错位 */
            return;           /* 丢弃当前包，等新帧首包（seq=0） */
        }
    }

    /* 追加数据片（超出帧长部分截断，防御配置不符） */
    if (data_len > 0)
    {
        uint16_t space = m->rx_frame_len - m->rx_cnt; /* 剩余目标字节数 */

        if (data_len > space)
            data_len = space;
        if (data_len > 0)
            memcpy(&m->rx_buff[m->rx_cnt], can->rx_buff, data_len);
        m->rx_cnt += data_len;
        m->rx_expect_pkt++;
    }

    /* 累积到完整协议帧长：上交一帧 */
    if (m->rx_cnt == m->rx_frame_len)
        MediaCanIdseqFrameComplete(m);
}

int8_t MediaCanIdseqRegister(CommMediaCanIdseq *media)
{
    CANInstance *can;

    if (media == NULL || media->rx_buff == NULL || media->tx_buff == NULL)
        return -1;
    if (media->rx_frame_len == 0 || media->tx_frame_len == 0)
        return -1; /* 收发协议帧长须非 0 */

    can = (CANInstance *)media->base.media; /* COMM_MEDIA_CAN_IDSEQ_DEF 已绑定 */
    if (can == NULL)
        return -1;

    /* bsp 注册（防重复注册；CAN_INSTANCE_NUM 共享池，本函数不可重入） */
    if (CANRegister(can) != 0)
        return -1;

    media->base.vtable = &s_can_idseq_vtable;
    media->base.parent = NULL; /* comm 层挂所属 CommInstance */

    can->parent = media; /* 反向指针：适配钩子据此取回 media */

    /* 清接收累积与分包序号状态（初始：期望首包 seq=0）及发送异步状态 */
    media->rx_cnt = 0;
    media->rx_expect_pkt = 0;
    media->lost_frames = 0;
    media->tx_sent = 0;
    media->tx_active = 0;
    return 0;
}

int8_t MediaCanIdseqConfig(CommMediaCanIdseq *media, CommMediaCanIdseqConfig_s *cfg)
{
    CANInstance *can;
    CAN_Config_s can_cfg;
    uint32_t id_max;
    uint32_t seq_mask;
    uint32_t rx_mask;
    uint64_t seq_space;

    if (media == NULL || cfg == NULL)
        return -1;
    can = (CANInstance *)media->base.media;
    if (can == NULL)
        return -1;

    /* 校验 seq_bits 范围：1 ~ ID 位宽（标准帧 11，扩展帧 29） */
    if (cfg->seq_bits < 1 || cfg->seq_bits > ((cfg->id_type == CAN_FRAME_ID_EXT) ? 29U : 11U))
        return -1;

    /* base_id 范围（ID 上限取决于帧类型） */
    id_max = (cfg->id_type == CAN_FRAME_ID_EXT) ? 0x1FFFFFFFU : 0x7FFU;
    if (cfg->base_id > id_max)
        return -1;

    /* base_id 对齐：低 seq_bits 位须为 0，否则序号会跨掩码位（发送被拒收/序号提取错误） */
    seq_mask = (1u << cfg->seq_bits) - 1u;
    if ((cfg->base_id & seq_mask) != 0)
        return -1;

    /* 帧长 ≤ 序号空间 8 × 2^seq_bits（用 uint64 防 seq_bits=29 时 8<<29 溢出 uint32） */
    seq_space = (uint64_t)CAN_MEDIA_FRAME_MAX << cfg->seq_bits;
    if ((uint64_t)media->rx_frame_len > seq_space || (uint64_t)media->tx_frame_len > seq_space)
        return -1;

    /* 掩码 = 保留高位的基址匹配，低 seq_bits 位不参与匹配；必须 AND id_max 过 bsp 校验 */
    rx_mask = (~seq_mask) & id_max;

    /* 组装 bsp 配置：MASK 范围滤波 + CLASSIC 帧 + tx_len=8（初始 DLC，发送时按分包改写） */
    can_cfg = (CAN_Config_s){
        .can_e = cfg->can_e,
        .tx_id = cfg->base_id,
        .tx_id_type = cfg->id_type,
        .tx_frame_format = CAN_FRAME_FORMAT_CLASSIC,
        .tx_len = CAN_MEDIA_FRAME_MAX,
        .filter_mode = CAN_FILTER_MODE_MASK,
        .rx_id_list = {cfg->base_id, CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED},
        .rx_mask = rx_mask,
        .rx_id_type = cfg->id_type,
        .rx_callback = MediaCanIdseqRxHook,          /* 保证接收统一进 comm 层接收入口 */
        .tx_complete_callback = MediaCanIdseqTxHook, /* 发送完成回调：续发下一分包 */
    };

    if (CANConfig(can, &can_cfg) != 0)
        return -1;

    media->base_id = cfg->base_id;
    media->seq_bits = cfg->seq_bits;
    media->id_type = cfg->id_type;
    media->timeout_ms = (cfg->timeout_ms != 0) ? cfg->timeout_ms : CAN_MEDIA_TX_TIMEOUT_MS;
    return 0;
}

#endif /* DRV_COMM_USED */

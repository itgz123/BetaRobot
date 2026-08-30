/**
 * @file comm_media_can_pkt0.c
 * @brief 通信框架-硬件层（Media）CAN 后端 - 第一字节分包（PKT0）实现
 *
 * 发送：整帧按 mode 分包（CLASSIC 7B/片 / FD 63B/片），每片前加 1B 分包序号
 *       （该片在整个帧的第几包，0 起递增），每包 = [pkt_idx][数据片] 经 CANTransmit 发出。
 * 接收：bsp CAN 中断 → 软件过滤分发（匹配后回调带 CAN_Pack_s）→ 适配钩子 MediaCanPkt0RxHook
 *       → 按分包序号连续重组整帧（错位丢帧重同步）→ CommMediaRxHook（comm 层接收入口）。
 *
 * @note bsp 传回的 CAN_Pack_s 为回调内栈上结构，回调返回后即失效，
 *       因此接收处理必须在回调上下文内同步 memcpy 累积，不能延迟引用 pack->data。
 * @note 分包序号是 media 层协议元数据，不进入协议内容；接收重组到固定协议帧长
 *       （rx_frame_len）即完成一帧（无末包标志，收发编译期约定帧长）。
 * @note 发送为异步分包：MediaCanPkt0Send 先整帧拷入自持 staging 缓冲 m->tx_buff，
 *       发送第一包后即返回（tx_sent 记录已发位置），后续包由 CAN 发送完成回调
 *       （bsp tx_complete_callback → MediaCanPkt0TxHook）逐包续发，发完清 tx_active。
 *       CANTransmit 同步等 mailbox/Tx FIFO 空间并拷入外设缓冲，故回调内续发不会覆盖已排队帧。
 */

#include "comm_media_can_pkt0.h"
#include "drv_comm.h" /* CommMediaRxHook：comm 层接收入口 */
#include <string.h>

#ifdef DRV_COMM_USED

static int8_t MediaCanPkt0Send(CommMedia *media, const uint8_t *data);
static int8_t MediaCanPkt0SendNext(CommMediaCanPkt0 *m);
static void MediaCanPkt0TxHook(CANInstance *can, uint32_t tx_mailbox);
static void MediaCanPkt0RxHook(CANInstance *can, const CAN_Pack_s *pack);

static const CommMediaVTable_s s_can_pkt0_vtable = {
    .send = MediaCanPkt0Send,
};

/* vtable 发送实现：整帧协议帧按 mode 分包异步发送，每包 = [分包序号][数据片]。
 * 1) 整帧拷入自持 staging 缓冲 m->tx_buff（comm 打包缓冲 data 在 CommSend 返回后即失效，
 *    而后续分包在 CAN 发送完成回调中续发，必须拷贝保数据）
 * 2) 发第一包（数据片 7B/63B + 序号 1B）后返回，tx_sent 记录已发位置
 * 3) 剩余包由 MediaCanPkt0TxHook（bsp tx_complete_callback）逐包续发
 * @note tx_active=1 表示上一帧尚未发完，此时拒绝新 Send（丢帧不覆盖）。
 * @note 中途某包发送失败则中止：已发部分由接收端序号错位丢帧重同步。 */
static int8_t MediaCanPkt0Send(CommMedia *media, const uint8_t *data)
{
    CommMediaCanPkt0 *m = (CommMediaCanPkt0 *)media;
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

    if (MediaCanPkt0SendNext(m) != 0)
    {
        m->tx_active = 0; /* 首包发送失败：中止 */
        return -1;
    }
    return 0;
}

/* 提交下一分包：从 tx_sent 起切 ≤单片长数据片（按 mode：CLASSIC 7B / FD 63B），
 * 前加 1B 分包序号（序号 = 已发片数）。
 * @retval 0 成功提交一片；1 整帧已发完（无待发数据）；-1 发送失败/超时 */
static int8_t MediaCanPkt0SendNext(CommMediaCanPkt0 *m)
{
    CANInstance *can = (CANInstance *)m->base.media;
    CAN_Pack_s pack = {0};
    uint16_t remain;
    uint8_t payload_max;
    uint8_t chunk;
    uint16_t pkt_idx;

    remain = m->tx_frame_len - m->tx_sent;
    if (remain == 0)
        return 1; /* 整帧已发完 */

    payload_max = (m->mode == CAN_FRAME_FORMAT_CLASSIC) ? CAN_MEDIA_PKT0_PAYLOAD_CLASSIC : CAN_MEDIA_PKT0_PAYLOAD_FD;
    chunk = (remain > payload_max) ? payload_max : (uint8_t)remain;
    pkt_idx = m->tx_sent / payload_max; /* 分包序号（0,1,2,...；FD 63B/片，序号 0..255 不溢出） */

    pack.id = m->tx_id;              /* 发送 ID（Config 写入；bsp 同步拷贝，可栈上构造） */
    pack.frame_type = m->frame_type; /* 标准/扩展数据帧（Config 写入） */
    pack.len = (uint8_t)(chunk + 1); /* 整包 = [序号][数据片]，len = 数据片长 + 1 */
    pack.data[0] = (uint8_t)pkt_idx;
    memcpy(&pack.data[1], &m->tx_buff[m->tx_sent], chunk);

    if (CANTransmit(can, &pack, m->timeout_ms, NULL, NULL) != 0)
        return -1; /* 发送失败或超时：中止分包（已发部分接收端丢帧重同步） */

    m->tx_sent += chunk; /* 记录发送到哪个位置 */
    return 0;
}

/* bsp 发送完成适配钩子（tx_complete_callback）：续发下一分包，直至整帧发完清 tx_active。
 * @note 在 CAN 中断上下文执行；CANTransmit 同步等 mailbox/FIFO 空间（上一片已发完必有空间）。
 * @note pkt0 为单通道串行分包发送，无需按 tx_mailbox 区分帧来源。 */
static void MediaCanPkt0TxHook(CANInstance *can, uint32_t tx_mailbox)
{
    CommMediaCanPkt0 *m = (CommMediaCanPkt0 *)can->parent; /* media 层设置的反向指针 */

    (void)tx_mailbox;
    if (m == NULL)
        return;
    if (MediaCanPkt0SendNext(m) != 0)
        m->tx_active = 0; /* 发完(1) 或失败(-1)：结束本轮异步发送 */
}

/* bsp 接收适配钩子：每包 = [pkt_idx][数据片]，按分包序号连续重组整帧。
 * 一包 ≤ 8B（CLASSIC）/ 64B（FD），包序号错位说明丢包 → 丢帧重同步等新帧首包。 */
static void MediaCanPkt0RxHook(CANInstance *can, const CAN_Pack_s *pack)
{
    CommMediaCanPkt0 *m = (CommMediaCanPkt0 *)can->parent; /* media 层设置的反向指针 */
    uint8_t pkt_idx;
    uint8_t frame_max;
    uint16_t data_len;

    if (m == NULL || pack == NULL || pack->len < 1)
        return;
    /* pack->len 超 mode 上限防御错配（FD 帧误入 CLASSIC 实例等） */
    frame_max = (m->mode == CAN_FRAME_FORMAT_CLASSIC) ? CAN_MEDIA_FRAME_MAX : CAN_MEDIA_FRAME_MAX_FD;
    if (pack->len > frame_max)
        return;

    pkt_idx = pack->data[0];
    data_len = (uint16_t)(pack->len - 1);

    /* 分包序号校验：期望连续。错位说明丢包/错乱 → 丢弃当前帧累积，重新同步 */
    if (pkt_idx != m->rx_expect_pkt)
    {
        if (m->rx_cnt > 0)
            m->lost_frames++; /* 帧中途丢包 */
        m->rx_cnt = 0;
        m->rx_expect_pkt = 0;
        if (pkt_idx != 0)
        {
            m->lost_frames++; /* 非新帧首包错位 */
            return;           /* 丢弃当前包，等新帧首包（pkt_idx=0） */
        }
    }

    /* 追加数据片（超出帧长部分截断，防御配置不符） */
    if (data_len > 0)
    {
        uint16_t space = m->rx_frame_len - m->rx_cnt; /* 剩余目标字节数 */

        if (data_len > space)
            data_len = space;
        if (data_len > 0)
            memcpy(&m->rx_buff[m->rx_cnt], &pack->data[1], data_len);
        m->rx_cnt += data_len;
        m->rx_expect_pkt++;
    }

    /* 累积到完整协议帧长：上交一帧并复位累积。
     * 跳过 media 基类，直连 comm 层接收入口；m->rx_buff 为完整协议帧（无分包序号）
     * @warning UNPACK_IN_ISR 下 payload 指向累积缓冲，回调返回后即被下一包覆盖，
     *          on_frame 必须同步消费（解析/拷贝） */
    if (m->rx_cnt == m->rx_frame_len)
    {
        CommMediaRxHook(&m->base, m->rx_buff);
        m->rx_cnt = 0;
        m->rx_expect_pkt = 0;
    }
}

int8_t MediaCanPkt0Register(CommMediaCanPkt0 *media)
{
    CANInstance *can;

    if (media == NULL || media->rx_buff == NULL || media->tx_buff == NULL)
        return -1;
    if (media->rx_frame_len == 0 || media->tx_frame_len == 0)
        return -1; /* 收发协议帧长须非 0 */
    if (media->rx_frame_len > CAN_MEDIA_PKT0_MAX_FRAME_FD ||
        media->tx_frame_len > CAN_MEDIA_PKT0_MAX_FRAME_FD)
        return -1; /* 帧长超 FD 序号空间（1B 序号 × 63B/包 = 16128；mode 未定，放宽到 FD 上限，
                    * Config 按所选 mode 精确校验） */

    can = (CANInstance *)media->base.media; /* COMM_MEDIA_CAN_PKT0_DEF 已绑定 */
    if (can == NULL)
        return -1;

    /* bsp 注册（防重复注册；CAN_INSTANCE_NUM 共享池，本函数不可重入） */
    if (CANRegister(can) != 0)
        return -1;

    media->base.vtable = &s_can_pkt0_vtable;
    media->base.parent = NULL; /* comm 层挂所属 CommInstance */

    can->parent = media; /* 反向指针：适配钩子据此取回 media */

    /* 清接收累积与分包序号状态（初始：期望首包 pkt_idx=0）及发送异步状态 */
    media->rx_cnt = 0;
    media->rx_expect_pkt = 0;
    media->lost_frames = 0;
    media->tx_sent = 0;
    media->tx_active = 0;
    return 0;
}

int8_t MediaCanPkt0Config(CommMediaCanPkt0 *media, CommMediaCanPkt0Config_s *cfg)
{
    CANInstance *can;
    CAN_Config_s can_cfg;
    uint32_t id_max;
    uint16_t frame_max;

    if (media == NULL || cfg == NULL)
        return -1;
    can = (CANInstance *)media->base.media;
    if (can == NULL)
        return -1;

    /* 帧类型：本后端为数据分包，仅允许标准/扩展数据帧（FD 帧无 RTR，数据帧天然兼容） */
    if (cfg->frame_type != CAN_STANDARD_DATA_FRAME &&
        cfg->frame_type != CAN_EXTENDED_DATA_FRAME)
        return -1;

    /* 帧格式 mode：仅三种合法值；BxCAN 非 CLASSIC 由 bsp 拒绝（此处不做硬件判断） */
    if (cfg->mode != CAN_FRAME_FORMAT_CLASSIC &&
        cfg->mode != CAN_FRAME_FORMAT_FD &&
        cfg->mode != CAN_FRAME_FORMAT_FD_BRS)
        return -1;

    /* 帧长按所选 mode 精确校验：CLASSIC ≤ 1792 / FD ≤ 16128（序号 0..255 × 单片长） */
    frame_max = (cfg->mode == CAN_FRAME_FORMAT_CLASSIC) ? CAN_MEDIA_PKT0_MAX_FRAME : CAN_MEDIA_PKT0_MAX_FRAME_FD;
    if (media->rx_frame_len > frame_max || media->tx_frame_len > frame_max)
        return -1;

    /* ID 范围校验（提前拦截，避免进 bsp 才失败）；CAN_ID_UNUSED(-1) 表示不发送/不接收 */
    id_max = (cfg->frame_type == CAN_EXTENDED_DATA_FRAME) ? 0x1FFFFFFFU : 0x7FFU;
    if (cfg->tx_id != CAN_ID_UNUSED && cfg->tx_id > id_max)
        return -1;
    if (cfg->rx_id != CAN_ID_UNUSED && cfg->rx_id > id_max)
        return -1;

    /* 组装 per-instance 过滤器：LIST 精确单 ID + 收发共用帧类型（bsp 为指针存储，须常驻实例） */
    media->can_filter.mode = CAN_FILTER_MODE_LIST;
    media->can_filter.id0 = cfg->rx_id;
    media->can_filter.id1 = CAN_ID_UNUSED;
    media->can_filter.frame_type = cfg->frame_type;
    media->can_filter.callback = (cfg->rx_id == CAN_ID_UNUSED) ? NULL : MediaCanPkt0RxHook;

    /* 组装 bsp 配置：mode 透传（FDCAN FD/FD_BRS 需 CubeMX FrameFormat 匹配，不匹配 bsp 返回 -1）
     * + 软件过滤 + 发送完成回调续发
     * @note .parent 必须显式设回 media：bsp CANConfig 会覆盖 Register 设的 can->parent */
    can_cfg = (CAN_Config_s){
        .can_e = cfg->can_e,
        .mode = cfg->mode,
        .parent = media,
        .filters = &media->can_filter,
        .filter_num = 1,
        .tx_complete_callback = MediaCanPkt0TxHook, /* 发送完成回调：续发下一分包 */
    };

    if (CANConfig(can, &can_cfg) != 0)
        return -1;

    media->tx_id = cfg->tx_id;
    media->frame_type = cfg->frame_type;
    media->mode = cfg->mode;
    media->timeout_ms = (cfg->timeout_ms != 0) ? cfg->timeout_ms : CAN_MEDIA_TX_TIMEOUT_MS;
    return 0;
}

#endif /* DRV_COMM_USED */

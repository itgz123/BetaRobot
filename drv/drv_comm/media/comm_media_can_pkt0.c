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
 *       （bsp tx_complete_callback → MediaCanPkt0TxHook）逐包续发，发完清 tx_active；
 *       某包被 bsp 判失败（result != BSP_OK）则当场结束本轮，不再续发。
 *       CANTransmit 同步等 mailbox/Tx FIFO 空间并拷入外设缓冲，故回调内续发不会覆盖已排队帧。
 */

#include "comm_media_can_pkt0.h"
#include "drv_comm.h" /* CommMediaRxHook：comm 层接收入口 */
#include "bsp_dwt.h"  /* DWT_GetTimeUs：发送入口自恢复探测的限频时基 */
#include "bsp_log.h"
#include <string.h>

#ifdef DRV_COMM_USED

/* 总线级自恢复探测的最小间隔（ms）：本后端的**发送入口**每次都会先看一眼（见 MediaCanPkt0ProbeBus），
 * 全靠这个周期限频。取值是两头权衡：CANRecover 的判据成立时会**放弃该总线上的在途帧**、还可能
 * Stop/Start 外设（见 bsp_can.h 的代价说明），不能按发送频率去试；但外设真卡在 bus-off /
 * 发送资源占死时，越早收口越早能再发帧，也不能太长。 */
#ifndef DRV_COMM_MEDIA_CAN_RECOVER_PERIOD_MS
#define DRV_COMM_MEDIA_CAN_RECOVER_PERIOD_MS 100
#endif

#ifndef DRV_COMM_MEDIA_CAN_PKT0_LOG_LIMIT
#define DRV_COMM_MEDIA_CAN_PKT0_LOG_LIMIT 10
#endif // !DRV_COMM_MEDIA_CAN_PKT0_LOG_LIMIT
LOG_INSTANCE_DEF(g_media_can_pkt0_log, "comm_media_can_pkt0", DRV_COMM_MEDIA_CAN_PKT0_LOG_LIMIT);

/* 每条总线（can_e）上一次探测的时刻。
 * @note 限频按**外设**去重而不是按实例：CANRecover 作用于整个外设（它取消的是该总线上
 *       所有实例的在途帧），同一条总线上挂多个东西时，一个共享节拍才是对的。 */
static uint64_t s_can_recover_us[CAN_NUM_MAX] = {0};

static int8_t MediaCanPkt0Send(CommMedia *media, const uint8_t *data);
static int8_t MediaCanPkt0SendNext(CommMediaCanPkt0 *m);
static void MediaCanPkt0TxHook(CANInstance *can, uint32_t tx_mailbox, BSP_Status_e result);
static void MediaCanPkt0ErrHook(CANInstance *can, CAN_ErrReason_e reason);
static void MediaCanPkt0RxHook(CANInstance *can, const CAN_Pack_s *pack);
static void MediaCanPkt0ProbeBus(CANInstance *can);

/* @note 本后端**刻意不挂 vtable->offline**（USART / USB / USB_SIMPLE 三个后端挂了）：
 *       offline 的触发条件是"这条链路没收到帧"，是**实例级**证据；而 CAN 的自恢复动作
 *       （CANRecover）作用于**整条总线**（取消该总线上所有实例的在途帧、必要时重停外设）。
 *       证据的作用域必须与动作的作用域对齐，否则"对端不发 / 滤波器不匹配 / 流量被同总线
 *       别的实例挤掉"都会把整条总线的在途帧打掉。故改为把探测放在**自己的发送入口**
 *       （任务上下文、每帧必经、"发不出去"正是要治的病），由 bsp 在入口内部自证总线级判据。
 *       连带效果：`CommConfig` 里"daemon_reload=0 提升为默认值"只对挂了 offline 钩子的
 *       后端生效，故 CAN 链路的 `daemon_reload = 0` 仍是真的"不监控"（见 drv_comm.c）。 */
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

    if (m->tx_id == CAN_ID_UNUSED)
        return -1; /* 未配置发送 ID：只收不发 */

    /* 发送前给 bsp 一次收口机会（任务上下文、每帧必经之处）：总线卡在 bus-off / 发送资源被
     * "发不出去"的帧占死时，先把发送能力救回来，本帧才可能发得出去。必须早于下面的
     * tx_active 判断——总线上不来时逐包续发的完成回调也不会来，tx_active 会永久为 1，
     * 若把探测放在它后面就永远够不到了。判据/动作/限频的理由见 MediaCanPkt0ProbeBus。 */
    MediaCanPkt0ProbeBus(can);

    if (m->tx_active)
    {
        /* 卡死兜底：分包续发依赖 bsp 发送完成回调，一旦该回调丢失（邮箱 RQCP 被同总线
         * 其他实例复用吞掉 / 总线异常），tx_active 会永久为 1，之后所有 Send 都被拒 →
         * 链路单向静默。连续 N 次 Send 仍见 tx_active=1 即判定卡死，强制放弃残帧
         * （接收端按分包序号错位丢帧重同步，仅损失一帧）后继续发新帧。 */
        if (++m->tx_stall >= CAN_MEDIA_PKT0_TX_STALL_LIMIT)
        {
            m->tx_stall = 0;
            m->tx_active = 0;
            m->tx_sent = 0;
            m->tx_stall_recover++; /* 调试观测：正常为 0 */
        }
        else
        {
            m->tx_fail++;
            return -1; /* 上一帧仍在异步分包发送中，拒绝重入 */
        }
    }

    memcpy(m->tx_buff, data, m->tx_frame_len); /* 整帧拷入自持缓冲（异步续发期间不失效） */
    m->tx_sent = 0;
    m->tx_active = 1;
    m->tx_stall = 0; /* 新帧开始，卡死计数归零 */

    if (MediaCanPkt0SendNext(m) != 0)
    {
        m->tx_active = 0; /* 首包发送失败：中止 */
        m->tx_fail++;
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

    if (CANTransmit(can, &pack, m->timeout_ms, NULL, NULL) != BSP_OK)
        return -1; /* 发送失败/资源忙/超时：中止分包（已发部分接收端丢帧重同步） */

    m->tx_sent += chunk; /* 记录发送到哪个位置 */
    return 0;
}

/* bsp 发送完成适配钩子（tx_complete_callback）：续发下一分包，直至整帧发完清 tx_active。
 * @note 在 CAN 中断上下文执行；CANTransmit 同步等 mailbox/FIFO 空间（上一片已发完必有空间）。
 * @note pkt0 为单通道串行分包发送，无需按 tx_mailbox 区分帧来源。 */
static void MediaCanPkt0TxHook(CANInstance *can, uint32_t tx_mailbox, BSP_Status_e result)
{
    CommMediaCanPkt0 *m = (CommMediaCanPkt0 *)can->parent; /* media 层设置的反向指针 */
    int8_t r;

    (void)tx_mailbox;
    if (m == NULL)
        return;

    /* 这一包没发出去（仲裁失败/发送错误/被取消/Tx Event 丢失）：当场结束本轮，不再续发。
     * 旧实现只有"发送成功"才回调，失败时什么也不知道，只能等 CAN_MEDIA_PKT0_TX_STALL_LIMIT
     * 次 Send 后由兜底判卡死——多丢 N 帧，且分不清"真卡死"与"总线一直在错"。 */
    if (result != BSP_OK)
    {
        m->tx_active = 0;
        m->tx_sent = 0;
        m->tx_fail++;
        return;
    }

    /* 本轮已被放弃（stall 兜底提前清过 tx_active，或此时已开始发新帧）：
     * 这是被丢弃残帧的迟到完成回调，不能再拿着当前的 tx_sent 续发。 */
    if (!m->tx_active)
        return;

    r = MediaCanPkt0SendNext(m);
    if (r != 0)
    {
        m->tx_active = 0; /* 发完(1) 或失败(-1)：结束本轮异步发送 */
        if (r < 0)
            m->tx_fail++; /* 分包续发失败：记录（接收端按分包序号错位丢帧重同步） */
    }
}

/* bsp 错误回调（总线/硬件级事件，ISR 上下文）：只做观测计数。
 * @note **刻意不在这里清 tx_active**：bsp 已经把"逐帧结果"通过 tx_complete_callback 送来了
 *       （被取消/判失败/溯源丢失的帧都会带 BSP_HW_ERR 回调），在这里另清一遍既冗余，
 *       又会引入"错误广播清掉 A 实例的 tx_active，而 A 的在途帧随后正常完成"这种
 *       跨实例误伤。真丢了完成回调的情形由 CAN_MEDIA_PKT0_TX_STALL_LIMIT 兜底。
 *       err_callback 是广播（同一条总线上的所有实例都会收到），所以 handler 里
 *       不能有任何"只对自己成立"的假设。 */
static void MediaCanPkt0ErrHook(CANInstance *can, CAN_ErrReason_e reason)
{
    CommMediaCanPkt0 *m = (CommMediaCanPkt0 *)can->parent;

    if (m == NULL)
        return;

    m->err_count++;
    m->last_err = (uint8_t)reason;
}

/**
 * @brief 发送前的总线级自恢复探测（任务上下文）——本层只管"何时看一眼"
 * @param can 本介质绑定的 CAN 实例（只用于指出**哪条总线**）
 *
 * @note **为什么搭在发送入口而不是 daemon 的 offline 钩子**：CANRecover 作用于**整条总线**
 *       （取消该总线上所有实例的在途帧、必要时重停外设，见 bsp_can.h），而 offline 钩子的
 *       触发条件是"这条链路没收到帧"——实例级证据。用实例级证据触发总线级动作会误伤：
 *       对端不发、滤波器不匹配、流量被同总线别的实例挤掉，都会让本链路"没收到帧"，而这三样
 *       恢复一个都治不好。发送入口是任务上下文、每帧必经，且"发不出去"正是本函数要治的病，
 *       与动作对症。
 * @note **判据不在这层**：本层只做限频（返回值不看内容、也不判"该不该恢复"），总线级判据
 *       （bus-off / 发送资源占满且发送错误计数器越界）由 bsp 在 CANRecover 入口自证，
 *       不成立时它返回 BSP_BUSY 且不碰任何在途帧 —— 也就是说**健康链路上本次探测是零代价的**
 *       （两次寄存器读）。这与 bsp_spi 的 SPIRecoverTxIfStuck 是同一条分工。
 * @note 返回值只用于日志分级：BSP_OK = 本次刚做过收口（bsp 内部已按 INFO 记账，本层不重复打）、
 *       BSP_HW_ERR = 做过收口但总线仍不可用（bsp 已计 recover_fail，本层补一条带链路上下文的
 *       ERROR，便于与"对端真掉线"区分）、BSP_BUSY = 没动作（绝大多数时候），不打日志、不刷屏。
 */
static void MediaCanPkt0ProbeBus(CANInstance *can)
{
    uint8_t idx;
    uint64_t now;

    if (can == NULL)
        return;

    idx = (uint8_t)can->can_e;
    if (idx >= CAN_NUM_MAX)
        return;

    now = DWT_GetTimeUs();
    if (s_can_recover_us[idx] != 0 &&
        (now - s_can_recover_us[idx]) < ((uint64_t)DRV_COMM_MEDIA_CAN_RECOVER_PERIOD_MS * 1000u))
        return; /* 限频：同一条总线这段时间内已经看过一眼 */
    s_can_recover_us[idx] = now;

    if (CANRecover(can) == BSP_HW_ERR)
        BSPLOG(&g_media_can_pkt0_log, LOG_LEVEL_ERROR,
               "CANRecover failed (can_e=%u), bus still unavailable", (unsigned)idx);
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
    if (CANRegister(can) != BSP_OK)
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
    media->tx_stall = 0;
    media->tx_fail = 0;
    media->tx_stall_recover = 0;
    media->err_count = 0;
    media->last_err = 0;
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
        .tx_complete_callback = MediaCanPkt0TxHook, /* 发送完成回调：逐包续发 + 逐帧失败当场收尾 */
        .err_callback = MediaCanPkt0ErrHook,        /* 总线/硬件级错误：仅观测计数（见 ErrHook 说明） */
    };

    if (CANConfig(can, &can_cfg) != BSP_OK)
        return -1;

    media->tx_id = cfg->tx_id;
    media->frame_type = cfg->frame_type;
    media->mode = cfg->mode;
    media->timeout_ms = cfg->timeout_ms; /* 完全按 Config 配置的超时时间使用 */
    return 0;
}

#endif /* DRV_COMM_USED */

/**
 * @file drv_comm.c
 * @brief 通信框架-顶层（CommInstance 统一入口）实现
 *
 * 两段式接口（对齐 bsp_usart 的 USARTRegister/USARTConfig 模式）：
 *   - CommRegister（不可重入）：media 后端注册 → proto 挂 vtable → 接线分发 + 登记看门狗
 *   - CommConfig  （可重入）  ：介质参数 + 链路对端看门狗 + 出帧回调（均可反复修改）
 *
 * 一条 comm = 一个双向对话：接收协议(rx_proto)与发送协议(tx_proto)分离，
 * 各自 payload 大小可不同（编译期确定，DEF 宏写入）。
 *
 * 接收链（无挂载表，跳过多余中转）：
 *   bsp ISR → media 适配钩子（长度校验）→ CommMediaRxHook → media->parent 反查 comm
 *     → rx_proto unpack → 出帧 → on_frame（业务回调由 CommConfig 挂到 rx_proto->on_frame）
 * 发送：CommSend → tx_proto pack（写 comm 打包缓冲）→ MediaSend（拷入 media 缓冲发出）
 */

#include "drv_comm.h"
#include "drv_daemon.h" /* 链路对端看门狗：注册/配置/喂狗统一在 comm 层 */
#include "bsp_log.h"
// 介质后端 Register/Config（协议后端经注册表 CommProtoBackendFind 分发，见 CommRegister）
#include "comm_media_usart.h"
#include "comm_media_usb.h"
#include "comm_media_usb_simple.h"
#include "comm_media_can_pkt0.h"
#include "comm_media_can_idseq.h"

#ifdef DRV_COMM_USED

/* daemon_reload 配 0 时的兜底值（单位：毫秒，即 100ms）：
 * 0 的本义是"禁用监控"——DaemonTask 会整个跳过该实例，等于把介质后端的离线自恢复
 * （如 USART 的接收停摆重启，见 CommMediaVTable_s.offline）一起禁掉。故 Config 对
 * **挂了 offline 钩子的后端**把 0 提升为该值（判定条件就是"钩子是否为空"，故不挂钩子的
 * 后端如两个 CAN 介质不受影响，见 CommConfig 里的说明）。
 * 该值同时是"多久没收到完整合法帧判离线"的阈值（离线日志 / fault_action / 对端在线查询都按它）。 */
#ifndef DRV_COMM_DAEMON_RELOAD_DEFAULT
#define DRV_COMM_DAEMON_RELOAD_DEFAULT 100
#endif
/* 兜底值自身不能再是 0 —— 否则"把 0 提升为兜底值"等于没提升，离线自恢复又会静默失效 */
_Static_assert(DRV_COMM_DAEMON_RELOAD_DEFAULT != 0,
               "DRV_COMM_DAEMON_RELOAD_DEFAULT must be non-zero (0 would silently disable offline self-heal)");

#ifndef DRV_COMM_LOG_LIMIT
#define DRV_COMM_LOG_LIMIT 10
#endif                                                     // !DRV_COMM_LOG_LIMIT
LOG_INSTANCE_DEF(g_comm_log, "drv_comm", DRV_COMM_LOG_LIMIT); // comm 日志实例

/* 从 CommInstance 取介质/协议基类指针（void* 指向派生实例，首成员即基类） */
#define COMM_INSTANCE_MEDIA(inst) ((CommMedia *)((inst)->media))
#define COMM_INSTANCE_RX_PROTO(inst) ((CommProto *)((inst)->rx_proto))
#define COMM_INSTANCE_TX_PROTO(inst) ((CommProto *)((inst)->tx_proto))

/*------------- 内部函数实现 -------------*/

/**
 * @brief 接收数据入队（UNPACK_IN_TASK 模式：不阻塞中断）
 * @todo 完整实现：接收队列（bsp_freertos）+ 共享 RX 任务解包（下一轮）
 * @note CommRegister 已在注册期拒绝 UNPACK_IN_TASK（本函数是空实现），走到这里说明
 *       接线被绕过。仍要报错而不是静默丢帧：后者表现为"链路看着通、就是收不到"，
 *       是最难查的一类故障。
 */
static void CommRxPush(CommInstance *inst, const uint8_t *data)
{
    (void)inst;
    (void)data;
    BSPLOG(&g_comm_log, LOG_LEVEL_ERROR, "UNPACK_IN_TASK not implemented yet, frame dropped!");
}

/**
 * @brief comm 层接收统一入口（media 后端适配钩子直接调用）
 * @note 经 media->parent（CommRegister 建立的反向指针）反查所属 comm 实例，
 *       按编译期配置的解包位置分流：ISR 直解 / 搬入队列由 RX 任务解包。
 *       一个 media 只属于一个 comm，无需挂载表。
 */
void CommMediaRxHook(CommMedia *media, const uint8_t *data)
{
    CommInstance *inst;
    CommProto *rx_proto;
    const uint8_t *payload;

    if (media == NULL)
        return;

    inst = (CommInstance *)media->parent;
    if (inst == NULL || inst->rx_proto == NULL)
        return; /* 协议层未接线：无从判断帧是否合法，也就没有"对端在线"的证据 */
    rx_proto = COMM_INSTANCE_RX_PROTO(inst);

    switch (inst->unpack_mode)
    {
    case UNPACK_IN_TASK:
        CommRxPush(inst, data); /* 待实现；CommRegister 已拒绝该模式，正常不可达 */
        break;
    case UNPACK_IN_ISR:
    default:
        /* ISR 直解：unpack 只解包，出帧回调由 comm 层统一调。
         * @warning payload 指向接收缓冲，回调返回后即被 bsp 清零——
         *          on_frame 必须同步消费，不可保存指针异步使用 */
        payload = ProtoUnpack(rx_proto, data);
        if (payload == NULL)
            break; /* 坏帧（长度/CRC/序号不合法）：不能证明对端在线，不喂狗。
                    * **重帧**（seq 与上一帧相同，ExtSeqCheck 判掉）也走这条：它确实来自
                    * 对端，但"同一帧的第 N 次投递"证明不了对端在推进新数据。而发送侧 seq
                    * 是每帧自增的（comm_proto_ext.c: out_buff[1] = tx_seq++），对端只要
                    * 还在按约定发，新 seq 就会持续到来 —— 所以这条判据不会因一次重传就判
                    * 出离线；本链路 2ms 一帧、10ms 判离线，要连续 5 帧全被丢弃才够。 */

        /* 链路对端看门狗喂狗：判据刻意取"协议层认下的帧"，而不是"media 收到一帧"。
         * media 侧只做长度/序号重组（见 MediaUsartRxHook：只比 rx_len），收到一截
         * 凑够长度、内容却是噪声的字节也算"一帧"；只有过了解包校验，才真正说明
         * 对端在按约定发数据。反过来，若把喂狗挪到 media 入口，一条持续吐垃圾的
         * 对端（假帧/回环噪声）会让看门狗一直"在线"，offline 钩子的自恢复永远不会触发。
         * 未配置 / 未登记看门狗时本入口空转。 */
        if (media->daemon != NULL)
            DaemonReload(media->daemon);

        if (rx_proto->on_frame)
            rx_proto->on_frame(payload);
        break;
    }
}

/*------------- 外部接口实现 -------------*/

int8_t CommRegister(CommInstance *inst)
{
    CommMedia *media;
    CommProto *rx_proto;

    if (inst == NULL || inst->media == NULL || inst->rx_proto == NULL || inst->tx_proto == NULL)
        return -1;

    /* 接收队列 + 共享 RX 任务解包（UNPACK_IN_TASK）尚未实现：注册期就明确拒绝。
     * 放行的代价是每一帧都在 CommRxPush 里被丢弃，而调用方只看到"注册成功、
     * 链路健康、收不到数据"，属最难定位的一类故障。 */
    if (inst->unpack_mode == UNPACK_IN_TASK)
    {
        BSPLOG(&g_comm_log, LOG_LEVEL_ERROR, "UNPACK_IN_TASK not implemented yet, use UNPACK_IN_ISR!");
        return -1;
    }

    media = COMM_INSTANCE_MEDIA(inst);
    rx_proto = COMM_INSTANCE_RX_PROTO(inst);

    /* 1. media 后端注册（不可重入：USART 内部做 bsp USARTRegister 防重复注册）
     * @note 按 inst->media_type 分发：COMM_DEF 静态定义时写入 media_type，
     *       不能用 media->type（CommMedia 基类无 type 字段，仅 vtable/parent/media） */
    switch (inst->media_type)
    {
    case MEDIA_USART:
        if (MediaUsartRegister((CommMediaUsart *)inst->media) != 0)
            return -1;
        break;
    case MEDIA_USB:
        if (MediaUsbRegister((CommMediaUsb *)inst->media) != 0)
            return -1;
        break;
    case MEDIA_USB_SIMPLE:
        if (MediaUsbSimpleRegister((CommMediaUsbSimple *)inst->media) != 0)
            return -1;
        break;
    case MEDIA_CAN_PKT0:
        if (MediaCanPkt0Register((CommMediaCanPkt0 *)inst->media) != 0)
            return -1;
        break;
    case MEDIA_CAN_IDSEQ:
        if (MediaCanIdseqRegister((CommMediaCanIdseq *)inst->media) != 0)
            return -1;
        break;
    default:
        return -1; /* 介质类型未支持 */
    }

    /* 2. 接收/发送协议后端初始化：查注册表（内置 RAW/CUSTOM + app 自定义统一分发）
     * @note 同上，避免用未初始化的基类字段；app 自定义协议须先 CommProtoRegisterBackend */
    {
        const CommProtoBackend_t *be = CommProtoBackendFind(inst->rx_proto_type);
        if (be == NULL || be->init(inst->rx_proto) != 0)
            return -1;
    }
    {
        const CommProtoBackend_t *be = CommProtoBackendFind(inst->tx_proto_type);
        if (be == NULL || be->init(inst->tx_proto) != 0)
            return -1;
    }

    /* 3. 接线：出帧回调由 CommConfig 挂到 rx_proto->on_frame；
     *    接收分发经 media->parent 反查（media 适配钩子直接调 CommMediaRxHook） */
    rx_proto->on_frame = NULL;

    /* 建立反向指针：media 回指所属 comm 实例（接收分发据此反查 rx_proto） */
    media->parent = inst;

    /* 登记链路对端看门狗：实例经 DEF 宏内嵌于 media（media->daemon）。
     * reload_count 由 CommConfig 决定（配 0 会被提升为默认值，见那里），故此处仅登记一次 */
    if (media->daemon != NULL)
        DaemonRegister(media->daemon);

    inst->inited = 1;
    return 0;
}

int8_t CommConfig(CommInstance *inst, const CommConfig_s *cfg)
{
    CommMedia *media;

    if (inst == NULL || inst->media == NULL || cfg == NULL)
        return -1;
    if (!inst->inited)
        return -1; /* 须先 CommRegister */
    media = COMM_INSTANCE_MEDIA(inst);

    /* 1. 介质参数（USART 需 media_cfg 非空才下发；USB 无运行期参数但须挂接收钩子）
     *    @note USB 的 MediaUsbConfig 内部强制接管接收回调，media_cfg 可为 NULL */
    switch (inst->media_type)
    {
    case MEDIA_USART:
        if (cfg->media_cfg != NULL)
        {
            if (MediaUsartConfig((CommMediaUsart *)inst->media, (CommMediaUsartConfig_s *)cfg->media_cfg) != 0)
                return -1;
        }
        break;
    case MEDIA_USB:
        if (MediaUsbConfig((CommMediaUsb *)inst->media, (USB_Config_s *)cfg->media_cfg) != 0)
            return -1;
        break;
    case MEDIA_USB_SIMPLE:
        if (MediaUsbSimpleConfig((CommMediaUsbSimple *)inst->media, (USB_Config_s *)cfg->media_cfg) != 0)
            return -1;
        break;
    case MEDIA_CAN_PKT0:
        if (MediaCanPkt0Config((CommMediaCanPkt0 *)inst->media, (CommMediaCanPkt0Config_s *)cfg->media_cfg) != 0)
            return -1;
        break;
    case MEDIA_CAN_IDSEQ:
        if (MediaCanIdseqConfig((CommMediaCanIdseq *)inst->media, (CommMediaCanIdseqConfig_s *)cfg->media_cfg) != 0)
            return -1;
        break;
    default:
        return -1; /* 介质类型未支持 */
    }

    /* 2. 链路对端看门狗参数（统一配置，可重入：反复调用改 reload/fault）。
     *    owner_id 填 comm 实例，离线日志/回调据此识别所属链路。
     *    callback 取介质后端的 offline 钩子（如 USART 的接收停摆重启；无则 NULL） */
    if (media->daemon != NULL)
    {
        uint16_t daemon_reload = cfg->daemon_reload;
        offline_callback offline_hook = (media->vtable != NULL) ? media->vtable->offline : NULL;

        /* reload==0 的本义是"禁用监控"（DaemonTask 跳过该实例 = 恒在线）。但 USART / USB /
         * USB_SIMPLE 三个后端的 offline 钩子**只有** daemon 这一个任务上下文周期时基
         * （"没收到帧"才触发它），禁用等于把发送卡死收尾 / 接收重挂一起禁掉——
         * 故对挂了钩子的后端把 0 提升为 DRV_COMM_DAEMON_RELOAD_DEFAULT。
         * @note 两个 CAN 后端**刻意不挂 offline 钩子**，所以 CAN 链路的 daemon_reload = 0
         *       仍然是真正的"不监控"。理由（与 bsp_spi 的 SPIRecoverTxIfStuck 同一条原则）：
         *       offline 的触发条件是"这条链路没收到帧"= **实例级**证据，而 CANRecover 取消的是
         *       **整条总线**上所有实例的在途帧 = **总线级**动作；证据的作用域必须与动作的
         *       作用域对齐，否则对端不发 / 滤波器不匹配 / 流量被同总线别的实例挤掉，都会把
         *       整条总线的在途帧打掉。它们的探测改搭在自己的发送入口上
         *       （见 comm_media_can_pkt0.c / comm_media_can_idseq.c 的 ProbeBus），
         *       总线级判据由 bsp 在 CANRecover 入口自证。 */
        /* @note 判定条件仍按"是否挂了钩子"写：将来若新增不带钩子的后端，配 0 就是真不监控。 */
        if (daemon_reload == 0 && offline_hook != NULL)
        {
            daemon_reload = DRV_COMM_DAEMON_RELOAD_DEFAULT;
            BSPLOG(&g_comm_log, LOG_LEVEL_WARNING,
                   "daemon_reload=0 disables offline self-heal, forced to %u", daemon_reload);
        }

        Daemon_Config_s daemon_cfg = {
            .reload_count = daemon_reload,
            .fault_action = cfg->daemon_fault,
            .callback = offline_hook,
            .owner_id = inst,
        };
        DaemonConfig(media->daemon, &daemon_cfg);
    }

    /* 3. 出帧消费回调（可重入：直接覆盖 rx_proto->on_frame，运行期可修改） */
    if (cfg->on_frame != NULL)
    {
        COMM_INSTANCE_RX_PROTO(inst)->on_frame = cfg->on_frame;
    }
    return 0;
}

int8_t CommSend(CommInstance *inst, const uint8_t *payload)
{
    CommProto *tx_proto;
    CommMedia *media;

    if (inst == NULL || inst->tx_proto == NULL || payload == NULL)
        return -1;

    /* 打包（vtable->pack，payload → inst->tx_buff）→ media 后端发出
     * @note 发送协议->media 由 COMM_DEF 宏静态绑定（tx_proto->media）。
     *       MediaSend 把 comm 打包缓冲 data 交给后端：USART 拷入自持 staging
     *       （DMA 异步发送期间不失效）；USB 直接引用 data 分包（本函数运行期间有效）。 */
    tx_proto = (CommProto *)inst->tx_proto;
    if (inst->tx_buff == NULL || ProtoPack(tx_proto, payload, inst->tx_buff) != 0)
        return -1;

    media = (CommMedia *)tx_proto->media;
    if (media == NULL)
        return -1;
    return MediaSend(media, inst->tx_buff);
}

#endif /* DRV_COMM_USED */

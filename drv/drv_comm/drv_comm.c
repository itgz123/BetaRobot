/**
 * @file drv_comm.c
 * @brief 通信框架-顶层（CommInstance 统一入口）实现
 *
 * 两段式接口（对齐 bsp_usart 的 USARTRegister/USARTConfig 模式）：
 *   - CommRegister（不可重入）：media 后端注册 → proto 挂 vtable → 接线分发
 *   - CommConfig  （可重入）  ：介质参数 + 出帧回调（均可反复修改）
 *
 * 一条 comm = 一个双向对话：接收协议(rx_proto)与发送协议(tx_proto)分离，
 * 各自 payload 大小可不同（编译期确定，DEF 宏写入）。
 *
 * 分发链路（无挂载表：media 收到数据 → 经 media->parent 反查所属 comm 实例）：
 *   接收：bsp ISR → media 适配钩子 → MediaHandleRx → CommMediaRxHook
 *         → media->parent 反查 comm → rx_proto unpack → 出帧 → on_frame
 *         （业务回调由 CommConfig 直接挂到 rx_proto->on_frame）
 *   发送：CommSend → tx_proto pack → MediaSend（tx_proto->media 由 DEF 宏绑定）
 */

#include "drv_comm.h"
// 各后端 Register/Init（token 拼接分发的具体实现在这里按类型调用）
#include "comm_media_usart.h"
#include "comm_proto_raw.h"

#ifdef DRV_COMM_USED

/* 从 CommInstance 取介质/协议基类指针（void* 指向派生实例，首成员即基类） */
#define COMM_INSTANCE_MEDIA(inst) ((CommMedia *)((inst)->media))
#define COMM_INSTANCE_RX_PROTO(inst) ((CommProto *)((inst)->rx_proto))
#define COMM_INSTANCE_TX_PROTO(inst) ((CommProto *)((inst)->tx_proto))

/*------------- 内部函数实现 -------------*/

/**
 * @brief 接收数据入队（UNPACK_IN_TASK 模式：不阻塞中断）
 * @todo 完整实现：接收队列（bsp_freertos）+ 共享 RX 任务解包（下一轮）
 */
static void CommRxPush(CommInstance *inst, const uint8_t *data)
{
    (void)inst;
    (void)data;
}

/**
 * @brief 接收分发钩子（挂到 media->rx_cb）
 * @note 经 media->parent（CommRegister 建立的反向指针）反查所属 comm 实例，
 *       按编译期配置的解包位置分流：ISR 直解 / 搬入队列由 RX 任务解包。
 *       一个 media 只属于一个 comm，无需挂载表。
 */
static void CommMediaRxHook(CommMedia *media, const uint8_t *data)
{
    CommInstance *inst = (CommInstance *)media->parent;
    if (inst == NULL || inst->rx_proto == NULL)
        return;

    switch (inst->unpack_mode)
    {
    case UNPACK_IN_TASK:
        CommRxPush(inst, data); /* 搬入接收队列，由 RX 任务解包（待实现） */
        break;
    case UNPACK_IN_ISR:
    default:
        ProtoUnpack(COMM_INSTANCE_RX_PROTO(inst), data); /* ISR 直解 */
        break;
    }
}

/*------------- 外部接口实现 -------------*/

int8_t CommRegister(CommInstance *inst)
{
    CommMedia *media;
    CommProto *rx_proto;
    CommProto *tx_proto;

    if (inst == NULL || inst->media == NULL || inst->rx_proto == NULL || inst->tx_proto == NULL)
        return -1;

    media = COMM_INSTANCE_MEDIA(inst);
    rx_proto = COMM_INSTANCE_RX_PROTO(inst);
    tx_proto = COMM_INSTANCE_TX_PROTO(inst);

    /* 1. media 后端注册（不可重入：USART 内部做 bsp USARTRegister 防重复注册）
     * @note 按 inst->media_type 分发，不能用 media->type：DEF 宏静态定义时
     *       尚未写入，默认 0 会误判为 MEDIA_CAN */
    switch (inst->media_type)
    {
    case MEDIA_USART:
        if (MediaUsartRegister((CommMediaUsart *)inst->media) != 0)
            return -1;
        break;
    default:
        return -1; /* 介质类型未支持 */
    }

    /* 2. 接收/发送协议后端初始化（各自挂 vtable），按类型分发
     * @note 同上，避免用未初始化的基类字段 */
    switch (inst->rx_proto_type)
    {
    case PROTO_RAW:
        if (CommProtoRawInit((CommProtoRaw *)inst->rx_proto) != 0)
            return -1;
        break;
    default:
        return -1; /* 接收协议类型未支持 */
    }
    switch (inst->tx_proto_type)
    {
    case PROTO_RAW:
        if (CommProtoRawInit((CommProtoRaw *)inst->tx_proto) != 0)
            return -1;
        break;
    default:
        return -1; /* 发送协议类型未支持 */
    }

    /* 3. 接线：media 接管接收钩子；出帧回调由 CommConfig 挂到 rx_proto->on_frame */
    media->rx_cb = CommMediaRxHook;
    rx_proto->on_frame = NULL;

    /* 建立反向指针：media/proto 回指所属 comm 实例（接收分发据此反查 rx_proto） */
    media->parent = inst;
    rx_proto->parent = inst;
    tx_proto->parent = inst;
    inst->inited = 1;
    return 0;
}

int8_t CommConfig(CommInstance *inst, const CommConfig_s *cfg)
{
    if (inst == NULL || inst->media == NULL || cfg == NULL)
        return -1;
    if (!inst->inited)
        return -1; /* 须先 CommRegister */

    /* 1. 介质参数（media_cfg 非空才下发；可重入，运行期可改波特率/发送模式等） */
    if (cfg->media_cfg != NULL)
    {
        switch (inst->media_type)
        {
        case MEDIA_USART:
            if (MediaUsartConfig((CommMediaUsart *)inst->media, (USART_Config_s *)cfg->media_cfg) != 0)
                return -1;
            break;
        default:
            return -1; /* 介质类型未支持 */
        }
    }

    /* 2. 出帧消费回调（可重入：直接覆盖 rx_proto->on_frame，运行期可修改） */
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

    /* 打包（vtable->pack，payload → inst->tx_buff）→ 拷贝到 media 缓冲发出
     * @note 发送协议->media 由 COMM_DEF 宏静态绑定（tx_proto->media）。
     *       MediaSend 把 comm 打包缓冲拷入 media->tx_buff：media 缓冲常驻，
     *       DMA 异步发送期间数据不失效；分包后端用状态机+发送完成回调续发。 */
    tx_proto = (CommProto *)inst->tx_proto;
    if (inst->tx_buff == NULL || ProtoPack(tx_proto, payload, inst->tx_buff) != 0)
        return -1;

    media = (CommMedia *)tx_proto->media;
    if (media == NULL || media->tx_buff == NULL)
        return -1;
    return MediaSend(media, inst->tx_buff);
}

#endif /* DRV_COMM_USED */

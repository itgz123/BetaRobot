/**
 * @file drv_comm.c
 * @brief 通信框架-顶层（CommInstance 统一入口）实现
 *
 * 两段式接口（对齐 bsp_usart 的 USARTRegister/USARTConfig 模式）：
 *   - CommRegister（不可重入）：media 后端注册 → proto 挂 vtable → 接线分发
 *   - CommConfig  （可重入）  ：介质参数 + 出帧回调（均可反复修改）
 *
 * 分发链路（原独立 engine 层收编到本层，删除了纯冗余的抽象）：
 *   接收：bsp ISR → media 适配钩子 → MediaHandleRx → CommMediaRxHook
 *         → 遍历挂载表 → ProtoUnpack → 出帧 → CommProtoFrameHook
 *         → 消费者表匹配 → 消费回调
 *   发送：CommSend → ProtoSend（proto->media 由 DEF 宏绑定）→ MediaSend
 */

#include "drv_comm.h"
// 各后端 Register/Init（token 拼接分发的具体实现在这里按类型调用）
#include "comm_media_usart.h"
#include "comm_proto_raw.h"

#ifdef DRV_COMM_USED

/*------------- 内部结构 -------------*/

/* media↔proto 挂载表（接收分发用：media 收到数据 → 喂给挂载的 proto） */
typedef struct
{
    CommMedia *media; /* 介质基类指针 */
    CommProto *proto; /* 协议基类指针 */
} CommLink_s;

/* 出帧消费者表（按 proto 匹配，可运行期覆盖更新） */
typedef struct
{
    CommProto *proto;            /* 监听的协议 */
    ProtoFrameCallback consumer; /* 消费回调 */
} CommConsumer_s;

/*------------- 静态实例 -------------*/

static CommLink_s s_links[COMM_LINK_NUM];
static uint8_t s_link_cnt = 0;

static CommConsumer_s s_consumers[COMM_CONSUMER_NUM];
static uint8_t s_consumer_cnt = 0;

/*------------- 内部函数声明 -------------*/

static void CommMediaRxHook(CommMedia *media, const uint8_t *data);
static void CommProtoFrameHook(CommProto *proto, const uint8_t *payload);
static int8_t CommSetConsumer(CommProto *proto, ProtoFrameCallback cb);

/*------------- 内部函数实现 -------------*/

/**
 * @brief 接收分发钩子（挂到 media->rx_cb）
 * @note 遍历该 media 挂载的所有 proto 逐个喂解包；
 *       各协议靠自身帧校验拒绝不属于自己的字节。
 */
static void CommMediaRxHook(CommMedia *media, const uint8_t *data)
{
    for (uint8_t i = 0; i < s_link_cnt; i++)
    {
        if (s_links[i].media == media)
        {
            ProtoUnpack(s_links[i].proto, data);
        }
    }
}

/**
 * @brief 出帧分发钩子（挂到 proto->on_frame）
 * @note 解出一条完整 payload 后，按 proto 匹配消费者表逐个回调。
 */
static void CommProtoFrameHook(CommProto *proto, const uint8_t *payload)
{
    for (uint8_t i = 0; i < s_consumer_cnt; i++)
    {
        if (s_consumers[i].proto == proto && s_consumers[i].consumer)
        {
            s_consumers[i].consumer(proto, payload);
        }
    }
}

/**
 * @brief 设置出帧消费者回调（可重入：同 proto 重复调用即覆盖更新）
 */
static int8_t CommSetConsumer(CommProto *proto, ProtoFrameCallback cb)
{
    if (proto == NULL || cb == NULL)
        return -1;

    for (uint8_t i = 0; i < s_consumer_cnt; i++)
    {
        if (s_consumers[i].proto == proto)
        {
            s_consumers[i].consumer = cb;
            return 0;
        }
    }

    if (s_consumer_cnt >= COMM_CONSUMER_NUM)
        return -1;

    s_consumers[s_consumer_cnt].proto = proto;
    s_consumers[s_consumer_cnt].consumer = cb;
    s_consumer_cnt++;
    return 0;
}

/*------------- 外部接口实现 -------------*/

int8_t CommRegister(CommInstance *inst)
{
    CommMedia *media;
    CommProto *proto;

    if (inst == NULL || inst->media == NULL || inst->proto == NULL)
        return -1;

    media = COMM_INSTANCE_MEDIA(inst);
    proto = COMM_INSTANCE_PROTO(inst);

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

    /* 2. proto 后端初始化（挂 vtable），按 inst->proto_type 分发
     * @note 同上，避免用未初始化的基类字段 */
    switch (inst->proto_type)
    {
    case PROTO_RAW:
        if (CommProtoRawInit((CommProtoRaw *)inst->proto) != 0)
            return -1;
        break;
    default:
        return -1; /* 协议类型未支持 */
    }

    /* 3. 接线分发：media 接管接收钩子 → proto 接管出帧钩子 → 建挂载
     * @note 挂载表静态零初始化，无需 EngineInit */
    media->rx_cb = CommMediaRxHook;
    proto->on_frame = CommProtoFrameHook;
    if (s_link_cnt >= COMM_LINK_NUM)
        return -1;
    s_links[s_link_cnt].media = media;
    s_links[s_link_cnt].proto = proto;
    s_link_cnt++;

    /* 建立反向指针：media/proto 回指所属 comm 实例 */
    media->parent = inst;
    proto->parent = inst;
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

    /* 2. 出帧消费回调（可重入：按 proto 覆盖式注册，运行期可修改） */
    if (cfg->on_frame != NULL)
    {
        if (CommSetConsumer(COMM_INSTANCE_PROTO(inst), cfg->on_frame) != 0)
            return -1;
    }
    return 0;
}

int8_t CommSend(CommInstance *inst, const uint8_t *payload)
{
    if (inst == NULL || inst->proto == NULL || payload == NULL)
        return -1;
    /* proto->media 由 COMM_DEF 宏静态绑定，直接经该协议打包并发送 */
    return ProtoSend((CommProto *)inst->proto, payload);
}

#endif /* DRV_COMM_USED */

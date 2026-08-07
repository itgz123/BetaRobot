/**
 * @file comm_engine.c
 * @brief 通信框架-引擎层（Engine）实现
 *
 * 统一接线链路：
 *   bsp ISR → media 适配钩子 → MediaHandleRx → 引擎 rx 钩子（EngineMediaRxHook）
 *   → 遍历该 media 挂载的 proto → ProtoUnpack → 出帧 → 引擎分发钩子（EngineProtoFrameHook）
 *   → 消费者表匹配 → 消费回调。
 *
 * 发送：EngineSend → 查 media 挂载的第一个 proto → ProtoSend → MediaSend。
 */

#include "comm_engine.h"
#include <string.h>

#ifdef DRV_COMM_USED

/*------------- 内部结构 -------------*/

typedef struct
{
    CommMedia *media; /* 介质基类指针 */
    CommProto *proto; /* 协议基类指针 */
} EngineLink_s;

typedef struct
{
    CommProto *proto;            /* 监听的协议 */
    ProtoFrameCallback consumer; /* 消费回调 */
} EngineConsumer_s;

/*------------- 静态实例 -------------*/

static EngineLink_s s_links[ENGINE_LINK_NUM];
static uint8_t s_link_cnt = 0;

static EngineConsumer_s s_consumers[ENGINE_CONSUMER_NUM];
static uint8_t s_consumer_cnt = 0;

/*------------- 内部函数声明 -------------*/

static void EngineMediaRxHook(CommMedia *media, const uint8_t *data);
static void EngineProtoFrameHook(CommProto *proto, const uint8_t *payload);

/*------------- 内部函数实现 -------------*/

/**
 * @brief 引擎接收钩子（挂到 media->rx_cb）
 * @note 遍历该 media 挂载的所有 proto 逐个喂解包；
 *       各协议靠自身帧校验拒绝不属于自己的字节。
 */
static void EngineMediaRxHook(CommMedia *media, const uint8_t *data)
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
 * @brief 引擎出帧分发钩子（挂到 proto->on_frame）
 * @note 解出一条完整 payload 后，按 proto 匹配消费者表逐个回调。
 */
static void EngineProtoFrameHook(CommProto *proto, const uint8_t *payload)
{
    for (uint8_t i = 0; i < s_consumer_cnt; i++)
    {
        if (s_consumers[i].proto == proto && s_consumers[i].consumer)
        {
            s_consumers[i].consumer(proto, payload);
        }
    }
}

/*------------- 外部接口实现 -------------*/

int8_t EngineInit(void)
{
    memset(s_links, 0, sizeof(s_links));
    memset(s_consumers, 0, sizeof(s_consumers));
    s_link_cnt = 0;
    s_consumer_cnt = 0;
    return 0;
}

int8_t EngineAttachMedia(CommMedia *media)
{
    if (media == NULL)
        return -1;

    media->rx_cb = EngineMediaRxHook; /* 引擎接管接收分发 */
    return 0;
}

int8_t EngineAttachProtocol(CommProto *proto, CommMedia *media)
{
    if (proto == NULL || media == NULL)
        return -1;
    if (s_link_cnt >= ENGINE_LINK_NUM)
        return -1;

    s_links[s_link_cnt].media = media;
    s_links[s_link_cnt].proto = proto;
    s_link_cnt++;

    proto->on_frame = EngineProtoFrameHook; /* 引擎接管出帧分发 */
    return 0;
}

int8_t EngineRegisterConsumer(CommProto *proto, ProtoFrameCallback cb)
{
    if (proto == NULL || cb == NULL)
        return -1;

    /* 同 proto 已注册则覆盖（可重入：运行期修改消费回调） */
    for (uint8_t i = 0; i < s_consumer_cnt; i++)
    {
        if (s_consumers[i].proto == proto)
        {
            s_consumers[i].consumer = cb;
            return 0;
        }
    }

    if (s_consumer_cnt >= ENGINE_CONSUMER_NUM)
        return -1;

    s_consumers[s_consumer_cnt].proto = proto;
    s_consumers[s_consumer_cnt].consumer = cb;
    s_consumer_cnt++;
    return 0;
}

int8_t EngineSend(CommMedia *media, const uint8_t *payload)
{
    if (media == NULL || payload == NULL)
        return -1;

    /* 取该 media 挂载的第一个协议发送 */
    for (uint8_t i = 0; i < s_link_cnt; i++)
    {
        if (s_links[i].media == media)
        {
            return ProtoSend(s_links[i].proto, payload);
        }
    }
    return -1;
}

#endif /* DRV_COMM_USED */

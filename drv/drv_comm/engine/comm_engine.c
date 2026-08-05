/**
 * @file comm_engine.c
 * @brief 引擎层实现
 */

#include "comm_engine.h"
#include "drv_comm.h"
#include "comm_media.h"
#include "comm_proto.h"
#include "comm_ring.h"

#include "bsp_uart_log.h"
#include <string.h>

/* 消费者表项 */
typedef struct
{
    EngineConsumerType_e type;
    CommId_t comm_id;
    QueueHandle_t queue;
    uint16_t item_size;
    EngineConsumerFn callback;
    void *ctx;
    uint8_t *latest_buff;
    uint16_t latest_size;
    uint8_t in_use;
} EngineConsumer_s;

static CommRing s_ring;
static TaskHandle_t s_rx_task = NULL;
static EngineConsumer_s s_consumers[ENGINE_CONSUMER_NUM];
static uint8_t s_consumer_cnt = 0;

static void EngineRxHook(CommMedia *media, MediaEvent_e evt, const uint8_t *data, uint16_t len);

/*------------------------------------------------ 消费者分发 ----------------*/

static void EngineDispatch(const ProtoMessage_s *msg)
{
    for (uint8_t i = 0; i < s_consumer_cnt; i++)
    {
        EngineConsumer_s *c = &s_consumers[i];
        if (!c->in_use)
        {
            continue;
        }
        if (c->comm_id != COMM_ID_ANY && c->comm_id != msg->comm_id)
        {
            continue;
        }
        switch (c->type)
        {
        case ENGINE_CONSUMER_QUEUE:
            if (c->queue && c->item_size == msg->len)
            {
                xQueueSend(c->queue, msg->payload, 0);
            }
            break;
        case ENGINE_CONSUMER_CALLBACK:
            if (c->callback)
            {
                c->callback(msg, c->ctx);
            }
            break;
        case ENGINE_CONSUMER_LATEST:
            if (c->latest_buff)
            {
                uint16_t n = (msg->len < c->latest_size) ? msg->len : c->latest_size;
                memcpy(c->latest_buff, msg->payload, n);
            }
            break;
        default:
            break;
        }
    }
}

/*------------------------------------------------ 接收处理 ----------------*/

static void EngineRxProcess(void)
{
    uint32_t media_id;
    uint16_t len;
    uint8_t buf[COMM_RING_CHUNK_SIZE];

    while (CommRingPop(&s_ring, &media_id, buf, &len) > 0)
    {
        CommMedia *media = MediaFindById((uint8_t)media_id);
        if (!media)
        {
            continue;
        }
        for (uint8_t i = 0; i < media->proto_count; i++)
        {
            ProtoMessage_s msg;
            if (ProtocolFeed(media->proto_list[i], buf, len, &msg, media_id) == PROTO_PARSE_OK)
            {
                EngineDispatch(&msg);
            }
        }
    }
}

/*------------------------------------------------ 接口实现 ----------------*/

int8_t EngineInit(void)
{
    CommRingInit(&s_ring);
    memset(s_consumers, 0, sizeof(s_consumers));
    s_consumer_cnt = 0;
    s_rx_task = NULL;
    return 0;
}

int8_t EngineAttachMedia(CommMedia *media)
{
    if (!media)
    {
        return -1;
    }
    if (MediaRegister(media) != 0)
    {
        return -1;
    }
    media->rx_cb = EngineRxHook; /* ISR 把 chunk 写 ring */
    return 0;
}

int8_t EngineAttachProtocol(CommMedia *media, CommProto *proto)
{
    if (!media || !proto)
    {
        return -1;
    }
    if (ProtocolRegister(proto) != 0)
    {
        return -1;
    }
    if (MediaAttachProtocol(media, proto) != 0)
    {
        return -1;
    }
    return 0;
}

int8_t EngineRegisterConsumer(const EngineConsumer_Config_s *cfg)
{
    if (!cfg || s_consumer_cnt >= ENGINE_CONSUMER_NUM)
    {
        return -1;
    }
    EngineConsumer_s *c = &s_consumers[s_consumer_cnt];
    c->type = cfg->type;
    c->comm_id = cfg->comm_id;
    c->queue = cfg->queue;
    c->item_size = cfg->item_size;
    c->callback = cfg->callback;
    c->ctx = cfg->ctx;
    c->latest_buff = cfg->latest_buff;
    c->latest_size = cfg->latest_size;
    c->in_use = 1;
    s_consumer_cnt++;
    return (int8_t)(s_consumer_cnt - 1);
}

int8_t EngineSend(uint8_t media_id, CommId_t comm_id, const uint8_t *payload, uint16_t len)
{
    static uint8_t tx_buf[COMM_TX_BUF_SIZE]; /* 发送缓冲：任务上下文调用 */

    CommMedia *media = MediaFindById(media_id);
    if (!media || media->proto_count == 0)
    {
        LOGWARNING("[comm_engine] media not found or no proto attached!");
        return -1;
    }
    CommProto *proto = media->proto_list[0];
    int16_t n = ProtocolPack(proto, comm_id, payload, len, tx_buf);
    if (n < 0)
    {
        LOGWARNING("[comm_engine] pack failed!");
        return -1;
    }
    return MediaSend(media, tx_buf, (uint16_t)n, 10);
}

void EngineRxTask(void)
{
    if (!s_rx_task)
    {
        s_rx_task = xTaskGetCurrentTaskHandle();
    }
    for (;;)
    {
        EngineRxProcess();
        /* 等 ISR 通知；超时兜底 1ms 轮询，防止启动早期通知丢失 */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1));
    }
}

/*------------------------------------------------ ISR 钩子 ----------------*/

static void EngineRxHook(CommMedia *media, MediaEvent_e evt, const uint8_t *data, uint16_t len)
{
    (void)evt;
    if (CommRingPush(&s_ring, media->media_id, data, len))
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (s_rx_task)
        {
            vTaskNotifyGiveFromISR(s_rx_task, &xHigherPriorityTaskWoken);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @file comm_media_can.c
 * @brief 硬件层 CAN 后端实现
 *
 * @note 接收路径（ISR 上下文）：bsp_can 收到帧 -> MediaCanRxHook
 *       -> media->rx_cb（引擎钩子写 ring）。每帧 8 字节作为单个 chunk 喂协议层，
 *       长帧跨 CAN 帧拼接由协议层状态机完成（等价 can_comm 分包）。
 *       send 单帧 ≤ 8 字节；更长数据需由协议层/发送侧分段后多次调用。
 */

#include "comm_media_can.h"
#include "drv_comm.h"

#include "bsp_uart_log.h"
#include <string.h>

static void MediaCanInit(CommMedia *inst)
{
    (void)inst; /* bsp 注册已在 Config 中完成 */
}

static int8_t MediaCanSend(CommMedia *inst, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    CommMediaCan *mc = COMM_CONTAINER_OF(inst, CommMediaCan);
    if (!data || len == 0 || len > 8)
    {
        LOGWARNING("[comm_media_can] send params invalid, len=%d (>8 需分段发送)", len);
        return -1;
    }
    memcpy(mc->can.tx_buff, data, len);
    CANSetDLC(&mc->can, (uint8_t)len);
    return CANTransmit(&mc->can, timeout_ms) ? 0 : -1;
}

static void MediaCanDeinit(CommMedia *inst)
{
    (void)inst;
}

static const CommMediaVTable_s s_media_can_vtable = {
    .init = MediaCanInit,
    .send = MediaCanSend,
    .deinit = MediaCanDeinit,
};

/* bsp_can 接收回调：ISR 上下文 */
static void MediaCanRxHook(CANInstance *can_inst)
{
    CommMediaCan *mc = (CommMediaCan *)can_inst->parent;
    if (!mc)
    {
        return;
    }
    CommMedia *media = &mc->media;
    if (media->rx_cb)
    {
        media->rx_cb(media, MEDIA_EVT_RX_FRAME, can_inst->rx_buff, can_inst->rx_len);
    }
}

int8_t MediaCanConfig(CommMedia *inst, const CommMediaCan_Config_s *cfg)
{
    CommMediaCan *mc = COMM_CONTAINER_OF(inst, CommMediaCan);
    if (!inst || !cfg)
    {
        LOGERROR("[comm_media_can] inst or cfg is NULL!");
        return -1;
    }

    inst->vtable = &s_media_can_vtable;
    inst->type = MEDIA_CAN;
    inst->kind = MEDIA_KIND_MESSAGE;
    inst->media_id = cfg->media_id;
    inst->unpack_in_isr = cfg->unpack_in_isr;

    CANRegister(&mc->can); /* bsp 注册（防重复），保证中断分发可用 */

    mc->can.parent = mc; /* bsp 回调通过 parent 找回派生实例 */
    CAN_Config_s can_cfg = {
        .can_e = cfg->can_e,
        .tx_id = cfg->tx_id,
        .filter_mode = cfg->filter_mode,
        .rx_callback = MediaCanRxHook,
        .rx_mask = cfg->rx_mask,
    };
    memcpy(can_cfg.rx_id_list, cfg->rx_id_list, sizeof(can_cfg.rx_id_list));
    if (CANConfig(&mc->can, &can_cfg) != 0)
    {
        LOGERROR("[comm_media_can] CAN config failed!");
        return -1;
    }
    return 0;
}

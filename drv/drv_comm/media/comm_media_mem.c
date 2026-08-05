/**
 * @file comm_media_mem.c
 * @brief 硬件层内存流后端实现
 *
 * send 把打包好的帧同步注入本介质的接收端（media->rx_cb），
 * 由引擎 RX 任务完成解包与分发。内存流介质天然不丢帧。
 */

#include "comm_media_mem.h"
#include "drv_comm.h"

#include "bsp_uart_log.h"

static void MediaMemInit(CommMedia *inst)
{
    (void)inst;
}

static int8_t MediaMemSend(CommMedia *inst, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (!inst || !data || len == 0)
    {
        return -1;
    }
    if (inst->rx_cb)
    {
        inst->rx_cb(inst, MEDIA_EVT_RX_STREAM, data, len); /* 同步注入接收通道 */
    }
    return 0;
}

static void MediaMemDeinit(CommMedia *inst)
{
    (void)inst;
}

static const CommMediaVTable_s s_media_mem_vtable = {
    .init = MediaMemInit,
    .send = MediaMemSend,
    .deinit = MediaMemDeinit,
};

int8_t MediaMemConfig(CommMedia *inst, const CommMediaMem_Config_s *cfg)
{
    if (!inst || !cfg)
    {
        LOGERROR("[comm_media_mem] inst or cfg is NULL!");
        return -1;
    }
    inst->vtable = &s_media_mem_vtable;
    inst->type = MEDIA_MEM;
    inst->kind = MEDIA_KIND_BYTESTREAM;
    inst->media_id = cfg->media_id;
    inst->unpack_in_isr = cfg->unpack_in_isr;
    return 0;
}

/**
 * @file bsp_usb.c
 * @brief USB 顶层：实例注册表 + HAL 弱回调桥接 + FIFO 配置 + Start/Connect
 *
 * 唯一触碰 PCD HAL 弱回调的顶层文件。三块板差异全部经 bsp_map 的
 * usb_map[] 句柄经 bsp_map 映射，协议栈配额（USB_EP_MAX / USB_*_FIFO_SIZE）
 * 用 bsp_usb_types.h 默认值，源码零平台分支。
 *
 * FIFO 配置（RX80 + TX0-16 + TX1-32 + TX2-8 = 136 字）在 USBStart 与
 * HAL_PCD_ResetCallback 各配一次：删掉中间件后 usb_otg.c 不配 FIFO，
 * 而 USB 总线复位会清掉 FIFO 配置，Reset 回调必须重配否则枚举失败。
 */

#include "bsp_usb.h"
#include "app_cfg.h"
#include "bsp_usb_core.h"
#include "bsp_usb_cdc.h"
#include "bsp_usb_ep.h"

#ifdef BSP_USB_USED

/*------------- 实例注册表 --------------*/
static USBInstance *s_usb_instances[USB_INSTANCE_NUM];

static USBInstance *USB_FindByHandle(const PCD_HandleTypeDef *hpcd)
{
    for (uint32_t i = 0; i < USB_INSTANCE_NUM; i++)
    {
        if (s_usb_instances[i] != NULL && s_usb_instances[i]->handle == hpcd)
        {
            return s_usb_instances[i];
        }
    }
    return NULL;
}

static void USB_EPDefaults(USBInstance *inst)
{
    for (uint8_t n = 0; n < USB_EP_MAX; n++)
    {
        for (uint8_t d = 0; d < 2; d++)
        {
            USBEndpoint *ep = &inst->ep[n][d];
            ep->number = n;
            ep->dir = d;
            ep->type = USB_EP_TYPE_CONTROL; /* 类 bind 时按需覆盖为 BULK/INT */
            ep->max_packet = 0;
            ep->state = USB_EP_STATE_DISABLED;
            ep->buf[0] = inst->ep_buff[n][d][0];
            ep->buf[1] = inst->ep_buff[n][d][1];
            ep->buf_size = USB_EP_BUFF_SIZE;
            ep->double_buf = (n == 0) ? 0 : 1; /* EP0 单缓冲 */
            ep->active = 0;
            ep->last_len = 0;
            ep->ctx = NULL;
            ep->on_complete = NULL;
        }
    }

    USB_EPPoolInit(&inst->ep_pool);
}

static void USB_ConfigureFifos(PCD_HandleTypeDef *hpcd)
{
    (void)HAL_PCDEx_SetRxFiFo(hpcd, USB_RX_FIFO_SIZE);
    (void)HAL_PCDEx_SetTxFiFo(hpcd, 0, USB_TX0_FIFO_SIZE);
    (void)HAL_PCDEx_SetTxFiFo(hpcd, 1, USB_TX1_FIFO_SIZE);
    (void)HAL_PCDEx_SetTxFiFo(hpcd, 2, USB_TX2_FIFO_SIZE);
    (void)HAL_PCDEx_SetTxFiFo(hpcd, 3, USB_TX3_FIFO_SIZE); /* 多类备用 */
}

/*------------- 对外 API --------------*/

int8_t USBRegister(USBInstance *instance)
{
    if (instance == NULL || instance->inited)
    {
        return -1;
    }

    for (uint32_t i = 0; i < USB_INSTANCE_NUM; i++)
    {
        if (s_usb_instances[i] == NULL)
        {
            s_usb_instances[i] = instance;
            instance->inited = 1;
            return 0;
        }
    }
    return -1; /* 超过 USB_INSTANCE_NUM */
}

int8_t USBConfig(USBInstance *instance, const USB_Config_s *config)
{
    if (instance == NULL || config == NULL || config->usb_e >= USB_NUM_MAX)
    {
        return -1;
    }

    instance->usb_e = config->usb_e;
    instance->handle = usb_map[config->usb_e].handle;
    instance->vid = config->vid;
    instance->pid = config->pid;
    instance->bcd = config->bcd;

    instance->cdc.line_coding_cb = config->line_coding_cb;
    instance->cdc.ctrl_line_cb = config->ctrl_line_cb;
    instance->cdc.inst = instance;

    USB_EPDefaults(instance);

    /* 清空所有配置的类注册（USBConfig 可重复调用） */
    for (uint8_t i = 0; i < USB_CONFIG_MAX; i++)
    {
        instance->configs[i].class_count = 0;
    }
    instance->class_binded = 0;

    /* 注册默认 CDC 类到配置 1（可再 USBAddClass 追加 HID/DFU 等到任意配置） */
    return USBAddClass(instance, 0, USB_CDCVTable(), &instance->cdc);
}

int8_t USBAddClass(USBInstance *instance, uint8_t cfg_index, const USBClassVTable_t *vtable,
                   void *ctx)
{
    if (instance == NULL || vtable == NULL || ctx == NULL || cfg_index >= USB_CONFIG_MAX)
    {
        return -1;
    }

    USBConfig_t *cfg = &instance->configs[cfg_index];
    if (cfg->class_count >= USB_CLASS_MAX)
    {
        return -1; /* 类槽满 */
    }

    USBClassSlot_t *slot = &cfg->classes[cfg->class_count];
    slot->vtable = vtable;
    slot->ctx = ctx;
    slot->itf_start = 0;
    slot->itf_count = 0;
    cfg->class_count++;
    return 0;
}

int8_t USBStart(USBInstance *instance)
{
    if (instance == NULL || instance->handle == NULL)
    {
        return -1;
    }

    USB_ConfigureFifos(instance->handle);
    (void)HAL_PCD_Start(instance->handle);
    (void)HAL_PCD_DevConnect(instance->handle);
    return 0;
}

int8_t USBStop(USBInstance *instance)
{
    if (instance == NULL || instance->handle == NULL)
    {
        return -1;
    }

    (void)HAL_PCD_DevDisconnect(instance->handle);
    (void)HAL_PCD_Stop(instance->handle);
    return 0;
}

int32_t USBTransmit(USBInstance *instance, const uint8_t *data, uint16_t len)
{
    return USBTransmitEx(instance, data, len, NULL, NULL);
}

int32_t USBTransmitEx(USBInstance *instance, const uint8_t *data, uint16_t len,
                      void (*on_done)(void *ctx, uint16_t len), void *done_ctx)
{
    if (instance == NULL)
    {
        return -1;
    }
    return USB_CDCTransmitEx(instance, data, len, on_done, done_ctx);
}

int32_t USBReceive(USBInstance *instance, uint8_t *data, uint16_t len)
{
    if (instance == NULL)
    {
        return -1;
    }
    return USB_CDCReceive(instance, data, len);
}

uint8_t USBIsConnected(USBInstance *instance)
{
    if (instance == NULL || !instance->inited)
    {
        return 0;
    }
    return (uint8_t)(instance->configured && (instance->config_value != 0));
}

/*------------- HAL 弱回调桥接 --------------*/

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    USBInstance *inst = USB_FindByHandle(hpcd);
    if (inst == NULL)
    {
        return;
    }

    /* HAL 已完成 SETUP 包接收，EP0 状态复位为 IDLE 后交 core 处理 */
    inst->ep[0][USB_EP_DIR_IN].state = USB_EP_STATE_IDLE;
    inst->ep[0][USB_EP_DIR_OUT].state = USB_EP_STATE_IDLE;

    USB_CoreOnSetupPacket(inst, (const USB_SetupPacket_t *)hpcd->Setup);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    USBInstance *inst = USB_FindByHandle(hpcd);
    if (inst == NULL)
    {
        return;
    }

    /* USB 复位清掉 FIFO 配置，重配 */
    USB_ConfigureFifos(hpcd);

    /* 解绑类：close 类端点 + 释放端点池 */
    USB_CoreUnbindClasses(inst);

    /* 重建 EP0 与状态机（ControlInit 内重新绑定类：分配接口号/端点 + open + 填描述符） */
    USB_CoreDeinit(inst);
    USB_CDCReset(inst);
    USB_CoreControlInit(inst);
    inst->configured = 0;
    inst->config_value = 0;
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBInstance *inst = USB_FindByHandle(hpcd);
    if (inst == NULL || epnum >= USB_EP_MAX)
    {
        return;
    }

    USB_EPOnTransferComplete(&inst->ep[epnum][USB_EP_DIR_IN],
                             hpcd->IN_ep[epnum].xfer_count);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBInstance *inst = USB_FindByHandle(hpcd);
    if (inst == NULL || epnum >= USB_EP_MAX)
    {
        return;
    }

    USB_EPOnTransferComplete(&inst->ep[epnum][USB_EP_DIR_OUT],
                             hpcd->OUT_ep[epnum].xfer_count);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    USBInstance *inst = USB_FindByHandle(hpcd);
    if (inst != NULL)
    {
        inst->configured = 0;
    }
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd; /* 首版无需处理 */
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    (void)hpcd;
}

#endif /* BSP_USB_USED */

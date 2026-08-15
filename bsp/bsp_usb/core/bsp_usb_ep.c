/**
 * @file bsp_usb_ep.c
 * @brief 端点对象实现（见 bsp_usb_ep.h）
 *
 * 包一层 HAL_PCD_EP_*，并把双缓冲切换时机管理起来：
 *   - IN：Transfer 提交时切换（发完后 Pending 即刚发数据）
 *   - OUT：完成回调里切换（Pending 即刚收数据）
 * F4 / H7 的 HAL PCD API 签名一致，无平台分支。
 */

#include "bsp_usb_ep.h"

/*------------- 端点池 --------------*/

void USB_EPPoolInit(USB_EPPool_t *pool)
{
    pool->used = 0;
}

int8_t USB_EPPoolGet(USB_EPPool_t *pool, USBEndpoint ep_tab[][2], uint8_t num,
                     USBEPDir_e dir, USBEndpoint **out)
{
    if (out == NULL || pool == NULL)
    {
        return -1;
    }

    if (num == USB_EP_NUM_AUTO)
    {
        /* 从 EP1 起找空闲端点号 */
        for (uint8_t n = 1; n < USB_EP_MAX; n++)
        {
            if (!(pool->used & (1u << (n * 2 + dir))))
            {
                num = n;
                break;
            }
        }
        if (num == USB_EP_NUM_AUTO)
        {
            return -1; /* 池满 */
        }
    }

    if (num == 0 || num >= USB_EP_MAX)
    {
        return -1; /* EP0 保留 / 超出 */
    }

    uint32_t bit = (1u << (num * 2 + dir));
    if (pool->used & bit)
    {
        return -1; /* 已占用 */
    }

    pool->used |= bit;
    *out = &ep_tab[num][dir];
    return 0;
}

void USB_EPPoolRelease(USB_EPPool_t *pool, USBEndpoint *ep)
{
    if (pool == NULL || ep == NULL)
    {
        return;
    }
    pool->used &= ~(1u << (ep->number * 2 + ep->dir));
}

void USB_EPInit(USBEndpoint *ep, uint8_t number, USBEPDir_e dir, uint8_t *buf0, uint8_t *buf1,
                uint16_t buf_size, uint8_t double_buf)
{
    ep->number = number;
    ep->dir = dir;
    ep->type = USB_EP_TYPE_CONTROL;
    ep->max_packet = 0;
    ep->state = USB_EP_STATE_DISABLED;
    ep->buf[0] = buf0;
    ep->buf[1] = double_buf ? buf1 : NULL;
    ep->buf_size = buf_size;
    ep->double_buf = double_buf;
    ep->active = 0;
    ep->last_len = 0;
    ep->ctx = NULL;
    ep->on_complete = NULL;
}

int8_t USB_EPConfigure(PCD_HandleTypeDef *hpcd, USBEndpoint *ep, USBEPType_e type,
                       uint16_t max_packet)
{
    uint8_t addr = USB_EP_ADDR(ep->number, ep->dir);

    ep->type = type;
    if (max_packet > ep->buf_size)
    {
        max_packet = ep->buf_size;
    }
    if (max_packet < 8)
    {
        max_packet = 8;
    }
    ep->max_packet = max_packet;

    if (HAL_PCD_EP_Open(hpcd, addr, max_packet, type) != HAL_OK)
    {
        ep->state = USB_EP_STATE_ERROR;
        return -1;
    }

    ep->state = USB_EP_STATE_IDLE;
    return 0;
}

void USB_EPClose(PCD_HandleTypeDef *hpcd, USBEndpoint *ep)
{
    HAL_PCD_EP_Close(hpcd, USB_EP_ADDR(ep->number, ep->dir));
    ep->state = USB_EP_STATE_DISABLED;
}

int8_t USB_EPTransfer(PCD_HandleTypeDef *hpcd, USBEndpoint *ep, uint16_t size)
{
    if (ep->state == USB_EP_STATE_BUSY)
    {
        return -1;
    }

    if (size > ep->buf_size)
    {
        size = ep->buf_size;
    }

    uint8_t addr = USB_EP_ADDR(ep->number, ep->dir);
    uint8_t *xfer_buf = ep->buf[ep->active];

    /* IN 双缓冲：提交前切换，使 Pending 在完成后指向刚发出的块 */
    if (ep->double_buf && ep->dir == USB_EP_DIR_IN && size > 0)
    {
        ep->active ^= 1;
    }

    ep->last_len = size;

    HAL_StatusTypeDef st;
    if (ep->dir == USB_EP_DIR_IN)
    {
        st = HAL_PCD_EP_Transmit(hpcd, addr, xfer_buf, size);
    }
    else
    {
        st = HAL_PCD_EP_Receive(hpcd, addr, xfer_buf, size);
    }

    if (st != HAL_OK)
    {
        ep->state = USB_EP_STATE_ERROR;
        return -1;
    }

    ep->state = USB_EP_STATE_BUSY;
    return 0;
}

int8_t USB_EPTransferZLP(PCD_HandleTypeDef *hpcd, USBEndpoint *ep)
{
    return USB_EPTransfer(hpcd, ep, 0);
}

int8_t USB_EPStall(PCD_HandleTypeDef *hpcd, USBEndpoint *ep)
{
    /* 允许对 OUT 端点从 BUSY 直接 STALL（主机可能连续发 OUT） */
    if (ep->state != USB_EP_STATE_IDLE &&
        !(ep->state == USB_EP_STATE_BUSY && ep->dir == USB_EP_DIR_OUT))
    {
        return -1;
    }

    if (HAL_PCD_EP_SetStall(hpcd, USB_EP_ADDR(ep->number, ep->dir)) == HAL_OK)
    {
        ep->state = USB_EP_STATE_STALLED;
        return 0;
    }

    ep->state = USB_EP_STATE_ERROR;
    return -1;
}

int8_t USB_EPUnstall(PCD_HandleTypeDef *hpcd, USBEndpoint *ep)
{
    if (ep->state != USB_EP_STATE_STALLED)
    {
        return -1;
    }

    /* EP0 的 STALL 由硬件在下一 SETUP 自动清除，软件只需复位状态 */
    if (ep->number == 0)
    {
        ep->state = USB_EP_STATE_IDLE;
        return 0;
    }

    if (HAL_PCD_EP_ClrStall(hpcd, USB_EP_ADDR(ep->number, ep->dir)) == HAL_OK)
    {
        ep->state = USB_EP_STATE_IDLE;
        return 0;
    }

    ep->state = USB_EP_STATE_ERROR;
    return -1;
}

void USB_EPOnTransferComplete(USBEndpoint *ep, uint32_t actual_len)
{
    if (ep->state != USB_EP_STATE_BUSY)
    {
        return;
    }

    ep->state = USB_EP_STATE_IDLE;

    /* OUT 双缓冲在完成时切换，Pending 变为刚接收的块 */
    if (ep->double_buf && ep->dir == USB_EP_DIR_OUT)
    {
        ep->active ^= 1;
    }

    if (ep->on_complete != NULL)
    {
        ep->on_complete(ep, actual_len);
    }
}

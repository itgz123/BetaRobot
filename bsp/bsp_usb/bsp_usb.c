/**
 * @file bsp_usb.c
 * @brief USB VCP BSP 驱动实现
 *
 * @note 实例管理模式（Register → Config → Transmit）：
 *       Register 将实例注册到管理数组
 *       Config   设置回调（USB 硬件初始化由 freertos.c 默认任务负责）
 *       Transmit 数据写入 ring → bsp_usb_process_tx() 发一包
 *       USB TX 完成中断 → CDC 回调 → bsp_usb_process_tx() 发下一包
 *       RX 经 bsp_usb_rx_handler() 转发给用户回调
 *       所有缓冲区均嵌入 BSP_USB_Instance 结构体，无需外部数组
 */

#include "bsp_usb.h"
#include "bsp_uart_log.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include <string.h>

/*============================================
 *      板级差异：FS(STM32F4) vs HS(STM32H7)
 *============================================
 * USB 函数名因 MCU 系列而异：
 *   STM32H7 (DM_MC02) — USB_OTG_HS + 内嵌 FS PHY → _HS 后缀
 *   STM32F4 (DJI_C/A) — USB_OTG_FS               → _FS 后缀
 * 但实际传输都运行在 FS 模式，最大包长均为 64。 */
#if defined(STM32H723xx)
#define BSP_USB_CDC_TRANSMIT CDC_Transmit_HS
#else
#define BSP_USB_CDC_TRANSMIT CDC_Transmit_FS
#endif

/* 最大实例数（USB 硬件仅一个） */
#define BSP_USB_INSTANCE_MAX 1

/*============================================
 *              内部数据
 *============================================*/
/** 当前活动实例（供 CDC 回调使用） */
static BSP_USB_Instance *s_active_inst = NULL;

/** USB 实例管理数组 */
static BSP_USB_Instance *s_usb_instances[BSP_USB_INSTANCE_MAX] = {NULL};
static uint8_t s_usb_idx = 0;

/*============================================
 *        内部函数（被 usbd_cdc_if.c 调用）
 *============================================*/

void bsp_usb_process_tx(void)
{
    BSP_USB_Instance *inst = s_active_inst;
    if (inst == NULL)
        return;

    if (inst->tx_head == inst->tx_tail)
        return; /* 无数据可发 */

    /* 计算到环尾的连续数据量 */
    uint16_t avail = (inst->tx_head > inst->tx_tail) ? (inst->tx_head - inst->tx_tail) : (sizeof(inst->tx_ring) - inst->tx_tail);

    uint16_t len = (avail > BSP_USB_TX_BUF_SIZE) ? BSP_USB_TX_BUF_SIZE : avail;

    memcpy(inst->tx_buf, &inst->tx_ring[inst->tx_tail], len);

    if (BSP_USB_CDC_TRANSMIT(inst->tx_buf, len) == USBD_OK)
    {
        inst->tx_tail = (inst->tx_tail + len) % (uint16_t)sizeof(inst->tx_ring);
    }
    /* USBD_BUSY → 等待 TX 完成中断重试 */
}

void bsp_usb_rx_handler(uint8_t *buf, uint32_t len)
{
    BSP_USB_Instance *inst = s_active_inst;
    if ((inst != NULL) && (inst->rx_cb != NULL))
    {
        inst->rx_cb(inst, buf, (uint16_t)len);
    }
}

void bsp_usb_tx_complete_handler(void)
{
    BSP_USB_Instance *inst = s_active_inst;

    /* 尝试发送 ring 中的下一包 */
    bsp_usb_process_tx();

    if ((inst != NULL) && (inst->tx_cb != NULL))
    {
        inst->tx_cb(inst);
    }
}

/*============================================
 *              外部接口
 *============================================*/

int8_t BSP_USB_Register(BSP_USB_Instance *inst)
{
    BSP_RETURN_IF_TRUE_LOG(inst == NULL, -1, LOGERROR("[bsp_usb] Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_usb_idx >= BSP_USB_INSTANCE_MAX, -1, LOGERROR("[bsp_usb] Exceeded max instance count!"));

    /* 防重复注册 */
    for (uint8_t i = 0; i < s_usb_idx; i++)
    {
        if (s_usb_instances[i] == inst)
        {
            LOGERROR("[bsp_usb] Instance already registered!");
            return -1;
        }
    }

    s_usb_instances[s_usb_idx++] = inst;
    s_active_inst = inst;
    LOGINFO("[bsp_usb] USB instance registered");
    return 0;
}

int8_t BSP_USB_Config(BSP_USB_Instance *inst, const BSP_USB_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(inst == NULL, -1, LOGERROR("[bsp_usb] Config: instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, LOGERROR("[bsp_usb] Config is NULL!"));

    /* 验证实例已注册 */
    uint8_t found = 0;
    for (uint8_t i = 0; i < s_usb_idx; i++)
    {
        if (s_usb_instances[i] == inst)
        {
            found = 1;
            break;
        }
    }
    BSP_RETURN_IF_TRUE_LOG(!found, -1, LOGERROR("[bsp_usb] Instance not registered!"));

    /* 设置回调 */
    inst->rx_cb = config->rx_cb;
    inst->tx_cb = config->tx_cb;
    inst->parent = NULL; /* DRV 层后续可设置 */

    LOGINFO("[bsp_usb] USB VCP configured");
    return 0;
}

void BSP_USB_Transmit(BSP_USB_Instance *inst, const uint8_t *data, uint16_t len)
{
    if ((inst == NULL) || (len == 0))
        return;

    /* 确保 s_active_inst 指向此实例，供 bsp_usb_process_tx 使用 */
    s_active_inst = inst;

    /* 逐字节写入环形缓冲区 */
    for (uint16_t i = 0; i < len; i++)
    {
        uint16_t next = (inst->tx_head + 1) % (uint16_t)sizeof(inst->tx_ring);
        if (next == inst->tx_tail)
        {
            LOGWARNING("[bsp_usb] TX ring full, %d bytes dropped", len - i);
            break;
        }
        inst->tx_ring[inst->tx_head] = data[i];
        inst->tx_head = next;
    }

    /* 尝试立即发送 */
    bsp_usb_process_tx();
}

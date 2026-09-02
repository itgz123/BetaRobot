/**
 * @file bsp_usb.c
 * @brief USB VCP 驱动实现
 *
 * @note 实例管理模式（Register → Config → Transmit）：
 *       Register 将实例注册到管理数组（TX 环形缓冲为实例内嵌数组 tx_ring）
 *       Config   设置回调（USB 硬件初始化由 MX_USB_DEVICE_Init 负责，见下方 @warning）
 *       Transmit 数据写入 ring → bsp_usb_process_tx() 发一包
 *       USB TX 完成中断 → CDC 回调 → bsp_usb_process_tx() 发下一包
 *       RX 经 bsp_usb_rx_handler() 填入 instance->rx_buff/rx_len 并调用用户回调
 *
 * @warning 【重要】USB 硬件初始化时机约定：
 *       MX_USB_DEVICE_Init() 必须在 main() 中 osKernelStart() 之前调用
 *       （USER CODE BEGIN 2 段，与 function_in_main_c() 同处），
 *       不要在 freertos.c 的 StartDefaultTask（osPriorityIdle）里调用！
 *       原因：
 *         1. 本驱动的 USBTransmit → CDC_Transmit_HS 会解引用 hUsbDeviceHS，
 *            未初始化或未枚举时可能崩溃/无效，提前初始化保证用户任务
 *            一启动即可安全使用；
 *         2. idle 任务优先级最低，可能被忙等任务饿死，USB 初始化若依赖
 *            idle 调度则存在不执行的隐患；
 *         3. MX_USB_DEVICE_Init 内部纯 HAL / USB stack 调用，不依赖
 *            FreeRTOS 也无阻塞，调度器启动前调用完全安全，且枚举更早完成。
 *       ⚠️ 若 CubeMX 重新生成又在 StartDefaultTask 中加回该调用，必须删除，
 *          或在 usb_device.c 的 USER CODE 段用 static 标志做幂等保护。
 *          （重复 USBD_Init 会重新注册类并重启 PCD，导致枚举异常。）
 */

#include "bsp_usb.h"
#include "bsp_log.h"
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
#define USB_CDC_TRANSMIT CDC_Transmit_HS
#define USB_TEST_DEVICE hUsbDeviceHS
#else
#define USB_CDC_TRANSMIT CDC_Transmit_FS
#define USB_TEST_DEVICE hUsbDeviceFS
#endif
extern USBD_HandleTypeDef USB_TEST_DEVICE;

/*============================================
 *              内部数据
 *============================================*/
/** 当前活动实例（供 CDC 回调使用） */
static USBInstance *s_active_inst = NULL;
LOG_INSTANCE_DEF(g_usb_log, "bsp_usb", 0); /* USB 日志实例 */

/** USB 实例管理数组 */
static USBInstance *s_usb_instances[USB_INSTANCE_NUM] = {NULL};
static uint8_t s_usb_idx = 0;

/*============================================
 *        内部函数（被 usbd_cdc_if.c 调用）
 *============================================*/

void bsp_usb_process_tx(void)
{
    USBInstance *inst = s_active_inst;
    if (inst == NULL)
        return;

    if (inst->tx_head == inst->tx_tail)
        return; /* 无数据可发 */

    /* 计算到环尾的连续数据量 */
    uint16_t avail = (inst->tx_head > inst->tx_tail) ? (inst->tx_head - inst->tx_tail) : (APP_TX_DATA_SIZE - inst->tx_tail);

    uint16_t len = (avail > USB_TX_BUF_SIZE) ? USB_TX_BUF_SIZE : avail;

    memcpy(inst->tx_buf, &inst->tx_ring[inst->tx_tail], len);

    if (USB_CDC_TRANSMIT(inst->tx_buf, len) == USBD_OK)
    {
        inst->tx_tail = (inst->tx_tail + len) % APP_TX_DATA_SIZE;
    }
    /* USBD_BUSY → 等待 TX 完成中断重试 */
}

void bsp_usb_rx_handler(uint8_t *buf, uint32_t len)
{
    USBInstance *inst = s_active_inst;
    if (inst != NULL)
    {
        /* 填入实例成员，供用户回调读取 */
        inst->rx_buff = buf;
        inst->rx_len = (uint16_t)len;

        if (inst->rx_callback != NULL)
        {
            inst->rx_callback(inst);
        }
    }
}

void bsp_usb_tx_complete_handler(void)
{
    USBInstance *inst = s_active_inst;

    /* 尝试发送 ring 中的下一包 */
    bsp_usb_process_tx();

    if ((inst != NULL) && (inst->tx_callback != NULL))
    {
        inst->tx_callback(inst);
    }
}

/*============================================
 *              外部接口
 *============================================*/

int8_t USBRegister(USBInstance *instance)
{

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_usb_idx >= USB_INSTANCE_NUM, -1, BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Exceeded max instance count!"));

    /* 防重复注册 */
    for (uint8_t i = 0; i < s_usb_idx; i++)
    {
        if (s_usb_instances[i] == instance)
        {
            BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Instance already registered!");
            return -1;
        }
    }

    s_usb_instances[s_usb_idx++] = instance;
    s_active_inst = instance;
    BSPLOG(&g_usb_log, LOG_LEVEL_INFO, "USB instance registered");
    return 0;
}

int8_t USBConfig(USBInstance *instance, const USB_Config_s *config)
{
    BSP_RETURN_IF_TRUE_LOG(instance == NULL, -1, BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Config: instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, -1, BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Config is NULL!"));

    /* 验证实例已注册 */
    uint8_t found = 0;
    for (uint8_t i = 0; i < s_usb_idx; i++)
    {
        if (s_usb_instances[i] == instance)
        {
            found = 1;
            break;
        }
    }
    BSP_RETURN_IF_TRUE_LOG(!found, -1, BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Instance not registered!"));

    /* 设置回调与反向指针 */
    instance->rx_callback = config->rx_callback;
    instance->tx_callback = config->tx_callback;
    instance->parent = config->parent; /* 反向指针：DRV 层传入 media 实例（可为 NULL）*/

    BSPLOG(&g_usb_log, LOG_LEVEL_INFO, "USB VCP configured");
    return 0;
}

void USBTransmit(USBInstance *instance, const uint8_t *data, uint16_t len)
{
    if ((instance == NULL) || (len == 0))
        return;

    if (USB_TEST_DEVICE.dev_state != USBD_STATE_CONFIGURED)
    {
        return;
    }

    /* 确保 s_active_inst 指向此实例，供 bsp_usb_process_tx 使用 */
    s_active_inst = instance;

    /* 逐字节写入环形缓冲区 */
    for (uint16_t i = 0; i < len; i++)
    {
        uint16_t next = (instance->tx_head + 1) % APP_TX_DATA_SIZE;
        if (next == instance->tx_tail)
        {
            BSPLOG(&g_usb_log, LOG_LEVEL_WARNING, "TX ring full, %d bytes dropped", len - i);
            break;
        }
        instance->tx_ring[instance->tx_head] = data[i];
        instance->tx_head = next;
    }

    /* 尝试立即发送 */
    bsp_usb_process_tx();
}

/**
 * @file bsp_usb.c
 * @brief USB VCP 驱动实现
 *
 * 实例管理模式（Register → Config → Transmit）：
 *   Register 将实例注册到管理数组（TX 环形缓冲为实例内嵌数组 tx_ring）
 *   Config   设置回调（USB 硬件初始化由 MX_USB_DEVICE_Init 负责，见下方 @warning）
 *   Transmit 数据写入 ring → bsp_usb_process_tx() 发一包
 *   USB TX 完成中断 → CDC 回调 → bsp_usb_process_tx() 发下一包
 *   RX 经 bsp_usb_rx_handler() 填入 instance->rx_buff/rx_len 并调用用户回调
 *
 * 错误上报与自恢复（与 bsp_usart / bsp_spi / bsp_i2c 同构）：
 *   ① 失败返回统一 BSP_Status_e（忙 / 未枚举 / 参数错三者分开，不再压在同一个 -1 里）
 *   ② err_callback 通报"已完成纠正动作"的停滞事件（见 USB_ErrReason_e）
 *   ③ 三条自恢复入口（USBRecoverTxIfStuck / USBRecoverRxIfStalled / USBReenumerate），
 *      只把"何时看一眼"交给上层，判据与动作都在这层
 *   ④ 计数与状态快照存 static 结构体供调试器 Watch（s_usb_status）
 *
 * @warning USB 硬件初始化时机约定：
 *       **真正有效的约束是"MX_USB_DEVICE_Init() 必须早于第一次 USBTransmit"**——
 *       本驱动的 USBTransmit → CDC_Transmit_HS 会解引用 hUsbDeviceHS 的 pClassData，
 *       未初始化时那里是 NULL，发送会被本层判为"未枚举"（BSP_HW_ERR）而不会崩，
 *       但在枚举完成前发送同样发不出去。
 *       现状：四块板都在 freertos.c 的 StartDefaultTask（osPriorityIdle）里调用，
 *       与旧版注释"必须在 osKernelStart() 之前"不符；实测可用（每个周期任务都会阻塞
 *       让出，idle 能跑到），故本轮只把注释改成与现状一致，不动 cubemx——搬初始化要
 *       实机验证。⚠️ 若 CubeMX 重新生成在别处又加一次 USBD_Init，必须删掉（重复
 *       USBD_Init 会重新注册类并重启 PCD，导致枚举异常）。
 */

#include "bsp_usb.h"

#if defined(HAL_PCD_MODULE_ENABLED)

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
 * 但实际传输都运行在 FS 模式，最大包长均为 64。
 * UserRxBufferFS/HS 是 CubeMX 生成文件里的非 static 全局（未写进头文件，
 * 故与设备句柄一样在这里自行 extern）：接收重新武装要挂回同一块缓冲。 */
#if defined(STM32H723xx)
#define USB_CDC_TRANSMIT CDC_Transmit_HS
#define USB_TEST_DEVICE hUsbDeviceHS
#define USB_USER_RX_BUFFER UserRxBufferHS
#else
#define USB_CDC_TRANSMIT CDC_Transmit_FS
#define USB_TEST_DEVICE hUsbDeviceFS
#define USB_USER_RX_BUFFER UserRxBufferFS
#endif
#ifndef BSP_USB_LOG_LIMIT
#define BSP_USB_LOG_LIMIT 10
#endif // !BSP_USB_LOG_LIMIT
extern USBD_HandleTypeDef USB_TEST_DEVICE;
extern uint8_t USB_USER_RX_BUFFER[APP_RX_DATA_SIZE];

/* 重新枚举时"断开 → 再上拉"之间的等待：给主机足够时间察觉拔出（USB 规范要求
 * 至少 2.5µs 的断开，但主机栈的拔出检测普遍按 10~100ms 量级节拍，
 * 20ms 是"能稳定触发重枚举"与"别把调用方阻塞太久"之间的折中）。 */
#ifndef USB_REENUMERATE_GAP_MS
#define USB_REENUMERATE_GAP_MS 20u
#endif

/*------------- USB 状态/错误统计（调试用，调试器直接 Watch s_usb_status） --------------*/

/**
 * @brief USB 状态与错误统计（每实例一份）
 * @note 纯调试辅助：只增不清，需要清零可在调试器里直接写 0。
 *       信息来源：收发接口的失败返回、CDC 收发回调、三条恢复入口，
 *       外加便于判断"当前卡在哪一步"的实时快照。
 */
typedef struct
{
    /* 收发计数 */
    uint32_t tx_ok;             /* USBTransmit 整帧成功入队次数 */
    uint32_t tx_pkt_ok;         /* 单包成功提交给 CDC 次数（bsp_usb_process_tx 中 USBD_OK） */
    uint32_t tx_fail;           /* USBTransmit 返回非 BSP_OK 的总次数 */
    uint32_t tx_ring_full;      /* 其中 BSP_BUSY（环放不下整帧，零字节写入） */
    uint32_t tx_param_err;      /* 其中 BSP_PARAM_ERR */
    uint32_t tx_not_configured; /* 其中 BSP_HW_ERR（未枚举：主机串口没开/已拔出） */
    uint32_t tx_busy;           /* CDC_Transmit 返回 USBD_BUSY 次数（上一包还在途，正常现象）
                                 * @note 与另两处同名计数**不是一个东西**：bsp_can 的 tx_busy 是
                                 *       "邮箱/FIFO 满"；comm media 的 tx_busy 是"上层 ring 满
                                 *       （BSP_BUSY）"。这里是端点级在途，是最内层、最不该涨的那个。 */
    uint32_t tx_hw_fail;        /* CDC_Transmit 返回其它非 OK 值次数 */
    uint32_t rx_ok;             /* 收帧次数 */
    /* 自恢复计数 */
    uint32_t tx_stuck;          /* 判定 TX 卡死（已执行收尾动作）次数 */
    uint32_t tx_recover_ok;     /* 卡死收尾后成功恢复出队次数 */
    uint32_t tx_recover_fail;   /* 卡死收尾后仍出不了队次数 */
    uint32_t rx_stalled;        /* 判定 RX 停摆（已尝试重新武装）次数 */
    uint32_t rx_recover_ok;     /* RX 重新武装被中间件接受次数（是否真恢复看后续 rx_ok） */
    uint32_t rx_rearm_fail;     /* RX 重新武装被拒次数（>0 表示 RX 已停摆且起不来） */
    uint32_t dev_state_drop;    /* 枚举状态下降沿次数（主机串口关闭 / 拔出） */
    /* 实时快照 */
    uint8_t dev_state;     /* USBD_HandleTypeDef.dev_state（最近一次采样） */
    uint8_t dev_speed;     /* USBD_HandleTypeDef.dev_speed（最近一次采样） */
    uint8_t tx_state;      /* hcdc->TxState（非 0 = 有包在途；0xFF = 句柄未就绪） */
    uint8_t tx_claim;      /* 正在提交一包的上下文标志 */
    uint16_t tx_head;      /* ring 生产者位置 */
    uint16_t tx_tail;      /* ring 消费者位置 */
    uint16_t ring_used;    /* ring 已占用字节数（= 待发量；长期不减说明发送卡住） */
    uint64_t rx_last_us;   /* 最近一次收帧时刻（DWT 微秒） */
    uint64_t last_err_us;  /* 最近一次错误/停滞时刻（DWT 微秒） */
} USB_Status_s;

/* 调试时 Watch 查看；volatile 保证调试器读到实时值、ISR 内写不被优化 */
volatile USB_Status_s s_usb_status[USB_INSTANCE_NUM];

/*============================================
 *              内部数据
 *============================================*/
/** 当前活动实例（供 CDC 回调使用；任务与 ISR 都会写，故 volatile） */
static volatile USBInstance *s_active_inst = NULL;
LOG_INSTANCE_DEF(g_usb_log, "bsp_usb", BSP_USB_LOG_LIMIT); /* USB 日志实例 */

/** USB 实例管理数组 */
static USBInstance *s_usb_instances[USB_INSTANCE_NUM] = {NULL};
static uint8_t s_usb_idx = 0;

/*============================================
 *              私有辅助
 *============================================*/

/**
 * @brief 实例 → 管理数组下标
 * @retval USB_INSTANCE_NUM 未找到
 */
static uint8_t USB_InstanceToIndex(const USBInstance *instance)
{
    uint8_t i;

    if (instance == NULL)
        return USB_INSTANCE_NUM;

    for (i = 0; i < s_usb_idx; i++)
    {
        if (s_usb_instances[i] == instance)
            return i;
    }
    return USB_INSTANCE_NUM;
}

/**
 * @brief 取 CDC 类句柄（TxState 就在这里）
 * @retval NULL 设备未初始化 / 未枚举（pClassData 为空）
 * @note 未调用 MX_USB_DEVICE_Init 时 hUsbDeviceHS 是全零结构体，pClassData 为 NULL，
 *       返回 NULL 即可，不会崩。
 */
static USBD_CDC_HandleTypeDef *USB_GetCdc(void)
{
    return (USBD_CDC_HandleTypeDef *)USB_TEST_DEVICE.pClassData;
}

/**
 * @brief 取 PCD 句柄（端点 abort / 重新枚举要用）
 * @retval NULL 设备未初始化
 */
static PCD_HandleTypeDef *USB_GetPcd(void)
{
    return (PCD_HandleTypeDef *)USB_TEST_DEVICE.pData;
}

/** ring 中待发字节数（生产者位置 - 消费者位置） */
static uint16_t USB_RingUsed(const USBInstance *inst)
{
    return (uint16_t)((inst->tx_head + APP_TX_DATA_SIZE - inst->tx_tail) % APP_TX_DATA_SIZE);
}

/**
 * @brief 忙等指定毫秒（DWT 计时，不依赖 RTOS）
 * @note 只给 USBReenumerate 的"断开→上拉"间隔用。调用方已在文档里被告知会阻塞。
 */
static void USB_BusyWaitMs(uint32_t ms)
{
    uint64_t start = DWT_GetTimeUs();

    while ((DWT_GetTimeUs() - start) < ((uint64_t)ms * 1000u))
    {
    }
}

/**
 * @brief 刷新状态快照（纯观测，任何上下文都可调）
 */
static void USB_Snapshot(USBInstance *inst, uint8_t idx)
{
    USBD_CDC_HandleTypeDef *hcdc;

    if (inst == NULL || idx >= USB_INSTANCE_NUM)
        return;

    hcdc = USB_GetCdc();

    s_usb_status[idx].dev_state = inst->dev_state;
    s_usb_status[idx].dev_speed = (uint8_t)USB_TEST_DEVICE.dev_speed;
    s_usb_status[idx].tx_state = (hcdc != NULL) ? (uint8_t)hcdc->TxState : 0xFFu;
    s_usb_status[idx].tx_claim = inst->tx_claim;
    s_usb_status[idx].tx_head = inst->tx_head;
    s_usb_status[idx].tx_tail = inst->tx_tail;
    s_usb_status[idx].ring_used = USB_RingUsed(inst);
    s_usb_status[idx].rx_last_us = inst->rx_last_us;
}

/**
 * @brief 采样枚举状态，检测"已枚举 → 未枚举"的下降沿并通报
 * @retval 当前 USB_TEST_DEVICE.dev_state
 * @warning **只在任务上下文调用**：下降沿要回调 err_callback，而 USB 中断里不产生
 *          err_callback（契约见 bsp_usb.h）。收帧入口（ISR）只刷新基准，不做边沿检测。
 * @note 下降沿的含义是"主机侧串口关了/线拔了"——软件无从恢复，只能等枚举回来。
 */
static uint8_t USB_SampleDevState(USBInstance *inst, uint8_t idx)
{
    uint8_t now = (uint8_t)USB_TEST_DEVICE.dev_state;

    if ((inst->dev_state == (uint8_t)USBD_STATE_CONFIGURED) && (now != (uint8_t)USBD_STATE_CONFIGURED))
    {
        if (idx < USB_INSTANCE_NUM)
        {
            s_usb_status[idx].dev_state_drop++;
            s_usb_status[idx].last_err_us = DWT_GetTimeUs();
        }
        BSPLOG(&g_usb_log, LOG_LEVEL_WARNING, "USB de-configured (dev_state=%u)", now);

        if (inst->err_callback != NULL)
            inst->err_callback(inst, USB_ERR_NOT_CONFIGURED);
    }

    inst->dev_state = now;
    USB_Snapshot(inst, idx);
    return now;
}

/*============================================
 *        内部函数（被 usbd_cdc_if.c 调用）
 *============================================*/

/**
 * @brief 提交 ring 中的下一包给 CDC —— **调用者必须已持有 inst->tx_claim**
 *
 * @note 只被 bsp_usb_process_tx() 与 USBTransmit() 调用：前者在提交窗口内自持 claim，
 *       后者把 claim 的范围扩到"写 ring + 提交"（见 USBTransmit 的注释）。
 *       本函数不判也不改 tx_claim —— 判在哪里，就在哪里负责释放。
 * @note 提交失败只计数不打日志：这是 2ms 周期的热路径，且 ISR 上下文不能打日志。
 */
static void USB_ProcessTxLocked(USBInstance *inst)
{
    USBD_CDC_HandleTypeDef *hcdc;
    uint16_t avail;
    uint16_t len;
    uint8_t r;
    uint8_t idx;

    if (inst->tx_head == inst->tx_tail)
        return; /* 无数据可发 */

    hcdc = USB_GetCdc();
    if (hcdc == NULL)
        return; /* 设备未初始化 */

    idx = USB_InstanceToIndex(inst);

    /* 计算到环尾的连续数据量 */
    avail = (inst->tx_head > inst->tx_tail) ? (uint16_t)(inst->tx_head - inst->tx_tail)
                                           : (uint16_t)(APP_TX_DATA_SIZE - inst->tx_tail);
    len = (avail > USB_TX_BUF_SIZE) ? USB_TX_BUF_SIZE : avail;

    memcpy(inst->tx_buf, &inst->tx_ring[inst->tx_tail], len);

    r = USB_CDC_TRANSMIT(inst->tx_buf, len);
    if (r == USBD_OK)
    {
        inst->tx_tail = (uint16_t)((inst->tx_tail + len) % APP_TX_DATA_SIZE);
        inst->tx_last_ok_us = DWT_GetTimeUs(); /* 卡死判据基准：只在"真提交成功"时刷新 */
        if (idx < USB_INSTANCE_NUM)
            s_usb_status[idx].tx_pkt_ok++;
    }
    else if (r == USBD_BUSY)
    {
        /* 上一包还在途：tx_tail 不推进是对的，等 TX 完成中断再来一次 */
        if (idx < USB_INSTANCE_NUM)
            s_usb_status[idx].tx_busy++;
    }
    else
    {
        if (idx < USB_INSTANCE_NUM)
            s_usb_status[idx].tx_hw_fail++;
    }

    USB_Snapshot(inst, idx);
}

/**
 * @brief 把 ring 中的下一包提交给 CDC（任务与 ISR 都会调）
 *
 * @note **tx_claim 保护**：提交要往共享的 inst->tx_buf 里 memcpy 再交给 CDC，而本函数有
 *       两个调用上下文（USBTransmit 的任务侧、CDC_TransmitCplt 的 ISR 侧）。没有保护时
 *       会出现：任务侧 memcpy 到一半被 TxCplt 中断，ISR 侧把 tx_buf 提交出去（CDC 的 IN
 *       端点此时正指向 tx_buf），任务侧回来接着写 → **在途数据被覆写**。
 *       tx_claim 让"先到的那个把它做完"，另一侧直接返回；USBD_CDC_TransmitPacket 内部的
 *       TxState 判据保证同一时刻只有一个包在途，故被跳过的那次不会漏发
 *       （提交成功的那个包完成后必然再来一次 TxCplt）。
 * @note **写 ring 的那一侧也要持 claim**（见 USBTransmit）：生产者逐字节推进 tx_head，
 *       ISR 若在写到一半时进来，会读到一个偏小的 tx_head → 把一个**被截断的包**发出去
 *       （对端按分包序号错位丢帧）。故 claim 的范围是"写 ring + 提交"整段，不只是提交。
 */
void bsp_usb_process_tx(void)
{
    USBInstance *inst = (USBInstance *)s_active_inst;

    if (inst == NULL)
        return;

    if (inst->tx_claim)
        return; /* 另一上下文正在写入/提交，见函数头注释 */

    inst->tx_claim = 1;
    USB_ProcessTxLocked(inst);
    inst->tx_claim = 0;
}

void bsp_usb_rx_handler(uint8_t *buf, uint32_t len)
{
    USBInstance *inst = (USBInstance *)s_active_inst;
    uint8_t idx;

    if (inst != NULL)
    {
        /* 填入实例成员，供用户回调读取 */
        inst->rx_buff = buf;
        inst->rx_len = (uint16_t)len;
        inst->rx_last_us = DWT_GetTimeUs(); /* 接收停摆判据基准 */

        idx = USB_InstanceToIndex(inst);
        if (idx < USB_INSTANCE_NUM)
            s_usb_status[idx].rx_ok++;

        USB_Snapshot(inst, idx);

        if (inst->rx_callback != NULL)
        {
            inst->rx_callback(inst);
        }
    }
}

void bsp_usb_tx_complete_handler(void)
{
    USBInstance *inst = (USBInstance *)s_active_inst;

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

BSP_Status_e USBRegister(USBInstance *instance)
{
    uint8_t i;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                          BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(s_usb_idx >= USB_INSTANCE_NUM, BSP_PARAM_ERR,
                          BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Exceeded max instance count!"));

    /* 防重复注册 */
    for (i = 0; i < s_usb_idx; i++)
    {
        if (s_usb_instances[i] == instance)
        {
            BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Instance already registered!");
            return BSP_PARAM_ERR;
        }
    }

    s_usb_instances[s_usb_idx++] = instance;
    s_active_inst = instance;
    BSPLOG(&g_usb_log, LOG_LEVEL_INFO, "USB instance registered");
    return BSP_OK;
}

BSP_Status_e USBConfig(USBInstance *instance, const USB_Config_s *config)
{
    uint8_t found = 0;
    uint8_t i;

    BSP_RETURN_IF_TRUE_LOG(instance == NULL, BSP_PARAM_ERR,
                          BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Config: instance is NULL!"));
    BSP_RETURN_IF_TRUE_LOG(config == NULL, BSP_PARAM_ERR,
                          BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Config is NULL!"));

    /* 验证实例已注册 */
    for (i = 0; i < s_usb_idx; i++)
    {
        if (s_usb_instances[i] == instance)
        {
            found = 1;
            break;
        }
    }
    BSP_RETURN_IF_TRUE_LOG(!found, BSP_PARAM_ERR,
                          BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Instance not registered!"));

    /* 设置回调与反向指针 */
    instance->rx_callback = config->rx_callback;
    instance->tx_callback = config->tx_callback;
    instance->err_callback = config->err_callback;
    instance->parent = config->parent; /* 反向指针：DRV 层传入 media 实例（可为 NULL）*/

    /* 采集一次初始枚举状态：避免第一个周期就把"开机时尚未枚举"误判成一次下降沿 */
    instance->dev_state = (uint8_t)USB_TEST_DEVICE.dev_state;

    BSPLOG(&g_usb_log, LOG_LEVEL_INFO, "USB VCP configured");
    return BSP_OK;
}

BSP_Status_e USBTransmit(USBInstance *instance, const uint8_t *data, uint16_t len)
{
    uint8_t idx;
    uint16_t free_slots;
    uint16_t i;

    if ((instance == NULL) || (data == NULL) || (len == 0))
    {
        idx = USB_InstanceToIndex(instance);
        if (idx < USB_INSTANCE_NUM)
        {
            s_usb_status[idx].tx_fail++;
            s_usb_status[idx].tx_param_err++;
        }
        return BSP_PARAM_ERR;
    }

    idx = USB_InstanceToIndex(instance);

    /* 确保 s_active_inst 指向此实例，供 bsp_usb_process_tx 使用 */
    s_active_inst = instance;

    /* 未枚举：整帧拒绝。与"环满"分开报，上层据此决定是等枚举还是退避重试。
     * 下降沿在这里（任务上下文）检测并回调 err_callback。 */
    if (USB_SampleDevState(instance, idx) != (uint8_t)USBD_STATE_CONFIGURED)
    {
        if (idx < USB_INSTANCE_NUM)
        {
            s_usb_status[idx].tx_fail++;
            s_usb_status[idx].tx_not_configured++;
        }
        return BSP_HW_ERR;
    }

    /* 环满判定：**先算空间再写**，放不下就一个字节都不写（旧版写半截丢尾，
     * 对端会收到残帧）。留一格空位区分"满"与"空"。ISR 只会推进 tx_tail（释放空间），
     * 故这里算出的空间是保守下界，不会写出界。 */
    free_slots = (uint16_t)(APP_TX_DATA_SIZE - 1u - USB_RingUsed(instance));
    if (len > free_slots)
    {
        if (idx < USB_INSTANCE_NUM)
        {
            s_usb_status[idx].tx_fail++;
            s_usb_status[idx].tx_ring_full++;
        }
        BSPLOG(&g_usb_log, LOG_LEVEL_WARNING, "TX ring full, %u bytes rejected", len);
        /* 顺手推一把已有数据（不是必需，但环满常伴随"主机没在读"，早推早察觉） */
        bsp_usb_process_tx();
        return BSP_BUSY;
    }

    /* 写入 + 提交整段持 tx_claim：CDC_TransmitCplt 中断会调 bsp_usb_process_tx，而它
     * 读到的是 tx_head/tx_tail —— 若允许它在下面这个循环中途插进来，它会看到写到一半的
     * tx_head，把**被截断的包**发出去（对端按分包序号错位丢帧，末片还会凑出一帧坏数据）。
     * 持 claim 后 ISR 侧直接返回；本函数结尾自己提交，且提交成功后那包完成时仍会再来一次
     * TxCplt（若本次提交撞上 USBD_BUSY 没发出去，也正是靠那次 TxCplt 续上），故不漏发。
     * claim 只能在本函数与 bsp_usb_process_tx 里成对出现——不要在别的上下文里清它。
     * 前提：**同一实例只有一个生产任务**（本模块本就是单实例用法，见 bsp_usb.h）。
     * 若将来出现两个任务并发调本函数，"后到者先清 claim"会把前者的写入窗口重新暴露给
     * ISR —— 那时要把这里改成 try-lock（拿不到就返回 BSP_BUSY 让上层退避）。 */
    instance->tx_claim = 1;

    /* 逐字节写入环形缓冲区 */
    for (i = 0; i < len; i++)
    {
        instance->tx_ring[instance->tx_head] = data[i];
        instance->tx_head = (uint16_t)((instance->tx_head + 1u) % APP_TX_DATA_SIZE);
    }

    if (idx < USB_INSTANCE_NUM)
        s_usb_status[idx].tx_ok++;

    /* 尝试立即发送（已持 claim，故走 locked 版本；它不判也不改 claim） */
    USB_ProcessTxLocked(instance);
    instance->tx_claim = 0;
    return BSP_OK;
}

BSP_Status_e USBRecoverTxIfStuck(USBInstance *instance, uint32_t stuck_ms)
{
    USBD_CDC_HandleTypeDef *hcdc;
    PCD_HandleTypeDef *hpcd;
    USBInstance *inst = (instance != NULL) ? instance : (USBInstance *)s_active_inst;
    uint64_t now;
    uint16_t tail_before;
    uint8_t recovered;
    uint8_t ep_abort_failed;
    uint8_t idx;

    BSP_RETURN_IF_TRUE_LOG(inst == NULL, BSP_PARAM_ERR,
                          BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "RecoverTx: no active instance!"));

    if (stuck_ms == 0)
        stuck_ms = USB_TX_STUCK_DEFAULT_MS;

    idx = USB_InstanceToIndex(inst);
    hcdc = USB_GetCdc();
    if (hcdc == NULL)
        return BSP_HW_ERR; /* 设备未初始化 */

    /* 未枚举不是"卡死"而是"主机不在"：此时 abort/清 TxState 都没意义，交给枚举恢复
     * （下降沿已由 USB_SampleDevState 回调过 err_callback）。 */
    if (USB_SampleDevState(inst, idx) != (uint8_t)USBD_STATE_CONFIGURED)
        return BSP_BUSY;

    now = DWT_GetTimeUs();

    /* ring 为空 = 没有待发数据，谈不上卡死；顺手刷新基准，避免"刚入队就判超时" */
    if (USB_RingUsed(inst) == 0)
    {
        inst->tx_last_ok_us = now;
        return BSP_BUSY;
    }

    /* 首次建立基准（从未成功提交过任何一包）：先记时刻，下一轮再看是否真的出不去 */
    if (inst->tx_last_ok_us == 0)
    {
        inst->tx_last_ok_us = now;
        return BSP_BUSY;
    }

    if ((now - inst->tx_last_ok_us) < ((uint64_t)stuck_ms * 1000u))
        return BSP_BUSY; /* 未超阈值 */

    /* ---- 判定卡死：收尾 ---- */
    if (idx < USB_INSTANCE_NUM)
    {
        s_usb_status[idx].tx_stuck++;
        s_usb_status[idx].last_err_us = now;
    }
    BSPLOG(&g_usb_log, LOG_LEVEL_WARNING, "TX stuck: %u bytes pending for >%ums, forcing flush",
           USB_RingUsed(inst), stuck_ms);

    /* ① 中止 CDC IN 端点上的在途传输。
     *    注意：这一步对"主机把端点停掉了/根本没在轮询"是治本的；对"纯 TxState 卡住"
     *    则不起作用，所以要接着 ②。
     *    失败不能吞：EP_Abort 没成功 = 端点层根本没被收尾，这时 ② 的清 TxState 是唯一
     *    还起作用的动作，属于"纠正动作本身失败"，按契约报 USB_ERR_HW（见 ③）。 */
    hpcd = USB_GetPcd();
    ep_abort_failed = ((hpcd == NULL) || (HAL_PCD_EP_Abort(hpcd, CDC_IN_EP) != HAL_OK)) ? 1u : 0u;

    /* ② 强制清 TxState。CDC_Transmit 的判据就是它非 0（usbd_cdc_if.c 里自己读它），
     *    HAL/中间件没有公开的"清 TxState"接口，只能直接写——代价是主机侧可能收到
     *    被截断的一包，由 media 的分包序号重组丢帧重同步（"卡死 vs 丢一帧"的取舍）。 */
    hcdc->TxState = 0;

    /* ③ 按契约在**动作之后**通报（告诉上层：这一包没发出去，别等它的完成回调）。
     *    一次卡死只报一个原因（与其余错误路径同形，免得 handler 的 last_err 被连续两次
     *    回调冲掉）：端点收尾失败报 USB_ERR_HW（纠正动作不完整），否则报 USB_ERR_TX_STUCK。 */
    if (inst->err_callback != NULL)
        inst->err_callback(inst, ep_abort_failed ? USB_ERR_HW : USB_ERR_TX_STUCK);

    /* ④ 立刻重试一次并判断是否真的恢复。
     *    基准先刷新：即便这次没恢复，也不该在下一个周期里再 abort 一次（限频）。 */
    inst->tx_last_ok_us = now;
    tail_before = inst->tx_tail;
    bsp_usb_process_tx();
    recovered = ((inst->tx_tail != tail_before) || (hcdc->TxState != 0u)) ? 1u : 0u;

    if (idx < USB_INSTANCE_NUM)
    {
        if (recovered)
            s_usb_status[idx].tx_recover_ok++;
        else
            s_usb_status[idx].tx_recover_fail++;
    }
    USB_Snapshot(inst, idx);

    return recovered ? BSP_OK : BSP_HW_ERR;
}

BSP_Status_e USBRecoverRxIfStalled(USBInstance *instance, uint32_t period_ms)
{
    USBInstance *inst = (instance != NULL) ? instance : (USBInstance *)s_active_inst;
    uint64_t now;
    uint8_t idx;
    uint8_t armed;

    BSP_RETURN_IF_TRUE_LOG(inst == NULL, BSP_PARAM_ERR,
                          BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "RecoverRx: no active instance!"));

    if (period_ms == 0)
        period_ms = USB_RX_STALL_DEFAULT_MS;

    idx = USB_InstanceToIndex(inst);
    if (USB_GetCdc() == NULL)
        return BSP_HW_ERR; /* 设备未初始化 */

    if (USB_SampleDevState(inst, idx) != (uint8_t)USBD_STATE_CONFIGURED)
        return BSP_BUSY; /* 未枚举：主机不在，重新武装没有意义 */

    now = DWT_GetTimeUs();

    /* 首次建立基准（从未收到过任何一帧）：先记时刻，下一轮再看是否真的没动静 */
    if (inst->rx_last_us == 0)
    {
        inst->rx_last_us = now;
        return BSP_BUSY;
    }

    if ((now - inst->rx_last_us) < ((uint64_t)period_ms * 1000u))
        return BSP_BUSY; /* 未超阈值 */

    /* ---- 判定接收停摆：重新武装 OUT 端点 ----
     * 挂回 CubeMX 的同一块接收缓冲（Buf 恒为 UserRxBufferFS/HS），对已武装的端点
     * 重复 HAL_PCD_EP_Receive 不改变数据通路。基准同时刷新 → 本入口按 period_ms 限频。 */
    if (idx < USB_INSTANCE_NUM)
    {
        s_usb_status[idx].rx_stalled++;
        s_usb_status[idx].last_err_us = now;
    }
    inst->rx_last_us = now;
    inst->rx_buff = USB_USER_RX_BUFFER; /* 记录当前挂载的缓冲（SDK 侧不 export 该变量名以外的信息） */

    (void)USBD_CDC_SetRxBuffer(&USB_TEST_DEVICE, USB_USER_RX_BUFFER);
    armed = (USBD_CDC_ReceivePacket(&USB_TEST_DEVICE) == USBD_OK) ? 1u : 0u;

    if (idx < USB_INSTANCE_NUM)
    {
        if (armed)
            s_usb_status[idx].rx_recover_ok++;
        else
            s_usb_status[idx].rx_rearm_fail++;
    }
    USB_Snapshot(inst, idx);

    if (!armed)
    {
        BSPLOG(&g_usb_log, LOG_LEVEL_WARNING, "RX re-arm failed: out endpoint unusable");
        /* 重新武装被拒 = 端点层收尾失败：按契约在动作之后通报 */
        if (inst->err_callback != NULL)
            inst->err_callback(inst, USB_ERR_RX_STALLED);
        return BSP_HW_ERR;
    }

    /* 武装被接受只说明中间件收下了请求，是否真恢复要看后续 rx_ok 是否继续增长，
     * 故这里不通报 err_callback（避免"每次无流量都报一次"把上层刷屏）。 */
    return BSP_OK;
}

BSP_Status_e USBReenumerate(USBInstance *instance)
{
    PCD_HandleTypeDef *hpcd;
    USBInstance *inst = (instance != NULL) ? instance : (USBInstance *)s_active_inst;

    BSP_RETURN_IF_TRUE_LOG(inst == NULL, BSP_PARAM_ERR,
                          BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Reenumerate: no active instance!"));

    hpcd = USB_GetPcd();
    if (hpcd == NULL)
        return BSP_HW_ERR;

    if (HAL_PCD_DevDisconnect(hpcd) != HAL_OK)
    {
        BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Reenumerate: DevDisconnect failed");
        return BSP_HW_ERR;
    }

    /* 断开保持一会儿，让主机栈确实看到"拔出" */
    USB_BusyWaitMs(USB_REENUMERATE_GAP_MS);

    if (HAL_PCD_DevConnect(hpcd) != HAL_OK)
    {
        BSPLOG(&g_usb_log, LOG_LEVEL_ERROR, "Reenumerate: DevConnect failed");
        return BSP_HW_ERR;
    }

    /* 重枚举后主机要重新打开串口、重新枚举；把两个判据基准归零，
     * 由 USBConfig 之外的首次调用重新建立，避免刚重连就被判"停摆/卡死"。 */
    inst->tx_last_ok_us = DWT_GetTimeUs();
    inst->rx_last_us = DWT_GetTimeUs();
    inst->dev_state = (uint8_t)USB_TEST_DEVICE.dev_state;

    BSPLOG(&g_usb_log, LOG_LEVEL_WARNING, "USB re-enumerated (host COM port will re-appear)");
    return BSP_OK;
}

#endif /* HAL_PCD_MODULE_ENABLED */

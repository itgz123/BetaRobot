/**
 * @file app_cmd.c
 * @brief 遥控器任务模块
 *
 * @note APP 层职责：
 *       1. 注册 DRV 层实例，提供回调
 *       2. 使用 FreeRTOS 队列实现中断到任务的数据传递
 *       3. 在任务上下文中解析数据
 */

#include "app_cmd.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
#include "drv_sbus.h"
#include "queue.h"

float ch1 = 0;
float ch2 = 0;
float ch3 = 0;
float ch4 = 0;

/*============= 私有函数声明 =============*/

static void SBUSCallback(SBUSInstance *inst);

/*============= 静态内存分配 =============*/

// SBUS 实例定义（所有参数在宏中预设）
SBUS_INSTANCE_DEF(sbus_inst, UART_SBUS, SBUSCallback);

// SBUS 原始帧队列（长度为1的覆盖式队列）
#define SBUS_QUEUE_LENGTH 1
#define SBUS_ITEM_SIZE sizeof(SBUS_RawFrame_t)

static uint8_t sbus_queue_storage[SBUS_QUEUE_LENGTH * SBUS_ITEM_SIZE];
static StaticQueue_t sbus_queue_buffer;
static QueueHandle_t sbus_queue;

// SBUS 解析后的数据
static SBUS_Data_t sbus_data;

/*============= 初始化 =============*/

static void CmdInit(void)
{
    // 创建覆盖式队列
    sbus_queue = xQueueCreateStatic(
        SBUS_QUEUE_LENGTH,
        SBUS_ITEM_SIZE,
        sbus_queue_storage,
        &sbus_queue_buffer);
    configASSERT(sbus_queue != NULL);

    // 注册 SBUS 实例（只需传入实例指针）
    if (SBUSRegister(&sbus_inst) != 0)
    {
        LOGERROR("[app_cmd] SBUS register failed!");
    }
}

/*============= 回调函数 =============*/

/**
 * @brief SBUS 接收回调（在中断中调用）
 * @param inst SBUS 实例指针
 * @note 将原始数据拷贝到队列，不进行解析
 */
static void SBUSCallback(SBUSInstance *inst)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    SBUS_RawFrame_t raw_frame;

    // 拷贝原始数据
    raw_frame.len = inst->usart_inst.rx_len;
    for (uint8_t i = 0; i < raw_frame.len && i < SBUS_FRAME_SIZE; i++)
    {
        raw_frame.data[i] = inst->usart_inst.rx_buff[i];
    }

    // 覆盖式写入队列
    xQueueOverwriteFromISR(sbus_queue, &raw_frame, &xHigherPriorityTaskWoken);

    // 触发上下文切换
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*============= 任务函数 =============*/

__attribute__((section(".itcmram"))) static void CmdTask(void)
{
    SBUS_RawFrame_t raw_frame;

    // 从队列获取数据（阻塞等待）
    if (xQueueReceive(sbus_queue, &raw_frame, portMAX_DELAY) == pdTRUE)
    {
        // 在任务上下文中解析数据
        sbus_data = SBUSDecodeFrame(raw_frame.data, raw_frame.len);

        ch1 = sbus_data.ch[0];
        ch2 = sbus_data.ch[1];
        ch3 = sbus_data.ch[2];
        ch4 = sbus_data.ch[3];

        // TODO: 在这里使用解析后的数据
        // 例如：根据通道值控制机器人
    }
}

/**
 * @brief  Cmd 任务函数
 * @param  argument: 未使用
 * @retval None
 */
__attribute__((section(".itcmram"), noreturn)) void StartCmdTask(void *argument)
{
    CmdInit();
    static uint64_t start;
    static uint64_t dt;
    LOGINFO("[freeRTOS] CMD Task Start");
    for (;;)
    {
        start = DWT_GetTimeline_us();
        CmdTask();
        dt = DWT_GetTimeline_us() - start;
        if ((dt / 1000) > CMD_FREQ_MS)
            LOGERROR("[freeRTOS] CMD Task is being DELAY! dt = %d(ms)", (dt / 1000));
        // vTaskDelay(pdMS_TO_TICKS(CMD_FREQ_MS));
    }
}
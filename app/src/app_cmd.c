#include "app_cmd.h"
#include "bsp_log.h"
#include "bsp_dwt.h"
#include "app_cfg.h"
#include "bsp_usart.h"
#include "bsp_gpio.h"

/*------------- USART 接收回调 --------------*/
static void CmdUartRxCpltCallback(USARTInstance *instance)
{
    LOGINFO("callback");
}

/*------------- USART 实例定义 --------------*/
USART_INSTANCE_DEF(cmd_uart, UART_SBUS, USART_DMA_MODE, 64, CmdUartRxCpltCallback);
uint8_t buf[] = "hello world";

static void CmdInit(void)
{
    USARTRegister(&cmd_uart);
}

static void CmdTask(void)
{
    // USARTTransmit(&cmd_uart, buf, 12);
}

/**
 * @brief  Cmd 任务函数
 * @param  argument: 未使用
 * @retval None
 */
void StartCmdTask(void *argument)
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
        vTaskDelay(pdMS_TO_TICKS(CMD_FREQ_MS));
    }
}
/**
 * @file drv_comm.c
 * @brief 通信框架总入口实现（仿 drv_daemon.c 的 DAEMON_USED 开关模式）
 *
 * CommInit() 由 function_in_main_c 调用，默认接线一套可用的链路：
 *   huart1(USART_DMA_MODE) + CommProtoCustom(0x5A 统一自定义帧)
 * 接线参数可通过 app_cfg.h 覆盖 COMM_DEFAULT_xxx 宏调整。
 *
 * DRV_COMM_USED 未定义时：整个 comm 模块不编译（各后端 .c 均有 #ifdef 保护，
 * 含 RX 任务），仅保留 CommInit() 空实现保证链接，零 RAM 占用。
 */

#include "drv_comm.h"

#ifdef DRV_COMM_USED

#include "bsp_map.h"      /* COMM_DEFAULT_UART: UART_1 */
#include "bsp_usart.h"    /* USART_DMA_MODE */
#include "media/comm_media_usart.h"
#include "proto/comm_proto_custom.h"

/* 默认实例（仿 drv_daemon：实例定义收进 drv，app 零关心） */
COMM_MEDIA_DEF(comm_uart1, MEDIA_USART);
COMM_PROTO_DEF(comm_custom, PROTO_CUSTOM);

void CommInit(void)
{
    EngineInit(); /* 内部自建 RX 任务（drv/drv_comm/engine/comm_engine.c） */

    /* 1. 介质：板载 UART（DMA 收发） */
    EngineAttachMedia(comm_uart1);
    MediaUsartConfig(comm_uart1, &(CommMediaUsart_Config_s){
                                     .uart_e = COMM_DEFAULT_UART,
                                     .tx_mode = USART_DMA_MODE,
                                     .media_id = COMM_DEFAULT_MEDIA_ID,
                                     .unpack_in_isr = 0,
                                 });

    /* 2. 协议：统一自定义帧（0x5A）挂到该介质 */
    CommProtoCustomConfig(comm_custom, &(CommProtoCustom_Config_s){
                                           .proto_id = COMM_DEFAULT_PROTO_ID,
                                           .max_payload = COMM_DEFAULT_MAX_PAYLOAD,
                                           .daemon = NULL,
                                       });
    EngineAttachProtocol(comm_uart1, comm_custom);
}

#else /* !DRV_COMM_USED */

void CommInit(void) { /* 空操作：comm 未启用 */ }

#endif /* DRV_COMM_USED */

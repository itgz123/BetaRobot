/**
 * @file drv_comm.c
 * @brief 通信框架总入口实现（仿 drv_daemon.c 的 DAEMON_USED 开关模式）
 *
 * CommInit() 由 function_in_main_c 调用，只做引擎级初始化（EngineInit：
 * 自建 RX 任务、清空路由表）。不设默认实例。
 *
 * COMM_DEF 定义的实例由定义方自己接线：
 *   COMM_DEF(comm_uart1, MEDIA_USART, PROTO_CUSTOM);
 *   void MyCommInit(void) {
 *       CommRegister(&comm_uart1);   // register + config（按类型分发）
 *   }
 * CommRegister 内部对介质/协议按类型分发默认 Config，参数可通过
 * app_cfg.h 覆盖 COMM_DEFAULT_xxx 宏调整。
 *
 * DRV_COMM_USED 未定义时：整个 comm 模块不编译（各后端 .c 均有 #ifdef 保护，
 * 含 RX 任务），仅保留 CommInit() 空实现保证链接，零 RAM 占用。
 */

#include "drv_comm.h"

#ifdef DRV_COMM_USED

#include "comm_instance.h" /* 派生 Config 声明（MediaUsartConfig / CommProtoCustomConfig 等） */
#include "bsp_map.h"       /* COMM_DEFAULT_UART: UART_1 */
#include "bsp_usart.h"     /* USART_DMA_MODE */
#include "bsp_uart_log.h"

void CommInit(void)
{
    EngineInit(); /* 内部自建 RX 任务（drv/drv_comm/engine/comm_engine.c） */
}

int8_t CommRegister(CommInstance *inst)
{
    if (!inst || !inst->media || !inst->proto)
    {
        LOGERROR("[comm] CommRegister: inst/media/proto NULL!");
        return -1;
    }

    CommMedia *media = COMM_INSTANCE_MEDIA(inst);
    CommProto *proto = COMM_INSTANCE_PROTO(inst);

    /* 1. 介质接线：按类型分发 */
    switch (inst->media_type)
    {
    case MEDIA_USART:
        MediaUsartConfig(media, &(CommMediaUsart_Config_s){
                                     .uart_e = COMM_DEFAULT_UART,
                                     .tx_mode = USART_DMA_MODE,
                                     .media_id = COMM_DEFAULT_MEDIA_ID,
                                     .unpack_in_isr = 0,
                                 });
        break;
    /* TODO 阶段2/3：CAN / USB / MEM 的默认接线（对应 Config 已在后端实现） */
    default:
        LOGWARNING("[comm] media_type %d not wired in CommRegister", (int)inst->media_type);
        return -1;
    }
    EngineAttachMedia(media);

    /* 2. 协议接线：按类型分发 */
    switch (inst->proto_type)
    {
    case PROTO_CUSTOM:
        CommProtoCustomConfig(proto, &(CommProtoCustom_Config_s){
                                         .proto_id = COMM_DEFAULT_PROTO_ID,
                                         .max_payload = COMM_DEFAULT_MAX_PAYLOAD,
                                         .daemon = NULL,
                                     });
        break;
    /* TODO 阶段2/3：SBUS / SEASKY / REFEREE 的默认接线 */
    default:
        LOGWARNING("[comm] proto_type %d not wired in CommRegister", (int)inst->proto_type);
        return -1;
    }
    EngineAttachProtocol(media, proto);

    inst->inited = 1;
    return 0;
}

int8_t CommSend(CommInstance *inst, CommId_t comm_id, const uint8_t *payload, uint16_t len)
{
    if (!inst || !inst->inited)
    {
        return -1;
    }
    CommMedia *media = COMM_INSTANCE_MEDIA(inst);
    return EngineSend(media->media_id, comm_id, payload, len);
}

int8_t CommRegisterConsumer(CommInstance *inst, const EngineConsumer_Config_s *cfg)
{
    if (!inst)
    {
        return -1;
    }
    return EngineRegisterConsumer(cfg);
}

#else /* !DRV_COMM_USED */

void CommInit(void) { /* 空操作：comm 未启用 */ }

#endif /* DRV_COMM_USED */

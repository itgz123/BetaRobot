/**
 * @file drv_comm.c
 * @brief 通信框架-顶层（CommInstance 统一入口）实现
 *
 * 两段式接口（对齐 bsp_usart 的 USARTRegister/USARTConfig 模式）：
 *   - CommRegister（不可重入）：media 后端注册 → proto 挂 vtable → 引擎接线 → 消费回调
 *   - CommConfig  （可重入）  ：介质参数下发（USART → bsp USARTConfig，可反复改配置）
 */

#include "drv_comm.h"
// 各后端 Register/Init（token 拼接分发的具体实现在这里按类型调用）
#include "comm_media_usart.h"
#include "comm_proto_raw.h"
#include "comm_engine.h"

#ifdef DRV_COMM_USED

int8_t CommRegister(CommInstance *inst)
{
    CommMedia *media;
    CommProto *proto;

    if (inst == NULL || inst->media == NULL || inst->proto == NULL)
        return -1;

    media = COMM_INSTANCE_MEDIA(inst);
    proto = COMM_INSTANCE_PROTO(inst);

    /* 1. media 后端注册（不可重入：USART 内部做 bsp USARTRegister 防重复注册）
     * @note 按 inst->media_type 分发，不能用 media->type：DEF 宏静态定义时
     *       尚未写入，默认 0 会误判为 MEDIA_CAN */
    switch (inst->media_type)
    {
    case MEDIA_USART:
        if (MediaUsartRegister((CommMediaUsart *)inst->media) != 0)
            return -1;
        break;
    default:
        return -1; /* 介质类型未支持 */
    }

    /* 2. proto 后端初始化（挂 vtable），按 inst->proto_type 分发
     * @note 同上，避免用未初始化的基类字段 */
    switch (inst->proto_type)
    {
    case PROTO_RAW:
        if (CommProtoRawInit((CommProtoRaw *)inst->proto) != 0)
            return -1;
        break;
    default:
        return -1; /* 协议类型未支持 */
    }

    /* 3. 引擎接线：media 接收分发 → proto 挂载（引擎表静态零初始化，无需 EngineInit） */
    if (EngineAttachMedia(media) != 0)
        return -1;
    if (EngineAttachProtocol(proto, media) != 0)
        return -1;

    /* 建立反向指针：media/proto 回指所属 comm 实例 */
    media->parent = inst;
    proto->parent = inst;
    inst->inited = 1;
    return 0;
}

int8_t CommConfig(CommInstance *inst, const CommConfig_s *cfg)
{
    if (inst == NULL || inst->media == NULL || cfg == NULL)
        return -1;
    if (!inst->inited)
        return -1; /* 须先 CommRegister */

    /* 1. 介质参数（media_cfg 非空才下发；可重入，运行期可改波特率/发送模式等） */
    if (cfg->media_cfg != NULL)
    {
        switch (inst->media_type)
        {
        case MEDIA_USART:
            if (MediaUsartConfig((CommMediaUsart *)inst->media, (USART_Config_s *)cfg->media_cfg) != 0)
                return -1;
            break;
        default:
            return -1; /* 介质类型未支持 */
        }
    }

    /* 2. 出帧消费回调（可重入：引擎按 proto 覆盖式注册，运行期可修改） */
    if (cfg->on_frame != NULL)
    {
        if (EngineRegisterConsumer(COMM_INSTANCE_PROTO(inst), cfg->on_frame) != 0)
            return -1;
    }
    return 0;
}

#endif /* DRV_COMM_USED */

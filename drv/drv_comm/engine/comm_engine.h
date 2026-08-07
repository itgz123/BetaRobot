/**
 * @file comm_engine.h
 * @brief 通信框架-引擎层（Engine）
 *
 * 职责：统一接线 media 接收 → proto 解包 → 出帧分发，替代 app 层手动接线。
 *   - EngineAttachMedia：注册介质，接管 media->rx_cb（接收分发）
 *   - EngineAttachProtocol：建立 media↔proto 挂载，接管 proto->on_frame（出帧分发）
 *   - EngineRegisterConsumer：注册出帧消费回调（按 proto 匹配，可多消费者）
 *   - EngineSend：统一发送入口（经挂载的 proto 打包 → media 发出）
 *
 * 接收路径（当前实现，media 默认 UNPACK_IN_ISR 直解）：
 *   bsp ISR → media 适配钩子 → MediaHandleRx → 引擎 rx 钩子
 *   → 遍历该 media 挂载的 proto → ProtoUnpack → 出帧 → 引擎分发钩子 → 消费回调
 *
 * @note 环形缓冲 + EngineRxTask（UNPACK_IN_TASK 任务解包）为后续扩展点。
 */

#ifndef COMM_ENGINE_H
#define COMM_ENGINE_H

#include <stdint.h>
#include "app_cfg.h"
//
#include "comm_media.h"
#include "comm_proto.h"

#ifdef DRV_COMM_USED

/* 数量宏（默认值，可被 app_cfg.h 覆盖） */
#ifndef ENGINE_LINK_NUM
#define ENGINE_LINK_NUM 16 /* media↔proto 挂载表容量 */
#endif
#ifndef ENGINE_CONSUMER_NUM
#define ENGINE_CONSUMER_NUM 16 /* 消费者表容量 */
#endif

/**
 * @brief 初始化引擎（清零挂载表/消费者表，可重复调用）
 * @retval 0 成功
 */
int8_t EngineInit(void);

/**
 * @brief 注册介质到引擎（接管 media->rx_cb = 引擎接收钩子）
 * @param media CommMedia 基类指针
 * @retval 0 成功；-1 参数非法
 */
int8_t EngineAttachMedia(CommMedia *media);

/**
 * @brief 建立 media↔proto 挂载（接管 proto->on_frame = 引擎分发钩子）
 * @param proto CommProto 基类指针
 * @param media CommMedia 基类指针（须先 EngineAttachMedia）
 * @retval 0 成功；-1 参数非法 / 挂载表满
 *
 * @note 一介质可挂多协议（各协议靠自身帧校验拒绝不属自己的字节）；
 *       一协议可绑多介质（消费回调按 proto 匹配）。
 */
int8_t EngineAttachProtocol(CommProto *proto, CommMedia *media);

/**
 * @brief 设置出帧消费者回调（可重入：同 proto 重复调用即覆盖更新）
 * @param proto 要监听的 CommProto 基类指针
 * @param cb    消费回调（与 ProtoFrameCallback 同签名）
 * @retval 0 成功；-1 参数非法 / 消费者表满
 *
 * @note 同 proto 再次调用以新回调覆盖旧回调，支持运行期修改消费逻辑；
 *       cb 为 NULL 时不注册（保留现有回调）。
 */
int8_t EngineRegisterConsumer(CommProto *proto, ProtoFrameCallback cb);

/**
 * @brief 统一发送入口：经该 media 挂载的 proto 打包后由 media 发出
 * @param media   CommMedia 基类指针
 * @param payload 待发送 payload 指针（长度 = 编译期确定）
 * @retval 0 成功；-1 无挂载协议 / 发送失败
 */
int8_t EngineSend(CommMedia *media, const uint8_t *payload);

#endif /* DRV_COMM_USED */
#endif /* COMM_ENGINE_H */

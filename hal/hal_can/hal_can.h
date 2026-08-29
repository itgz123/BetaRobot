/**
 * @file hal_can.h
 * @brief CAN 外设重配置层（可选覆盖层）
 *
 * @note 职责仅封装"重配置"：默认不调用本层任何函数，bsp 直接使用 CubeMX 初始化
 *       （MX_FDCAN1/2/3_Init / MX_CAN1/2_Init）的结果。
 *       若要在运行时覆盖外设配置（波特率 / FrameFormat / 各元素数量尺寸等），
 *       可在 app 层调用本层函数，配置参数直接用 HAL 原生 InitTypeDef
 *       （从 hfdcan->Init 拷贝一份再修改，或自行构造 FDCAN_InitTypeDef）。
 *       - 本层不内置默认值，也不被 bsp 层调用，只提供可选覆盖入口。
 *       - 无状态，handle 一律参数传入。
 */

#ifndef __HAL_CAN_H
#define __HAL_CAN_H

#include "main.h"

/*------------- FDCAN 重配置 --------------*/

#if defined(HAL_FDCAN_MODULE_ENABLED)

/**
 * @brief 重配置 FDCAN 外设（覆盖 CubeMX 初始化）
 * @param hfdcan FDCAN 句柄（&hfdcan1/2/3）
 * @param init   配置指针（HAL 原生 FDCAN_InitTypeDef：可从 hfdcan->Init 拷贝后修改，或自行构造）
 * @retval HAL_OK 成功
 * @retval HAL_ERROR 参数非法或 HAL_FDCAN_Init 失败
 *
 * @note 逐字段覆盖 hfdcan->Init.* 后重新调用 HAL_FDCAN_Init。
 *       再次 Init 不会重复配置时钟/GPIO/NVIC（State 已是 READY），
 *       但会清空分配区 Message RAM 并重写 SIDFC/XIDFC 等基址，
 *       故本函数必须在配置过滤器之前调用（由调用方保证时序）。
 * @note 本层默认不被 bsp 层调用（bsp 直接使用 CubeMX 初始化结果）；
 *       若需修改配置，由 app 层在首次 CANConfig 之前调用本函数。
 */
HAL_StatusTypeDef HalCanReconfigureFdcan(FDCAN_HandleTypeDef *hfdcan, const FDCAN_InitTypeDef *init);

#endif /* HAL_FDCAN_MODULE_ENABLED */

/*------------- BxCAN 重配置 --------------*/

#if defined(HAL_CAN_MODULE_ENABLED)

/**
 * @brief 重配置 BxCAN 外设（覆盖 CubeMX 初始化）
 * @param hcan CAN 句柄（&hcan1/2）
 * @param init 配置指针（HAL 原生 CAN_InitTypeDef：可从 hcan->Init 拷贝后修改，或自行构造）
 * @retval HAL_OK 成功
 * @retval HAL_ERROR 参数非法或 HAL_CAN_Init 失败
 *
 * @note 逐字段覆盖 hcan->Init.* 后重新调用 HAL_CAN_Init。
 *       再次 Init 不会重复配置时钟/GPIO/NVIC（State 已是 READY），
 *       但会重置外设状态，故本函数必须在配置过滤器之前调用（由调用方保证时序）。
 * @note 本层默认不被 bsp 层调用（bsp 直接使用 CubeMX 初始化结果）；
 *       若需修改配置，由 app 层在首次 CANConfig 之前调用本函数。
 */
HAL_StatusTypeDef HalCanReconfigureBxcan(CAN_HandleTypeDef *hcan, const CAN_InitTypeDef *init);

#endif /* HAL_CAN_MODULE_ENABLED */

#endif /* __HAL_CAN_H */

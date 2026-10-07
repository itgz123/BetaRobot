/**
 * @file referee2026_proto.h
 * @brief 2026 赛季机器人裁判系统**对接层**：串口外设配置 / CRC 算法与查表
 *
 * 与 referee2026_frame.h 分工：frame.h 只描述"协议长什么样"（零依赖，PC 可编译自检）；
 * 本文件承接"要落到 MCU 上的东西"——需要 HAL（UART_InitTypeDef）与 lib_crc（CRC 类型与函数）。
 *
 * 依据：RoboMaster 2026 机甲大师高校系列赛通信协议 V2.0.0（20260626）
 *   §1.1 串口协议格式（表 1-1 波特率等参数）
 *   串口协议附录 V1.9.0（0xA9 图传遥控帧的 CRC）
 *
 * 本文件只放"静态描述量"（编译期常量，无函数）：
 *   ① 串口外设配置：`referee2026_uart_init[]`（按链路一条）—— 协议要求的硬件参数
 *   ② CRC 类型：直接用 lib_crc 的 LIB_CRC_Algo_t / LIB_CRC_Table_t，不另造结构体
 *   ③ CRC 算法与查表：本模块自备的三张表 + 算法描述（定义在 referee2026_proto.c）
 *
 * @note **CRC 表归本模块所有，与 `LIB_CRC_TABLES_USED` 无关**（lib 有 6 张通用表，
 *       本模块不用它的）。本模块对 lib 的依赖只有**函数** `LIB_CRC_TableCalc` / `LIB_CRC_Direct`，
 *       即 `LIB_CRC_USED`。"要不要 CRC 功能"与"带哪几张表"是两件事，别混。
 *
 * @warning 本文件**不设 `_USED` 开关**（与 lib_crc.h 同一写法）：
 *          开关只决定"哪些 .c 参与编译"，头文件一律无条件声明。
 *          本文件的入口 drv_referee2026.h 已把开关按序把好：
 *          `HAL_UART_MODULE_ENABLED` → `DRV_REFEREE2026_USED` → `LIB_CRC_USED`。
 *          与全仓约定一致：`#define XXX_USED` = 打开，写 `0` **仍是打开**。
 *
 * @note 全文件只剩一道 `#ifdef HAL_UART_MODULE_ENABLED`（只管第一节）。它不是模块开关，
 *       而是 HAL 的能力开关：该宏未开时 stm32*xx_hal_conf.h 根本不引 hal_uart.h，
 *       **`UART_InitTypeDef` 这个类型都不存在**，无法声明。CRC 各节不依赖 HAL，不加壳。
 */

#ifndef __REFEREE2026_PROTO_H
#define __REFEREE2026_PROTO_H

#include <stdint.h>

#include "main.h"    /* UART_InitTypeDef 与 UART_* 常量（H7 的 UART_WORDLENGTH_8B 经 _ex 头带出） */
#include "lib_crc.h" /* LIB_CRC_Algo_t / LIB_CRC_Table_t / LIB_CRC_Direct / LIB_CRC_TableCalc */

#include "referee2026_frame.h"

/*============================================
 *   一、串口外设配置（协议 §1.1 表 1-1）
 *============================================*/

/* main.h 已在上方无条件引入，故此处的判据此时一定可见
 * （顺序颠倒会恒假 —— HAL 各 MODULE_ENABLED 宏是 main.h 带出来的）。 */
#ifdef HAL_UART_MODULE_ENABLED

/**
 * @brief 各数据链路的串口外设配置，按 Referee2026LinkType_e 下标取用
 *
 * 数值出处（协议 §1.1 表 1-1）：
 *   常规链路    COMMON 115200 —— 裁判系统服务器 ↔ 主控模块转发，电源管理模块 User 串口
 *   图传链路    VIDEO  921600 —— 选手端图传 ↔ 图传模块转发，图传发送端串口
 *   雷达无线    RADAR  ——      —— 电磁波，**不占串口**，本行全 0（占位，保持下标对齐）
 *   非链路      NONE   115200 —— 0x0306 自定义控制器 ↔ 选手端图传模块直连串口
 *
 * 三条链路的其余参数一致：8 位数据、1 位停止、无校验、无硬件流控、收发双向、16 倍过采样。
 *
 * @warning **必须用指定初始化器（`.BaudRate = ...`）填表，不能按位置填**：
 *          本表要同时在 F4（DJI_A/DJI_C）和 H7（DM_MC02*）上编译，而两家的
 *          `UART_InitTypeDef` **字段数不同** —— H7 多出 `OneBitSampling` 与 `ClockPrescaler`。
 *          指定初始化器下，H7 这两项被隐式清零，恰好就是 HAL 的期望默认
 *          （`UART_ONE_BIT_SAMPLE_DISABLE` / `UART_PRESCALER_DIV1`，两者宏值都是 0），
 *          故一份表两边通吃，无需 `#if defined(STM32H7xx)` 分支。
 *          六个公共字段的画法两边同名同义（`UART_WORDLENGTH_8B` 在 H7 上由
 *          stm32h7xx_hal_uart_ex.h 提供，经 main.h 可达）。
 *
 * @note 与 bsp_usart 的分工：BSP 层只登记实例、不管硬件配置（见 bsp_usart.h 的说明），
 *       本表是"协议要求的硬件参数"在固件里的**权威记录**——CubeMX 里各串口就照这张表配。
 *       若哪天要在运行期改波特率，可把对应行直接喂给 HAL_UART_Init 重初始化。
 * @note 表里给的是**协议侧**要求；实际用哪块板子的哪个 UART 由 app 在 Config 的 `uart_e`
 *       里给（板级映射见 bsp_map）。表与 uart_e 不一致时，以本表为准去改 CubeMX。
 */
extern const UART_InitTypeDef referee2026_uart_init[REFEREE2026_LINK_TYPE_COUNT];

/**
 * @brief 该链路是否占用串口
 * @retval 0 不占（雷达无线链路：数据来自电磁波，实例不注册 USART）
 * @retval 1 占（其余链路）
 * @note 取用 `referee2026_uart_init[link]` 前先过这道判据，别把 RADAR 行的全 0 当合法配置。
 */
static inline uint8_t Referee2026LinkHasUart(Referee2026LinkType_e link)
{
    return (link == REFEREE2026_LINK_TYPE_RADAR) ? 0u : 1u;
}

#endif /* HAL_UART_MODULE_ENABLED */

/*============================================
 *      二、CRC 类型（复用 lib_crc，不再另造）
 *============================================*/

/**
 * @brief CRC 描述用 lib_crc 的两个现成类型
 *
 *   LIB_CRC_Algo_t  ：算法描述（Init / PolySize / Poly / XorOut / RefIn / RefOut 六元组）
 *   LIB_CRC_Table_t ：算法描述 + 256 项查表指针（`algo` + `table`）
 *
 * 三个 CRC 的参数（协议附录一 / 附录 V1.9.0）：
 *
 * | 用途                | 宽度 | Poly   | Init   | XorOut | RefIn | RefOut | 表（本模块自备）           |
 * | ------------------- | ---- | ------ | ------ | ------ | ----- | ------ | --------------------------- |
 * | 帧头 CRC8           | 8    | 0x31   | 0xFF   | 0x00   | 1     | 1      | s_referee2026_crc8_tab      |
 * | 整包 CRC16          | 16   | 0x1021 | 0xFFFF | 0x0000 | 1     | 1      | s_referee2026_crc16_tab     |
 * | 0xA9 遥控帧 CRC16   | 16   | 0x1021 | 0xFFFF | 0x0000 | 0     | 0      | s_referee2026_crc16_remote_tab |
 *
 * @note 表只由 (Poly, 宽度, RefIn) 三个量决定，Init/XorOut/RefOut 是查表时另加在寄存器上的，
 *       **不体现在表数据里**。所以"帧 CRC16"与"0xA9 CRC16"虽然同 poly，但方向相反 ⇒
 *       是两张不同的表；而"帧 CRC8(init 0xFF)"与 lib 的 CRC-8/MAXIM(init 0x00) 表完全相同。
 * @note 查表方向由 `algo.reverse_in` 决定，**挂错表不会报错、只会静默算出错值**，
 *       两条 16 位算法的表绝不能对调。
 *
 * @note 覆盖范围（协议表 1-2/1-3）：CRC8 覆盖帧头前 4B（SOF、data_length 小端、seq）；
 *       CRC16 覆盖「帧头 5B + cmd_id 2B + 数据段」，**不含自身的 2B 帧尾**。
 * @note 校验值一律**小端**写进帧（CRC16 占 2B）。
 */
typedef LIB_CRC_Algo_t Referee2026CrcAlgo_t;   //!< 算法描述（六元组）
typedef LIB_CRC_Table_t Referee2026CrcTable_t; //!< 算法 + 查表

/*============================================
 *   三、CRC 算法与查表（表归本模块所有）
 *============================================*/

/**
 * @brief 帧头 CRC8 的算法描述（Init 0xFF，RefIn/RefOut = 1）
 * @note poly/方向与 lib 的 `LIB_CRC_ALGO_CRC8_MAXIM` 相同，**只差 Init**（那个是 0x00）——
 *       但本模块用自己的表，不引 lib 的表，见头文件开头的说明。
 */
extern const Referee2026CrcAlgo_t referee2026_crc8_algo;

/**
 * @brief 整包 CRC16 的算法描述（Init 0xFFFF，RefIn/RefOut = 1）
 * @note 标准名 CRC-16/MCRF4XX。
 */
extern const Referee2026CrcAlgo_t referee2026_crc16_algo;

/**
 * @brief 0xA9 图传遥控帧 CRC16 的算法描述（Init 0xFFFF，**非反射**）
 * @note 标准名 CRC-16/CCITT-FALSE。与上面那条同 poly 反方向，表也不同。
 */
extern const Referee2026CrcAlgo_t referee2026_crc16_remote_algo;

/**
 * @brief 帧头 CRC8 查表描述（= referee2026_crc8_algo + s_referee2026_crc8_tab）
 * @note 用法：`LIB_CRC_TableCalc(&referee2026_crc8_table, frame, REFEREE2026_OFF_CRC8)`
 */
extern const Referee2026CrcTable_t referee2026_crc8_table;

/**
 * @brief 整包 CRC16 查表描述（= referee2026_crc16_algo + s_referee2026_crc16_tab）
 * @note 用法：对整包（帧头 + cmd_id + 数据段，即除帧尾外的全部）计算，
 *       结果小端写入帧尾 2B。
 */
extern const Referee2026CrcTable_t referee2026_crc16_table;

/**
 * @brief 0xA9 图传遥控帧 CRC16 查表描述（= referee2026_crc16_remote_algo + 非反射表）
 */
extern const Referee2026CrcTable_t referee2026_crc16_remote_table;

#endif /* __REFEREE2026_PROTO_H */

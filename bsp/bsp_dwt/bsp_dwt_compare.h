/**
 * @file bsp_dwt_compare.h
 * @brief DWT 4 通道比较器接口
 * @note 比较器匹配产生的是调试事件：调试器连接时可能直接停机，或经
 *       DEMCR.MON_EN 进入 DebugMonitor；它不经过 NVIC。本模块只负责配置与
 *       查询，不实现异常处理。
 * @note 匹配地址掩码写入 MASK[4:0]：bit=1 表示忽略对应地址位（仅低 5 位有效）。
 *       PC 匹配要求目标地址半字对齐，且应命中指令的第一个半字。
 * @note CYCMATCH 是 FUNCTION0 的字段，仅通道 0 支持周期计数匹配。
 * @note 依赖 DWT_Init() 已完成（使能 DEMCR.TRCENA）；本模块不得调用 DWT_Init()
 *       （会清零 CYCCNT，破坏全局时间轴）。
 * @note 调用 DWT_CompareInit() 会关闭并接管全部比较器通道，与调试器硬件断点
 *       同时使用时需自行协调。
 */

#ifndef __BSP_DWT_COMPARE_H
#define __BSP_DWT_COMPARE_H

#include "stdint.h"

/*------------- 宏定义 --------------*/

#define DWT_COMPARE_CHANNEL_NUM 4u

/*------------- 类型定义 --------------*/

typedef enum : uint8_t
{
    DWT_COMPARE_CH0 = 0,
    DWT_COMPARE_CH1,
    DWT_COMPARE_CH2,
    DWT_COMPARE_CH3
} DWT_CompareChannel_e;

typedef enum : uint8_t
{
    DWT_COMPARE_MODE_DISABLED = 0,
    DWT_COMPARE_MODE_PC,      /* 指令地址匹配 */
    DWT_COMPARE_MODE_DATA_R,  /* 数据读 */
    DWT_COMPARE_MODE_DATA_W,  /* 数据写 */
    DWT_COMPARE_MODE_DATA_RW, /* 数据读写 */
    DWT_COMPARE_MODE_CYCCNT   /* 周期计数匹配 */
} DWT_CompareMode_e;

typedef struct
{
    DWT_CompareMode_e mode;
    uint32_t value; /* PC / 数据地址 / 周期数 */
    uint32_t mask;  /* 数据地址掩码；0 表示不使用 */
} DWT_CompareConfig_s;

/*------------- 外部接口声明 --------------*/

/**
 * @brief 初始化比较器模块：解锁 DWT 并关闭全部通道
 * @return 1=成功，0=未初始化 DWT 或内核比较器不足 4 个
 */
uint8_t DWT_CompareInit(void);

/**
 * @brief 配置一个比较器通道（配置后仍处于关闭状态，需再调用 Enable）
 * @param channel 通道号
 * @param config  比较模式、比较值与地址掩码
 * @return 1=成功，0=参数非法或通道不支持该模式
 */
uint8_t DWT_CompareConfigure(DWT_CompareChannel_e channel, const DWT_CompareConfig_s *config);

/**
 * @brief 使能已配置的比较器通道，并清除历史匹配状态
 * @return 1=成功，0=通道未配置或参数非法
 */
uint8_t DWT_CompareEnable(DWT_CompareChannel_e channel);

/**
 * @brief 关闭比较器通道并清除匹配状态
 * @return 1=成功，0=参数非法
 */
uint8_t DWT_CompareDisable(DWT_CompareChannel_e channel);

/**
 * @brief 查询比较器是否匹配
 * @param channel 通道号
 * @param matched 输出：1=已匹配，0=未匹配（仅返回 1 时有效）
 * @return 1=查询成功，0=参数非法
 * @note 每次匹配只会上报一次：查询到 matched=1 后会自动清除该状态。
 */
uint8_t DWT_CompareGetMatched(DWT_CompareChannel_e channel, uint8_t *matched);

#endif /* __BSP_DWT_COMPARE_H */

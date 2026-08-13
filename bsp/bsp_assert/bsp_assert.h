/**
 * @file bsp_assert.h
 * @brief 系统状态断言模块：集中记录 bsp/drv/app 各层初始化异常计数
 *
 * @note 设计目的：
 *       1. 提供一个系统状态结构体（内含多个 uint32_t 计数器），定义全局状态
 *          变量 bsp_assert_state，用于判断系统初始化/运行状态是否健康。
 *       2. 在 bsp/drv 各模块注册/配置失败处，以及 app 调用 bsp/drv 判断返回值
 *          非 0 处，调用 BSP_AssertCount() 进行 +1 上报。
 *       3. 所有 "+1" 都收敛到 BSP_AssertCount()：若发现初始化异常，直接在该
 *          函数打断点，结合调用堆栈即可定位是哪个模块的注册/配置/调用异常。
 *       4. 通过读取 bsp_assert_state 各计数器判断系统运行状态，如
 *          bsp_assert_state.total != 0 说明存在初始化异常。
 */

#ifndef __BSP_ASSERT_H
#define __BSP_ASSERT_H

#include <stdint.h>

/*============================================
 *              通用断言宏
 *============================================*/
#define BSP_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BSP_STATIC_ASSERT_MAP_SIZE(arr, max) _Static_assert(BSP_ARRAY_SIZE(arr) == (max), #arr " size must equal " #max)

#define BSP_RETURN_IF_TRUE(cond, ret) \
    do                                \
    {                                 \
        if (cond)                     \
        {                             \
            return (ret);             \
        }                             \
    } while (0)

#define BSP_RETURN_IF_TRUE_LOG(cond, ret, log_expr) \
    do                                              \
    {                                               \
        if (cond)                                   \
        {                                           \
            log_expr;                               \
            return (ret);                           \
        }                                           \
    } while (0)

/*============================================
 *              断言分组枚举
 *============================================*/

/**
 * @brief 断言分组：按 bsp/drv/app 三层划分
 */
typedef enum
{
    BSP_ASSERT_GROUP_BSP = 0, /* BSP 层：bsp_* 注册/配置异常 */
    BSP_ASSERT_GROUP_DRV,     /* DRV 层：drv_* 注册/配置异常 */
    BSP_ASSERT_GROUP_APP,     /* APP 层：调用 bsp/drv 返回值非 0 等异常 */
    BSP_ASSERT_GROUP_NUM,     /* 分组数量（不可作为分组使用） */
} BSP_Assert_Group_e;

/*============================================
 *              系统状态结构体
 *============================================*/

/**
 * @brief 系统状态结构体
 * @note 各计数器均为 uint32_t；bsp_assert_state 为其全局实例。
 *       原 bsp/drv_system 中的 system_init_state 已并入本结构体，
 *       对应 counter[BSP_ASSERT_GROUP_APP]（app 初始化累计错误）。
 */
typedef struct
{
    uint32_t counter[BSP_ASSERT_GROUP_NUM]; /* 各层错误计数（索引见 BSP_Assert_Group_e） */
    uint32_t total;                         /* 错误总数（各层计数之和） */
} BSP_Assert_State_s;

/* 系统状态全局变量（供各层读取判断运行状态） */
extern BSP_Assert_State_s bsp_assert_state;

/*============================================
 *              接口声明
 *============================================*/

/**
 * @brief 断言错误计数 +1（所有出错上报的唯一入口）
 * @param group 所属分组，见 BSP_Assert_Group_e
 * @note 调试：在此函数打断点，初始化异常时会在断点停下，
 *       通过调用堆栈定位是哪个模块的注册/配置/调用异常
 */
void BSP_AssertCount(BSP_Assert_Group_e group);

/*============================================
 *              便捷宏
 *============================================*/

#define BSP_ASSERT_COUNT(group) BSP_AssertCount(group)
#define BSP_ASSERT_BSP() BSP_AssertCount(BSP_ASSERT_GROUP_BSP)
#define BSP_ASSERT_DRV() BSP_AssertCount(BSP_ASSERT_GROUP_DRV)
#define BSP_ASSERT_APP() BSP_AssertCount(BSP_ASSERT_GROUP_APP)

/**
 * @brief 调用函数并断言返回值为 0（0 视为成功，非 0 视为异常）
 * @param func 要调用的 bsp/drv 函数调用表达式
 * @note 用于 app 层调用 bsp/drv 注册/配置/初始化函数时自动检查返回值：
 *       返回值非 0 则调用 BSP_AssertCount(BSP_ASSERT_GROUP_APP) 计数 +1。
 *       约定：仅适用于"返回 0 表示成功"的注册/配置类函数
 *       （如 CANRegister / CANConfig 返回 0 成功、-1 失败）。
 * @example BSP_ASSERT_APP_CALL(CANRegister(&can_inst));
 */
#define BSP_ASSERT_APP_CALL(func)                  \
    do                                             \
    {                                              \
        if ((func) != 0)                           \
        {                                          \
            BSP_AssertCount(BSP_ASSERT_GROUP_APP); \
        }                                          \
    } while (0)

#endif /* __BSP_ASSERT_H */

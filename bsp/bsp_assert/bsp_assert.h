/**
 * @file bsp_assert.h
 * @brief 通用断言/检查宏：参数检查提前返回、数组长度静态校验
 *
 * @note 系统状态计数（app 初始化错误 / 任务超时等）已迁出本模块，
 *       见 bsp/bsp_sys_status/bsp_sys_status.h（BSP_ASSERT_APP_CALL 等）。
 */

#ifndef __BSP_ASSERT_H
#define __BSP_ASSERT_H

#include <stdint.h>

/*============================================
 *              通用断言宏
 *============================================*/
#define BSP_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BSP_STATIC_ASSERT_MAP_SIZE(arr, max) _Static_assert(BSP_ARRAY_SIZE(arr) == (max), #arr " size must equal " #max)

#define BSP_RETURN_IF_TRUE(cond, ret)                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (cond)                                                                                                      \
        {                                                                                                              \
            return (ret);                                                                                              \
        }                                                                                                              \
    } while (0)

#define BSP_RETURN_IF_TRUE_LOG(cond, ret, log_expr)                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (cond)                                                                                                      \
        {                                                                                                              \
            log_expr;                                                                                                  \
            return (ret);                                                                                              \
        }                                                                                                              \
    } while (0)

#endif /* __BSP_ASSERT_H */

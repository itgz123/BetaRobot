/**
 * @file bsp_check.h
 * @brief BSP 通用检查宏（用于统一注册流程样板代码）
 */

#ifndef __BSP_CHECK_H
#define __BSP_CHECK_H

/**
 * @brief 条件成立时直接返回
 */
#define BSP_RETURN_IF_TRUE(cond, ret) \
    do                                \
    {                                 \
        if (cond)                     \
        {                             \
            return (ret);             \
        }                             \
    } while (0)

/**
 * @brief 条件成立时执行日志表达式并返回
 */
#define BSP_RETURN_IF_TRUE_LOG(cond, ret, log_expr) \
    do                                              \
    {                                               \
        if (cond)                                   \
        {                                           \
            log_expr;                               \
            return (ret);                           \
        }                                           \
    } while (0)

#endif /* __BSP_CHECK_H */

/**
 * @file bsp.h
 * @brief 板级支持包统一头文件
 *
 * @note 包含板级常量、检查宏、板选逻辑
 *       各开发板的类型/枚举/映射数组定义分别在 hal/xxx/bsp_map.h/.c 中
 */

#ifndef __BSP_H
#define __BSP_H

// 开发板类型
#define DM_MC02 0
#define DJI_A 1
#define DJI_C 2

/*============================================
 *              内核类型常量
 *============================================*/
#define CORTEX_M4 0
#define CORTEX_M7 1

/*============================================
 *              CAN 控制器类型常量
 *============================================*/
#define BSP_CAN_IP_BXCAN 0
#define BSP_CAN_IP_FDCAN 1

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

#endif /* __BSP_H */

/**
 * @file bsp_assert.c
 * @brief 系统状态断言模块实现
 *
 * @note 使用示例：
 *       1. bsp/drv 注册/配置函数内部出错时上报（内部 +1）：
 *          BSP_ASSERT_BSP();   // bsp 层
 *          BSP_ASSERT_DRV();   // drv 层
 *       2. app 调用 bsp/drv 函数判断返回值非 0 时上报（外部 +1）：
 *          if (CANRegister(&inst) != 0) BSP_ASSERT_APP();
 */

#include "bsp_assert.h"
#include "app_cfg.h"

#ifdef BSP_ASSERT_USED

/* 系统状态全局变量定义 */
BSP_Assert_State_s bsp_assert_state = {0};

void BSP_AssertCount(BSP_Assert_Group_e group)
{
    if (group < BSP_ASSERT_GROUP_NUM) /* 在此打断点：初始化/运行异常会停在这里，查看调用堆栈定位异常模块 */
    {
        bsp_assert_state.counter[group]++;
        bsp_assert_state.total++;
    }
}

#endif /* BSP_ASSERT_USED */

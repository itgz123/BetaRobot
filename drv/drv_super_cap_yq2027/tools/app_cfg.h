/**
 * @file app_cfg.h
 * @brief 离线测试用的 app_cfg.h 桩
 *
 * @note 只声明驱动需要的开关；真正的板级配置不在这里。
 */

#ifndef __TEST_APP_CFG_H
#define __TEST_APP_CFG_H

#include <stdint.h>

#define HAL_CAN_MODULE_ENABLED
#define DRV_SUPER_CAP_YQ2027_USED
#define DAEMON_USED

#endif /* __TEST_APP_CFG_H */

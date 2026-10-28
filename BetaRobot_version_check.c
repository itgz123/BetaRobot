/**
 * @file BetaRobot_version_check.c
 * @brief 强制每个 app 检查 BetaRobot 版本，防止 app_cfg.h 漏写 include 绕过。
 * @note 本文件被所有 app 目标无条件编译：
 *       1. 先包含 app_cfg.h，拿到 app 自己定义的 APP_LOCKED_BETAROBOT_VERSION；
 *       2. 再包含 BetaRobot_version.h，执行版本检查（未锁定/不匹配都会 #error）。
 *       即使 app_cfg.h 自己没写 #include "BetaRobot_version.h"，也绕不开。
 */

#include "app_cfg.h"
#include "BetaRobot_version.h"

/* ISO C 不允许空翻译单元；同时避免 -Wpedantic 报错。 */
typedef int BetaRobotVersionCheckTranslationUnitNotEmpty;

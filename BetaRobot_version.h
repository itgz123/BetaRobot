/**
 * @file BetaRobot_version.h
 * @brief BetaRobot 主仓库版本号。由 .githooks/pre_commit.py 自动生成，请勿手动修改。
 * @note app 必须锁定内核版本（写在 app 的 app_cfg.h 里，且在 include 之前）：
 *         #define APP_LOCKED_BETAROBOT_VERSION 20261029175747
 *         #include "BetaRobot_version.h"
 *       未定义该宏或版本不匹配都会在下面的检查里直接 #error。
 */
#ifndef BETAROBOT_VERSION_H
#define BETAROBOT_VERSION_H

/* 版本号格式：V_年_月_日_时_分_秒（本地时间） */
#define V_2026_10_29_17_57_47   20261029175747
#define BETAROBOT_VERSION        V_2026_10_29_17_57_47
#define BETAROBOT_VERSION_STRING "V_2026_10_29_17_57_47"

/* app 必须锁定版本：未定义或版本不一致都在这里 #error */
#ifndef APP_LOCKED_BETAROBOT_VERSION
#error "app 未锁定 BetaRobot 版本：请在 app_cfg.h 中定义 APP_LOCKED_BETAROBOT_VERSION"
#elif BETAROBOT_VERSION != APP_LOCKED_BETAROBOT_VERSION
#error "BetaRobot 版本不匹配：app 的 APP_LOCKED_BETAROBOT_VERSION 与主仓库 BetaRobot_version.h 不一致"
#endif

#endif /* BETAROBOT_VERSION_H */

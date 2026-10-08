/**
 * @file lib_chassis_def.h
 * @brief 底盘轮系专属词汇：只有"具体某型底盘"才用得着的类型/枚举（纯几何，无运算）
 * @author TRW
 * @date 2026-10-08
 *
 * @note 与 lib_chassis.h **互不包含**，两个头各自自足：
 *       - 本文件：内核用不到的轮系词汇（如四轮轮序）。某一型底盘专有的东西放这里。
 *       - lib_chassis.h：内核（轮/配置/状态/实例/接口），它需要的东西都在它自己里面。
 *       6 对类型封装两个都包含（`#include "lib_chassis.h"` + `#include "lib_chassis_def.h"`），
 *       app 只包含某型封装的 .h 即拿到全部。
 *
 * @note 坐标系（全项目统一）：x+ 向前, y+ 向左, w+ 逆时针（俯视）。
 */

#ifndef __LIB_CHASSIS_DEF_H
#define __LIB_CHASSIS_DEF_H

#include <stdint.h>

/*============================================
 *              四轮底盘的轮序
 *============================================*/
/**
 * @brief 四轮底盘的轮序（全向/麦轮/全舵共用）：LF 左前 → LB 左后 → RB 右后 → RF 右前
 * @note 顺时针绕车一圈，相邻轮在几何上相邻，便于按侧/按角成对处理
 */
typedef enum : uint8_t
{
    LIB_CHASSIS_WHEEL_LF = 0, // 左前
    LIB_CHASSIS_WHEEL_LB,     // 左后
    LIB_CHASSIS_WHEEL_RB,     // 右后
    LIB_CHASSIS_WHEEL_RF,     // 右前
    LIB_CHASSIS_WHEEL_4NUM,   // 四轮底盘的轮数
} LibChassisWheel4_e;

#endif // !__LIB_CHASSIS_DEF_H

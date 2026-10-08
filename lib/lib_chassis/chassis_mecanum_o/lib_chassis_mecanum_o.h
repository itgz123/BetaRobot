/**
 * @file lib_chassis_mecanum_o.h
 * @brief 麦轮 O 型（4 个麦轮，矩形四角，滚子俯视为 O）几何封装
 * @author TRW
 * @date 2026-10-08
 *
 * @note O 型 = 左右两侧麦轮镜像安装：左轮轮面朝前（α=0），右轮轮面朝后（α=π），
 *       同一侧两个轮子的滚子方向一致，俯视下滚子构成 O 字。
 *       正/逆解与力分配走 lib_chassis 内核（见 lib_chassis.h）。
 * @note 安装角 α = [LF, LB, RB, RF] = [0, 0, π, π]；滚子角 β = [+π/4, -π/4, +π/4, -π/4]。
 *       行 = [1, -1, -(hl+hw)] / [1, +1, -(hl+hw)] / [-1, +1, -(hl+hw)] / [-1, -1, -(hl+hw)]，
 *       与旧 drv_chassis_lite 的 MECANUM_O 闭式解**逐项相同**。
 * @note 与 mecanum_x 的唯一差别在右后/右前两轮的安装角（本型为 π，X 型为 0）。
 *       若实车左右装反，把 α 的右轮两项改回 0 即退化为 X 型（反之亦然）。
 * @note 编译开关：app_cfg.h 里定义 LIB_CHASSIS_MECANUM_O_USED 时本型 .c 参与编译（.h 不受门控，只有声明）。
 */

#ifndef __LIB_CHASSIS_MECANUM_O_H
#define __LIB_CHASSIS_MECANUM_O_H

#include <stdint.h>
#include "lib_chassis.h"
#include "lib_chassis_def.h"

/*============================================
 *              几何配置
 *============================================*/
typedef struct
{
    float half_len;   // 轮心到车体中心的前后距离 (m)，前=+x
    float half_width; // 轮心到车体中心的左右距离 (m)，左=+y
    float radius;     // 轮子半径 (m)，>0
} LibChassisMecanumOGeom_s;

/**
 * @brief 初始化麦轮 O 型内核实例
 * @param inst 内核实例
 * @param geom 几何尺寸
 * @retval 0  成功
 * @retval -1 空指针 / 参数非法
 */
int8_t LibChassisMecanumOInit(LibChassisInstance_s *inst, const LibChassisMecanumOGeom_s *geom);

#endif // !__LIB_CHASSIS_MECANUM_O_H

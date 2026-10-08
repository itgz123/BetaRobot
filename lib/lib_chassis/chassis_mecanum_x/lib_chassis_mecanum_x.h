/**
 * @file lib_chassis_mecanum_x.h
 * @brief 麦轮 X 型（4 个麦轮，矩形四角，滚子俯视为 X）几何封装
 * @author TRW
 * @date 2026-10-08
 *
 * @note 4 个麦轮轮面均朝前（安装角 α=0），滚子角按对角线成对：滚子方向在俯视下构成 X。
 *       正/逆解与力分配走 lib_chassis 内核（见 lib_chassis.h）。
 * @note 滚子角 β 取值：[LF, LB, RB, RF] = [+π/4, -π/4, +π/4, -π/4]，即对角线两轮滚子同向
 *       （标准 X 型：四轮滚子轴都指向车心）。由刚体模型（g = e_r - cotβ·e_t）推出，
 *       行 = [1, -1, -(hl+hw)] / [1, +1, -(hl+hw)] / [1, -1, +(hl+hw)] / [1, +1, +(hl+hw)]。
 * @note 与 mecanum_o 的唯一差别在右后/右前两轮的**安装角**：本型 α 全为 0（轮面朝前），
 *       O 型把右侧两项改成 π（右轮绕 z 转 180°，驱动方向与滚子倾角同时反向）。
 * @note ⚠️ 已删除的旧 drv_chassis_lite 里 MECANUM_X 的闭式解与本刚体模型只差一个 vy 符号
 *       （旧式的 vx/vy 两列对应 g=(1,+1)，w 列却对应 g=(1,-1)，自相矛盾；O 型则完全一致）。
 *       上线前请实车核对：给纯 +vy（向左平移）指令，看车是否真的向左。
 * @note 编译开关：app_cfg.h 里定义 LIB_CHASSIS_MECANUM_X_USED 时本型 .c 参与编译（.h 不受门控，只有声明）。
 */

#ifndef __LIB_CHASSIS_MECANUM_X_H
#define __LIB_CHASSIS_MECANUM_X_H

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
} LibChassisMecanumXGeom_s;

/**
 * @brief 初始化麦轮 X 型内核实例
 * @param inst 内核实例
 * @param geom 几何尺寸
 * @retval 0  成功
 * @retval -1 空指针 / 参数非法
 */
int8_t LibChassisMecanumXInit(LibChassisInstance_s *inst, const LibChassisMecanumXGeom_s *geom);

#endif // !__LIB_CHASSIS_MECANUM_X_H

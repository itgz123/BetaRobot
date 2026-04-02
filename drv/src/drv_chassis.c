/**
 * @file drv_chassis.c
 * @brief 底盘运动学解算实现
 *
 * @note 电机编号：1-4 对应 左前(LF)、左后(LB)、右后(RB)、右前(RF)
 *       坐标系：向前 +X，向左 +Y，逆时针旋转 +W
 *       轮子转向：前进方向为正
 */

#include "drv_chassis.h"
#include "app_cfg.h"

/*============================================
 *              运动学逆解
 *============================================*/

WheelSpeed_t ChassisInverseKinematics(ChassisVelCmd_t *cmd)
{
    WheelSpeed_t wheel;

#if CHASSIS_TYPE == CHASSIS_MECANUM_X
    /**
     * @brief 麦克纳姆轮 X 型安装逆解
     * @note  俯视时4个辊子形成 X 形
     *        LF(+)  RF(-)
     *          \    /
     *           \  /
     *           /  \
     *          /    \
     *        LB(-)  RB(+)
     *
     *        左边轮子：前进方向与逆时针一致
     *        右边轮子：前进方向与顺时针一致，需要取反
     */
    wheel.w1 = (cmd->vx - cmd->vy - cmd->w * K_INVERSE) / WHEEL_RADIUS;
    wheel.w2 = (cmd->vx + cmd->vy - cmd->w * K_INVERSE) / WHEEL_RADIUS;
    wheel.w3 = (cmd->vx - cmd->vy + cmd->w * K_INVERSE) / WHEEL_RADIUS;
    wheel.w4 = (cmd->vx + cmd->vy + cmd->w * K_INVERSE) / WHEEL_RADIUS;

#elif CHASSIS_TYPE == CHASSIS_MECANUM_O
    /**
     * @brief 麦克纳姆轮 O 型安装逆解
     * @note  俯视时4个辊子形成 O 形
     *        LF(-)  RF(+)
     *          /    \
     *         /      \
     *         \      /
     *          \    /
     *        LB(+)  RB(-)
     */
    wheel.w1 = (cmd->vx + cmd->vy - cmd->w * K_INVERSE) / WHEEL_RADIUS;
    wheel.w2 = (cmd->vx - cmd->vy - cmd->w * K_INVERSE) / WHEEL_RADIUS;
    wheel.w3 = (cmd->vx + cmd->vy + cmd->w * K_INVERSE) / WHEEL_RADIUS;
    wheel.w4 = (cmd->vx - cmd->vy + cmd->w * K_INVERSE) / WHEEL_RADIUS;

#elif CHASSIS_TYPE == CHASSIS_OMNI_X
    /**
     * @brief 全向轮 X 型安装逆解
     * @note  四轮都与底盘成 45 度
     *          LF      RF
     *            \    /
     *             \  /
     *             /  \
     *            /    \
     *          LB      RB
     *
     *        LF: 左前45度    RF: 右前45度
     *        LB: 左后45度    RB: 右后45度
     *
     *        由于轮子45度安装，需要除以 √2
     */
    wheel.w1 = (cmd->vx - cmd->vy - cmd->w * K_INVERSE) / (WHEEL_RADIUS * SQRT2);
    wheel.w2 = (cmd->vx + cmd->vy - cmd->w * K_INVERSE) / (WHEEL_RADIUS * SQRT2);
    wheel.w3 = (cmd->vx - cmd->vy + cmd->w * K_INVERSE) / (WHEEL_RADIUS * SQRT2);
    wheel.w4 = (cmd->vx + cmd->vy + cmd->w * K_INVERSE) / (WHEEL_RADIUS * SQRT2);

#elif CHASSIS_TYPE == CHASSIS_OMNI_CROSS
    /**
     * @brief 全向轮十字安装逆解（支持矩形底盘）
     * @note  前后左右四个轮子，轮子方向分别朝前、后、左、右
     *
     *             RF(前)
     *               |
     *               |
     *    LB(左) ----+---- RB(右)
     *               |
     *               |
     *             LF(后)
     *
     *        LF: 后轮，朝后     RF: 前轮，朝前
     *        LB: 左轮，朝左     RB: 右轮，朝右
     *
     *        前后轮使用 K_CROSS_A，左右轮使用 K_CROSS_B
     */
    wheel.w1 = (cmd->vy + cmd->w * K_CROSS_A) / WHEEL_RADIUS;
    wheel.w2 = (-cmd->vx + cmd->w * K_CROSS_B) / WHEEL_RADIUS;
    wheel.w3 = (cmd->vx + cmd->w * K_CROSS_B) / WHEEL_RADIUS;
    wheel.w4 = (-cmd->vy + cmd->w * K_CROSS_A) / WHEEL_RADIUS;

#else
    wheel.w1 = 0;
    wheel.w2 = 0;
    wheel.w3 = 0;
    wheel.w4 = 0;
#endif

    return wheel;
}

/*============================================
 *              运动学正解
 *============================================*/

ChassisVelCmd_t ChassisForwardKinematics(WheelSpeed_t *wheel)
{
    ChassisVelCmd_t cmd;

#if CHASSIS_TYPE == CHASSIS_MECANUM_X
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.vy = (-wheel->w1 + wheel->w2 - wheel->w3 + wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * K_FORWARD / 4.0f;

#elif CHASSIS_TYPE == CHASSIS_MECANUM_O
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.vy = (wheel->w1 - wheel->w2 + wheel->w3 - wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * K_FORWARD / 4.0f;

#elif CHASSIS_TYPE == CHASSIS_OMNI_X
    // 全向轮X型正解需要乘以 √2
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * WHEEL_RADIUS * SQRT2 / 4.0f;
    cmd.vy = (-wheel->w1 + wheel->w2 - wheel->w3 + wheel->w4) * WHEEL_RADIUS * SQRT2 / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * K_FORWARD / 4.0f;

#elif CHASSIS_TYPE == CHASSIS_OMNI_CROSS
    // 十字型正解
    cmd.vx = (-wheel->w2 + wheel->w3) * WHEEL_RADIUS / 2.0f;
    cmd.vy = (wheel->w1 - wheel->w4) * WHEEL_RADIUS / 2.0f;
    cmd.w = (wheel->w1 * K_CROSS_A + wheel->w2 * K_CROSS_B + wheel->w3 * K_CROSS_B + wheel->w4 * K_CROSS_A) * WHEEL_RADIUS / 4.0f;

#else
    cmd.vx = 0;
    cmd.vy = 0;
    cmd.w = 0;
#endif

    cmd.mode = 0;
    return cmd;
}

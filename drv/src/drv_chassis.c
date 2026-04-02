/**
 * @file drv_chassis.c
 * @brief 底盘运动学解算实现
 *
 * @note 电机编号：1-4 对应 左前(LF)、左后(LB)、右后(RB)、右前(RF)
 *       坐标系：向前 +X，向左 +Y，逆时针旋转 +W
 *       轮子转向：面朝轮子，逆时针为正
 */

#include "drv_chassis.h"
#include "app_cfg.h"

/*============================================
 *              底盘参数选择
 *============================================*/

#if CHASSIS_TYPE == CHASSIS_MECANUM_X || CHASSIS_TYPE == CHASSIS_MECANUM_O
#define CHASSIS_PARAM (WHEELBASE_A + WHEELBASE_B)
#elif CHASSIS_TYPE == CHASSIS_OMNI_X || CHASSIS_TYPE == CHASSIS_OMNI_CROSS
#define CHASSIS_PARAM OMNI_L
#endif

/*============================================
 *              运动学逆解
 *============================================*/

WheelSpeed_t ChassisInverseKinematics(ChassisVelCmd_t *cmd)
{
    WheelSpeed_t wheel;
    float k = CHASSIS_PARAM / WHEEL_RADIUS;

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
     */
    wheel.w1 = (cmd->vx - cmd->vy - cmd->w * k) / WHEEL_RADIUS;
    wheel.w2 = (cmd->vx + cmd->vy - cmd->w * k) / WHEEL_RADIUS;
    wheel.w3 = (cmd->vx - cmd->vy + cmd->w * k) / WHEEL_RADIUS;
    wheel.w4 = (cmd->vx + cmd->vy + cmd->w * k) / WHEEL_RADIUS;

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
    wheel.w1 = (cmd->vx + cmd->vy - cmd->w * k) / WHEEL_RADIUS;
    wheel.w2 = (cmd->vx - cmd->vy - cmd->w * k) / WHEEL_RADIUS;
    wheel.w3 = (cmd->vx + cmd->vy + cmd->w * k) / WHEEL_RADIUS;
    wheel.w4 = (cmd->vx - cmd->vy + cmd->w * k) / WHEEL_RADIUS;

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
     */
    wheel.w1 = (cmd->vx - cmd->vy - cmd->w * k) / WHEEL_RADIUS;
    wheel.w2 = (cmd->vx + cmd->vy - cmd->w * k) / WHEEL_RADIUS;
    wheel.w3 = (cmd->vx - cmd->vy + cmd->w * k) / WHEEL_RADIUS;
    wheel.w4 = (cmd->vx + cmd->vy + cmd->w * k) / WHEEL_RADIUS;

#elif CHASSIS_TYPE == CHASSIS_OMNI_CROSS
    /**
     * @brief 全向轮十字安装逆解
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
     *        旋转时4轮同时参与，负载均衡
     */
    wheel.w1 = (cmd->vy + cmd->w * k) / WHEEL_RADIUS;
    wheel.w2 = (-cmd->vx + cmd->w * k) / WHEEL_RADIUS;
    wheel.w3 = (cmd->vx + cmd->w * k) / WHEEL_RADIUS;
    wheel.w4 = (-cmd->vy + cmd->w * k) / WHEEL_RADIUS;

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
    float k = WHEEL_RADIUS / CHASSIS_PARAM;

#if CHASSIS_TYPE == CHASSIS_MECANUM_X
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.vy = (-wheel->w1 + wheel->w2 - wheel->w3 + wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * k / 4.0f;

#elif CHASSIS_TYPE == CHASSIS_MECANUM_O
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.vy = (wheel->w1 - wheel->w2 + wheel->w3 - wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * k / 4.0f;

#elif CHASSIS_TYPE == CHASSIS_OMNI_X
    cmd.vx = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.vy = (-wheel->w1 + wheel->w2 - wheel->w3 + wheel->w4) * WHEEL_RADIUS / 4.0f;
    cmd.w = (-wheel->w1 - wheel->w2 + wheel->w3 + wheel->w4) * k / 4.0f;

#elif CHASSIS_TYPE == CHASSIS_OMNI_CROSS
    cmd.vx = (-wheel->w2 + wheel->w3) * WHEEL_RADIUS / 2.0f;
    cmd.vy = (wheel->w1 - wheel->w4) * WHEEL_RADIUS / 2.0f;
    cmd.w = (wheel->w1 + wheel->w2 + wheel->w3 + wheel->w4) * k / 4.0f;

#else
    cmd.vx = 0;
    cmd.vy = 0;
    cmd.w = 0;
#endif

    cmd.mode = 0;
    return cmd;
}
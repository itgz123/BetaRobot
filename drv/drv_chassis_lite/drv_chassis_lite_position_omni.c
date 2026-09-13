/**
 * @file drv_chassis_lite_position_omni.c
 * @brief 轻量级全向/麦轮底盘运动学实现（纯计算，不依赖电机）
 * @author TRW
 * @date 2026-09-13
 */

#include "drv_chassis_lite_position_omni.h"
#include "app_cfg.h"

#ifdef DRV_CHASSIS_LITE_POSITION_OMNI_USED

#include "lib_math.h"

/*
 * 坐标系:  x+向前, y+向左, w+逆时针
 * 轮子顺序: LF(0左前), LB(1左后), RB(2右后), RF(3右前)
 *
 * 所有逆解公式直接计算轮子接触点线速度 [m/s],
 * 最终乘以 reduction_ratio / wheel_radius 得到电机侧角速度 [rad/s].
 *
 * 所有正解公式从电机侧角速度 [rad/s] 出发,
 * 乘 wheel_radius / reduction_ratio 得到接触点线速度 [m/s].
 */

/*============================================================================*
 * OMNI_T — 全向轮 (前后左右)
 *
 * 轮子在矩形4条边的中点上:
 *   LF(前): 位置 (+x/2, 0),   驱动 -y (向左)  (有效力臂 x/2)
 *   LB(左): 位置 (0, +y/2),   驱动 +x (向前)  (有效力臂 y/2)
 *   RB(后): 位置 (-x/2, 0),   驱动 +y (向右)  (有效力臂 x/2)
 *   RF(右): 位置 (0, -y/2),   驱动 -x (向后)  (有效力臂 y/2)
 *
 * 逆解:
 *   v_LF = -vy - w·x/2
 *   v_LB =  vx - w·y/2
 *   v_RB =  vy - w·x/2
 *   v_RF = -vx - w·y/2
 *
 * 正解:
 *   vx = (v_LB - v_RF) / 2
 *   vy = (v_RB - v_LF) / 2
 *   w  = -((v_LF+v_RB)/x + (v_LB+v_RF)/y) / 2
 *============================================================================*/
static void omnit_inverse(const ChassisLitePositionOmniInstance_t *inst, ChassisCmd_t cmd, ChassisLitePositionRef_t *out)
{
    float y2 = inst->y * 0.5f;
    float x2 = inst->x * 0.5f;
    float k = inst->reduction_ratio / inst->wheel_radius;

    out->drive_speed[WHEEL_LF] = (-cmd.vy - cmd.w * x2) * k;
    out->drive_speed[WHEEL_LB] = (cmd.vx - cmd.w * y2) * k;
    out->drive_speed[WHEEL_RB] = (cmd.vy - cmd.w * x2) * k;
    out->drive_speed[WHEEL_RF] = (-cmd.vx - cmd.w * y2) * k;
}

static ChassisCmd_t omnit_forward(const ChassisLitePositionOmniInstance_t *inst, const ChassisLitePositionRef_t *in)
{
    float k = inst->wheel_radius / inst->reduction_ratio;
    float v_lf = in->drive_speed[WHEEL_LF] * k;
    float v_lb = in->drive_speed[WHEEL_LB] * k;
    float v_rb = in->drive_speed[WHEEL_RB] * k;
    float v_rf = in->drive_speed[WHEEL_RF] * k;

    ChassisCmd_t cmd;
    cmd.vx = (v_lb - v_rf) * 0.5f;
    cmd.vy = (v_rb - v_lf) * 0.5f;
    cmd.w = -((v_lf + v_rb) / inst->x + (v_lb + v_rf) / inst->y) * 0.5f;
    cmd.enable = 0;
    return cmd;
}

/*============================================================================*
 * OMNI_X — 全向轮 (4个角)
 *
 * 轮子在矩形4个角, 切线方向驱动.
 *   a = x/2, b = y/2, r = √(a²+b²)
 *   d_i = (py_i, -px_i) / r  (顺时针切线)
 *
 * 约定: 向前(vx>0)时所有轮子正转, 向左(vy>0)时 LF/RF 反转、LB/RB 正转.
 *
 * 逆解 (所有轮子顺时针切线驱动, 旋转项均为 -w·r):
 *   v_LF = ( b·vx - a·vy) / r - w·r
 *   v_LB = ( b·vx + a·vy) / r - w·r
 *   v_RB = (-b·vx + a·vy) / r - w·r
 *   v_RF = (-b·vx - a·vy) / r - w·r
 *
 * 正解:
 *   vx = r/(4·b) · (v_LF + v_LB - v_RB - v_RF)
 *   vy = r/(4·a) · (-v_LF + v_LB + v_RB - v_RF)
 *   w  = (-v_RB - v_RF - v_LF - v_LB) / (4·r)
 *============================================================================*/
static void omnix_inverse(const ChassisLitePositionOmniInstance_t *inst, ChassisCmd_t cmd, ChassisLitePositionRef_t *out)
{
    float a = inst->x * 0.5f;
    float b = inst->y * 0.5f;
    float r = inst->r;
    float inv_r = 1.0f / r;
    float k = inst->reduction_ratio / inst->wheel_radius;

    out->drive_speed[WHEEL_LF] = ((b * cmd.vx - a * cmd.vy) * inv_r - cmd.w * r) * k;
    out->drive_speed[WHEEL_LB] = ((b * cmd.vx + a * cmd.vy) * inv_r - cmd.w * r) * k;
    out->drive_speed[WHEEL_RB] = ((-b * cmd.vx + a * cmd.vy) * inv_r - cmd.w * r) * k;
    out->drive_speed[WHEEL_RF] = ((-b * cmd.vx - a * cmd.vy) * inv_r - cmd.w * r) * k;
}

static ChassisCmd_t omnix_forward(const ChassisLitePositionOmniInstance_t *inst, const ChassisLitePositionRef_t *in)
{
    float k = inst->wheel_radius / inst->reduction_ratio;
    float v_lf = in->drive_speed[WHEEL_LF] * k;
    float v_lb = in->drive_speed[WHEEL_LB] * k;
    float v_rb = in->drive_speed[WHEEL_RB] * k;
    float v_rf = in->drive_speed[WHEEL_RF] * k;

    float a = inst->x * 0.5f;
    float b = inst->y * 0.5f;
    float r = inst->r;
    float r4b = r * 0.25f / b;
    float r4a = r * 0.25f / a;

    ChassisCmd_t cmd;
    cmd.vx = r4b * (v_lf + v_lb - v_rb - v_rf);
    cmd.vy = r4a * (-v_lf + v_lb + v_rb - v_rf);
    cmd.w = (-v_rb - v_rf - v_lf - v_lb) / (4.0f * r);
    cmd.enable = 0;
    return cmd;
}

/*============================================================================*
 * MECANUM_O — 麦轮 (俯视锟子O型)
 *
 * 左轮锟子 \, 右轮锟子 /, 俯视构成 O 型.
 * 全部驱动方向为 ±x (前后).
 *
 * 逆解 (Z = (x+y)/2):
 *   v_LF =  vx - vy - Z·w
 *   v_LB =  vx + vy - Z·w
 *   v_RB = -vx + vy - Z·w   (右后电机安装方向相反)
 *   v_RF = -vx - vy - Z·w   (右前电机安装方向相反)
 *
 * 正解:
 *   vx = (v_LF + v_LB - v_RB - v_RF) / 4
 *   vy = (v_LB - v_RF - v_LF + v_RB) / 4
 *   w  = (-v_RF - v_RB - v_LF - v_LB) / 2·(x+y)
 *============================================================================*/
static void mecanumo_inverse(const ChassisLitePositionOmniInstance_t *inst, ChassisCmd_t cmd, ChassisLitePositionRef_t *out)
{
    float Z = (inst->x + inst->y) * 0.5f;
    float k = inst->reduction_ratio / inst->wheel_radius;

    out->drive_speed[WHEEL_LF] = (cmd.vx - cmd.vy - Z * cmd.w) * k;
    out->drive_speed[WHEEL_LB] = (cmd.vx + cmd.vy - Z * cmd.w) * k;
    out->drive_speed[WHEEL_RB] = (-cmd.vx + cmd.vy - Z * cmd.w) * k;
    out->drive_speed[WHEEL_RF] = (-cmd.vx - cmd.vy - Z * cmd.w) * k;
}

static ChassisCmd_t mecanumo_forward(const ChassisLitePositionOmniInstance_t *inst, const ChassisLitePositionRef_t *in)
{
    float k = inst->wheel_radius / inst->reduction_ratio;
    float v_lf = in->drive_speed[WHEEL_LF] * k;
    float v_lb = in->drive_speed[WHEEL_LB] * k;
    float v_rb = in->drive_speed[WHEEL_RB] * k;
    float v_rf = in->drive_speed[WHEEL_RF] * k;

    ChassisCmd_t cmd;
    cmd.vx = (v_lf + v_lb - v_rb - v_rf) * 0.25f;
    cmd.vy = (v_lb - v_rf - v_lf + v_rb) * 0.25f;
    cmd.w = (-v_rf - v_rb - v_lf - v_lb) / (2.0f * (inst->x + inst->y));
    cmd.enable = 0;
    return cmd;
}

/*============================================================================*
 * MECANUM_X — 麦轮 (俯视锟子X型)
 *
 * 左轮锟子 /, 右轮锟子 \, 俯视构成 X 型.
 * 全部驱动方向为 ±x (前后).
 *
 * 逆解 (Z = (x+y)/2):
 *   v_LF =  vx + vy - Z·w
 *   v_LB =  vx - vy - Z·w
 *   v_RB =  vx + vy + Z·w
 *   v_RF =  vx - vy + Z·w
 *
 * 正解:
 *   vx = (v_LF + v_LB + v_RB + v_RF) / 4
 *   vy = (v_LF + v_RB - v_RF - v_LB) / 4
 *   w  = (v_RB + v_RF - v_LF - v_LB) / 2·(x+y)
 *============================================================================*/
static void mecanumx_inverse(const ChassisLitePositionOmniInstance_t *inst, ChassisCmd_t cmd, ChassisLitePositionRef_t *out)
{
    float Z = (inst->x + inst->y) * 0.5f;
    float k = inst->reduction_ratio / inst->wheel_radius;

    out->drive_speed[WHEEL_LF] = (cmd.vx + cmd.vy - Z * cmd.w) * k;
    out->drive_speed[WHEEL_LB] = (cmd.vx - cmd.vy - Z * cmd.w) * k;
    out->drive_speed[WHEEL_RB] = (cmd.vx + cmd.vy + Z * cmd.w) * k;
    out->drive_speed[WHEEL_RF] = (cmd.vx - cmd.vy + Z * cmd.w) * k;
}

static ChassisCmd_t mecanumx_forward(const ChassisLitePositionOmniInstance_t *inst, const ChassisLitePositionRef_t *in)
{
    float k = inst->wheel_radius / inst->reduction_ratio;
    float v_lf = in->drive_speed[WHEEL_LF] * k;
    float v_lb = in->drive_speed[WHEEL_LB] * k;
    float v_rb = in->drive_speed[WHEEL_RB] * k;
    float v_rf = in->drive_speed[WHEEL_RF] * k;

    ChassisCmd_t cmd;
    cmd.vx = (v_lf + v_lb + v_rb + v_rf) * 0.25f;
    cmd.vy = (v_lf + v_rb - v_rf - v_lb) * 0.25f;
    cmd.w = (v_rb + v_rf - v_lf - v_lb) / (2.0f * (inst->x + inst->y));
    cmd.enable = 0;
    return cmd;
}

/*============================================================================*
 * 对外接口
 *============================================================================*/

int8_t ChassisLitePositionOmniInit(ChassisLitePositionOmniInstance_t *inst, const ChassisLitePositionOmni_Cfg_s *cfg)
{
    if (inst == NULL || cfg == NULL)
        return -1;
    if (cfg->wheel_radius <= 0.0f)
        return -2;
    if (cfg->reduction_ratio <= 0.0f)
        return -3;

    /* 复制配置 */
    inst->wheel_radius = cfg->wheel_radius;
    inst->reduction_ratio = cfg->reduction_ratio;
    inst->x = cfg->x;
    inst->y = cfg->y;
    inst->chassis_type = cfg->chassis_type;

    /* 计算 r (着地点到中心距离) */
    switch (cfg->chassis_type)
    {
    case CHASSISTYPE_OMNI_T:
        inst->r = 0.0f;
        break;

    case CHASSISTYPE_OMNI_X:
    case CHASSISTYPE_MECANUM_O:
    case CHASSISTYPE_MECANUM_X:
        if (cfg->x <= 0.0f || cfg->y <= 0.0f)
            return -4;
        inst->r = Lib_Math_Sqrt(cfg->x * cfg->x * 0.25f +
                                cfg->y * cfg->y * 0.25f);
        break;

    default:
        return -5; /* 不支持的轮系（舵轮不走本模块） */
    }

    return 0;
}

void ChassisLitePositionOmniInverse(const ChassisLitePositionOmniInstance_t *inst, ChassisCmd_t cmd, ChassisLitePositionRef_t *out)
{
    if (inst == NULL || out == NULL)
        return;

    out->valid = 1;
    for (int i = 0; i < WHEEL_NUM; i++)
    {
        out->steer_target[i] = 0.0f; /* 全向/麦轮无舵向 */
        out->drive_speed[i] = 0.0f;
    }

    switch (inst->chassis_type)
    {
    case CHASSISTYPE_OMNI_T:
        omnit_inverse(inst, cmd, out);
        break;
    case CHASSISTYPE_OMNI_X:
        omnix_inverse(inst, cmd, out);
        break;
    case CHASSISTYPE_MECANUM_O:
        mecanumo_inverse(inst, cmd, out);
        break;
    case CHASSISTYPE_MECANUM_X:
        mecanumx_inverse(inst, cmd, out);
        break;
    default:
        out->valid = 0;
        break;
    }
}

ChassisCmd_t ChassisLitePositionOmniForward(const ChassisLitePositionOmniInstance_t *inst, const ChassisLitePositionRef_t *in)
{
    ChassisCmd_t cmd = {0};

    if (inst == NULL || in == NULL)
        return cmd;

    switch (inst->chassis_type)
    {
    case CHASSISTYPE_OMNI_T:
        cmd = omnit_forward(inst, in);
        break;
    case CHASSISTYPE_OMNI_X:
        cmd = omnix_forward(inst, in);
        break;
    case CHASSISTYPE_MECANUM_O:
        cmd = mecanumo_forward(inst, in);
        break;
    case CHASSISTYPE_MECANUM_X:
        cmd = mecanumx_forward(inst, in);
        break;
    default:
        break;
    }

    return cmd;
}

#endif /* DRV_CHASSIS_LITE_POSITION_OMNI_USED */

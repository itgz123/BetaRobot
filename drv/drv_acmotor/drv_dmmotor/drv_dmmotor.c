/**
 * @file drv_dmmotor.c
 * @brief 达妙 DM4310 电机驱动实现
 *
 * @note 数据流：
 *       CAN中断 → DMMotorDecode → raw_data + data（含多圈+双温度）
 *       APP任务 → DMMotorSend → ApplyLimits + MIT帧（仅填t_ff）+ 发送
 */

#include "drv_dmmotor.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include <math.h>
#include <string.h>

/*============================================
 *              内部函数声明
 *============================================*/

// 单电机虚函数实现
static int8_t DMMotorRegister(ACMotorInstance *inst, ACMotor_Init_Config_s *cfg);
static void DMMotorEnable(ACMotorInstance *inst);
static void DMMotorDisable(ACMotorInstance *inst);
static void DMMotorSetRef(ACMotorInstance *inst, float ref);
static void DMMotorSend(ACMotorInstance *inst);

// 内部辅助函数
static void DMMotorDecode(CANInstance *can_inst);
static void DMMotorSetMode(DMMotorInstance *motor, DMMotorCmd_e cmd);
static void DMMotorLostCallback(void *owner);
static uint16_t DMErrorToFlags(uint8_t err_code);

/*============================================
 *              虚函数表定义（导出）
 *============================================*/

const ACMotorInterface_s dmmotor_vtable = {
    .register_ = DMMotorRegister,
    .enable    = DMMotorEnable,
    .disable   = DMMotorDisable,
    .set_ref   = DMMotorSetRef,
    .send      = DMMotorSend,
};

/*============================================
 *              单电机虚函数实现
 *============================================*/

static int8_t DMMotorRegister(ACMotorInstance *inst, ACMotor_Init_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    DMMotorInstance *motor = (DMMotorInstance *)inst;
    DMMotorPriv_t *priv = &motor->priv;

    // 填充基类字段
    inst->brand = cfg->brand;
    inst->model = cfg->model;
    inst->reduction_ratio = cfg->reduction_ratio;
    inst->torque_coef = cfg->torque_coef;
    inst->settings.outer_loop_type = cfg->outer_loop_type;
    inst->settings.close_loop_type = cfg->close_loop_type;
    inst->settings.motor_reverse = cfg->motor_reverse;
    inst->settings.feedback_reverse = cfg->feedback_reverse;
    inst->settings.angle_feedback_src = cfg->angle_feedback_src;
    inst->settings.speed_feedback_src = cfg->speed_feedback_src;
    inst->settings.feedforward_flag = cfg->feedforward_flag;
    inst->limits = cfg->limits;
    inst->controller.pid_ref = 0.0f;

    // 配置 PID
    if (cfg->current_pid_cfg)
        PIDInit(&inst->controller.current_pid, cfg->current_pid_cfg);
    if (cfg->speed_pid_cfg)
        PIDInit(&inst->controller.speed_pid, cfg->speed_pid_cfg);
    if (cfg->angle_pid_cfg)
        PIDInit(&inst->controller.angle_pid, cfg->angle_pid_cfg);

    // 同步基类 can 指针
    inst->can = priv->can_inst;

    // 配置 CAN 实例（收发共用）
    priv->can_inst->parent = motor;
    priv->can_inst->rx_callback = DMMotorDecode;

    // 注册 daemon
    if (inst->daemon)
    {
        Daemon_Init_Config_s daemon_cfg = {
            .reload_count = cfg->daemon_reload,
            .fault_action = cfg->daemon_fault_action,
            .callback = cfg->lost_callback ? cfg->lost_callback : DMMotorLostCallback,
            .owner_id = motor,
        };
        DaemonRegister(inst->daemon, &daemon_cfg);
    }

    // 发送使能命令
    DMMotorSetMode(motor, DM_CMD_MOTOR_MODE);

    // 初始化完成
    inst->enable = 1;
    inst->data.fresh = 0;

    return 0;
}

static void DMMotorEnable(ACMotorInstance *inst)
{
    if (!inst)
        return;
    inst->enable = 1;
    DMMotorSetMode((DMMotorInstance *)inst, DM_CMD_MOTOR_MODE);
}

static void DMMotorDisable(ACMotorInstance *inst)
{
    if (!inst)
        return;
    inst->enable = 0;
    DMMotorSetMode((DMMotorInstance *)inst, DM_CMD_RESET_MODE);
}

static void DMMotorSetRef(ACMotorInstance *inst, float ref)
{
    if (inst)
        inst->controller.pid_ref = ref;
}

/**
 * @brief 单电机发送：限幅 + MIT帧（仅填t_ff）+ 发送
 */
static void DMMotorSend(ACMotorInstance *inst)
{
    if (!inst)
        return;

    DMMotorInstance *motor = (DMMotorInstance *)inst;

    if (!inst->enable)
    {
        DMMotorSetMode(motor, DM_CMD_RESET_MODE);
        return;
    }

    // 应用限制
    ACMotorApplyLimits(inst);

    float output = inst->controller.pid_ref;

    // 钳位到 DM 力矩范围
    if (output > DM_T_MAX) output = DM_T_MAX;
    if (output < DM_T_MIN) output = DM_T_MIN;

    uint16_t t_ff = (uint16_t)ACMotorFloatToUint(output, DM_T_MIN, DM_T_MAX, 12);

    // MIT 帧打包：p_des=0, v_des=0, kp=0, kd=0, 只填 t_ff
    CANInstance *can = motor->priv.can_inst;
    can->tx_buff[0] = 0;                      // p_des[15:8]
    can->tx_buff[1] = 0;                      // p_des[7:0]
    can->tx_buff[2] = 0;                      // v_des[11:4]
    can->tx_buff[3] = 0;                      // v_des[3:0] | kp[11:8]
    can->tx_buff[4] = 0;                      // kp[7:0]
    can->tx_buff[5] = 0;                      // kd[11:4]
    can->tx_buff[6] = (uint8_t)(t_ff >> 4);   // kd[3:0] | t_ff[11:8]
    can->tx_buff[7] = (uint8_t)(t_ff & 0xFF); // t_ff[7:0]

    CANTransmit(can, 1);
}

/*============================================
 *              内部辅助函数实现
 *============================================*/

/**
 * @brief DM CAN 接收解码（中断上下文）
 * @note 填入 raw_data 并转换为 data（双温度、减速比）
 */
static void DMMotorDecode(CANInstance *can_inst)
{
    DMMotorInstance *motor = (DMMotorInstance *)can_inst->parent;
    if (!motor)
        return;

    ACMotorInstance *base = &motor->base;
    DMMotorPriv_t *priv = &motor->priv;
    uint8_t *rx = can_inst->rx_buff;

    // 离线恢复：数据回来时补发使能
    if (priv->was_lost)
    {
        DMMotorSetMode(motor, DM_CMD_MOTOR_MODE);
        priv->was_lost = 0;
        priv->lost_cnt = 0;
    }

    // 喂狗
    if (base->daemon)
        DaemonReload(base->daemon);

    // 计算 dt
    uint64_t now = DWT_GetTimeUs();
    base->dt = (float)(now - priv->feed_cnt) * 1e-6f;
    priv->feed_cnt = now;

    // ---- 1. 解析 DM 反馈帧 ----
    // D[0] = ID(低4位) | ERR(高4位)
    // D[1:2] = POS[15:0], D[3:4] = VEL[11:0], D[4:5] = T[11:0]
    // D[6] = T_MOS, D[7] = T_Rotor
    MotorRawData_t *raw = &base->raw_data;
    raw->error_code       = rx[0] >> 4;                               // ERR 4bit
    raw->raw_encoder      = ((uint16_t)rx[1] << 8) | rx[2];          // POS 16bit
    raw->raw_velocity     = ((uint16_t)rx[3] << 4) | (rx[4] >> 4);   // VEL 12bit
    raw->raw_current      = (((uint16_t)rx[4] & 0x0F) << 8) | rx[5]; // T 12bit
    raw->raw_temperature_mos = (int8_t)rx[6];                           // T_MOS
    raw->raw_temperature_motor  = (int8_t)rx[7];                           // T_Rotor 线圈
    raw->fresh = 1;

    // ---- 2. 单位转换 → data ----
    MotorData_t *d = &base->data;
    float ratio = base->reduction_ratio;

    float pos = ACMotorUintToFloat(raw->raw_encoder,  DM_P_MIN, DM_P_MAX, 16);
    float vel = ACMotorUintToFloat(raw->raw_velocity, DM_V_MIN, DM_V_MAX, 12);
    float tor = ACMotorUintToFloat(raw->raw_current,  DM_T_MIN, DM_T_MAX, 12);

    // ---- 3. 多圈累积（半量程 0x8000 判向） ----
    uint16_t last_ecd = priv->last_ecd;
    priv->last_ecd = raw->raw_encoder;
    if (raw->raw_encoder - last_ecd > 0x8000)
        priv->total_round--;
    else if (raw->raw_encoder - last_ecd < -0x8000)
        priv->total_round++;

    // 单圈位置 (rad) [0, 2π)
    float pos_out = pos / ratio;
    d->position_single = fmodf(pos_out, 2.0f * (float)M_PI);
    if (d->position_single < 0.0f)
        d->position_single += 2.0f * (float)M_PI;

    // 多圈位置 (rad)
    d->position_multi = (float)priv->total_round * 2.0f * (float)M_PI + pos_out;

    // 速度 (rad/s) — 输出轴
    d->speed = vel / ratio;

    // 力矩 (N·m) — 输出轴
    d->torque = tor;

    // 电流 (A) = 力矩 / 扭矩系数
    d->current = tor / base->torque_coef;

    // 温度 (°C)
    d->temperature     = (float)raw->raw_temperature_motor;
    d->temperature_mos = (float)raw->raw_temperature_mos;

    // 错误标志
    d->error_flags = DMErrorToFlags(raw->error_code);

    d->fresh = 1;
}

/**
 * @brief 发送 DM 控制命令
 * @note 帧格式：8 字节全 0xFF + 最后一字节为命令
 */
static void DMMotorSetMode(DMMotorInstance *motor, DMMotorCmd_e cmd)
{
    if (!motor || !motor->priv.can_inst)
        return;

    CANInstance *can = motor->priv.can_inst;
    memset(can->tx_buff, 0xFF, 8);
    can->tx_buff[7] = (uint8_t)cmd;
    CANTransmit(can, 1);
}

/**
 * @brief daemon 离线回调
 */
static void DMMotorLostCallback(void *owner)
{
    DMMotorInstance *motor = (DMMotorInstance *)owner;
    if (!motor)
        return;

    motor->priv.was_lost = 1;

    // 每 10 次回调重试一次使能
    motor->priv.lost_cnt++;
    if (motor->priv.lost_cnt >= 10)
    {
        DMMotorSetMode(motor, DM_CMD_MOTOR_MODE);
        motor->priv.lost_cnt = 0;
    }
}

/**
 * @brief DM 错误码 → 统一位掩码
 */
static uint16_t DMErrorToFlags(uint8_t err_code)
{
    switch (err_code)
    {
    case 0x01: return 0x0000; // 使能（正常）
    case 0x00: return 0x0001; // 失能
    case 0x08: return 0x0002; // 超压
    case 0x09: return 0x0004; // 欠压
    case 0x0A: return 0x0008; // 过流
    case 0x0B: return 0x0010; // MOS 过温
    case 0x0C: return 0x0020; // 线圈过温
    case 0x0D: return 0x0040; // 通讯丢失
    case 0x0E: return 0x0080; // 过载
    default:   return 0x8000; // 未知错误
    }
}

#endif // BSP_CAN_MODULE_ENABLED
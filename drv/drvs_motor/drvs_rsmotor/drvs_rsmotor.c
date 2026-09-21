/**
 * @file drvs_rsmotor.c
 * @brief 灵足时代 RS05 准直驱电机纯协议驱动实现
 * @author TRW
 * @date 2026-09-21
 *
 * @note 协议：CAN MIT 模式，8 字节帧，12 位跨字节位域（控制帧与 DM 逐字节相同）
 * @note 映射：无符号整数线性映射（不是有符号补码）
 * @note 职责边界见 drvs_rsmotor.h：只做字节 ↔ 物理量，不做闭环/滤波/方向/累加/零点
 */

#include "drvs_rsmotor.h"
#include "app_cfg.h"

#ifdef DRVS_RSMOTOR_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include <math.h>
#include <string.h>

/* RS05 出厂默认量程（cfg 传 0 时采用，需与灵足上位机一致） */
#define DRVS_RS_DEFAULT_POS_MAX 12.57f  // 位置范围 ±12.57 rad
#define DRVS_RS_DEFAULT_VEL_RANGE 50.0f // 速度范围 ±50 rad/s
#define DRVS_RS_DEFAULT_T_RANGE 5.5f    // 扭矩范围 ±5.5 Nm

/*============================================
 *              内部辅助函数（热路径优化）
 *
 * @note 使用预计算的 scale 因子，把浮点除法变成乘法：
 *       Cortex-M7 FPU 乘法 1 周期、除法约 14 周期
 *============================================*/

/**
 * @brief 无符号定点 → 浮点（每帧 3 次 / 电机）
 * @note  raw * scale + offset  (1 mul + 1 add)
 */
static inline float drvs_rs_uint_to_float(uint16_t uint_val, float scale, float offset)
{
    return (float)uint_val * scale + offset;
}

/**
 * @brief 浮点 → 无符号定点（每帧 1 次 / 电机）
 * @note  (val + range) * inv_scale  (1 add + 1 mul)
 */
static inline uint16_t drvs_rs_float_to_uint(float float_val, float inv_scale, float range)
{
    /* 输入保护：NaN/Inf 和限幅 */
    if (!isfinite(float_val))
        float_val = 0.0f;
    if (float_val < -range)
        float_val = -range;
    else if (float_val > range)
        float_val = range;

    return (uint16_t)((float_val + range) * inv_scale);
}

/*============================================
 *              CAN 接收回调（ISR）
 *
 * 就地解析，写进双缓冲里"当前 ISR 正在写"的那一份，写完翻索引。
 * 读者读的是另一份，所以永远拿到完整的一帧，不会被半截数据打断。
 *============================================*/

/**
 * @brief RS05 反馈帧回调（ISR 上下文）
 *
 * 反馈帧格式（8 字节）见 drvs_rsmotor.h
 */
static void DrvsRSMotorRxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (!can || !can->parent)
        return;

    DrvsRSMotor_s *inst = (DrvsRSMotor_s *)can->parent;
    const DrvsRSMotorFeedbackFrame_u *fb = (const DrvsRSMotorFeedbackFrame_u *)pack->data;
    const DrvsRSMotorProtocolMap_s *map = &inst->proto_map;

    /* 位域展开（位置是大端 16 位；速度/扭矩各 12 位跨字节；温度 12 位跨字节） */
    uint16_t raw_position = ((uint16_t)fb->parts.position_be >> 8) | ((uint16_t)fb->parts.position_be << 8);
    uint16_t raw_velocity = ((uint16_t)fb->parts.vel_hi << 4) | (fb->parts.vel_lo_and_torque_hi >> 4);
    uint16_t raw_torque = ((uint16_t)(fb->parts.vel_lo_and_torque_hi & 0x0F) << 8) | fb->parts.torque_lo;
    uint16_t raw_temperature = ((uint16_t)(fb->parts.status_and_temp_hi & 0x0F) << 8) | fb->parts.temp_lo;

    /* 写入当前 ISR 缓冲区 */
    DrvsRSMotorData_s *out = &inst->data[inst->data_idx];

    /* 定点 → 物理量 */
    out->position = drvs_rs_uint_to_float(raw_position, map->pos_to_float_scale, -map->p_max);
    out->speed = drvs_rs_uint_to_float(raw_velocity, map->vel_to_float_scale, -map->v_range);
    out->torque = drvs_rs_uint_to_float(raw_torque, map->t_to_float_scale, -map->t_range);
    out->temperature = (float)raw_temperature * 0.1f; // 原始值 = ℃×10

    /* 状态位 */
    out->mode_state = fb->parts.status_and_temp_hi >> 6;              // bits[7:6]
    out->fault = (uint8_t)((fb->parts.status_and_temp_hi >> 5) & 1u); // bit[5]
    out->warn = (uint8_t)((fb->parts.status_and_temp_hi >> 4) & 1u);  // bit[4]
    out->timestamp_us = DWT_GetTimeUs();

    /* flip 双缓冲索引：写完才翻，读者看不到半截帧 */
    inst->data_idx = (uint8_t)(!inst->data_idx);

    /* 喂狗 */
    if (inst->daemon)
        DaemonReload(inst->daemon);
}

/*============================================
 *              模式命令发送
 *============================================*/

void DrvsRSMotorSendCmd(DrvsRSMotor_s *inst, uint8_t cmd)
{
    if (!inst || !inst->can)
        return;

    /* 帧格式：前 7 字节 0xFF，第 8 字节命令码 */
    CAN_Pack_s pack = {.id = inst->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    memset(pack.data, 0xFF, 7);
    pack.data[7] = cmd;

    CANTransmit(inst->can, &pack, inst->timeout_ms, NULL, NULL);
}

/**
 * @brief 修改 ID 类命令（0xFA 改电机 ID / 0x01 改主机 ID）
 * @note  帧格式：前 6 字节 0xFF，Byte6 = 新 ID，Byte7 = 命令码
 * @note  帧 ID 用电机"当前" canid 发出（一对一定址）
 */
static void DrvsRSMotorSendChangeIdCmd(DrvsRSMotor_s *inst, uint16_t new_id, uint8_t cmd)
{
    if (!inst || !inst->can)
        return;

    CAN_Pack_s pack = {.id = inst->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    memset(pack.data, 0xFF, 6);
    pack.data[6] = (uint8_t)new_id;
    pack.data[7] = cmd;

    CANTransmit(inst->can, &pack, inst->timeout_ms, NULL, NULL);
}

void DrvsRSMotorChangeCanID(DrvsRSMotor_s *inst, uint16_t can_id)
{
    DrvsRSMotorSendChangeIdCmd(inst, can_id, DRVS_RS_CMD_CHANGE_CAN_ID);
}

void DrvsRSMotorChangeMasterCanID(DrvsRSMotor_s *inst, uint16_t can_id)
{
    DrvsRSMotorSendChangeIdCmd(inst, can_id, DRVS_RS_CMD_CHANGE_MASTER_ID);
}

/*============================================
 *              注册 / 配置
 *============================================*/

/**
 * @brief 注册 RS 电机实例（仅调用一次）
 * @note 只注册 CAN/Daemon 实例，不配置量程等参数（由 DrvsRSMotorConfig 负责）
 */
int8_t DrvsRSMotorRegister(DrvsRSMotor_s *inst)
{
    if (!inst)
        return -1;

    /* 防重复注册 */
    if (inst->can && inst->can->parent == inst)
        return -1;

    if (inst->can)
    {
        if (CANRegister(inst->can) != 0)
            return -1;
        inst->can->parent = inst;
    }

    if (inst->daemon)
        DaemonRegister(inst->daemon);

    return 0;
}

/**
 * @brief 配置 RS 电机实例（可重复调用）
 * @note 要求在 DrvsRSMotorRegister 之后调用
 */
int8_t DrvsRSMotorConfig(DrvsRSMotor_s *inst, const DrvsRSMotorConfig_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    /* CAN 滤波器：tx 逐帧指定 can_id；rx 收 master_id。经典帧 */
    if (inst->can)
    {
        inst->can_filter.mode = CAN_FILTER_MODE_LIST;
        inst->can_filter.id0 = cfg->master_id;
        inst->can_filter.id1 = CAN_ID_UNUSED;
        inst->can_filter.frame_type = CAN_STANDARD_DATA_FRAME;
        inst->can_filter.callback = DrvsRSMotorRxCallback;

        CAN_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .mode = CAN_FRAME_FORMAT_CLASSIC,
            .parent = inst, /* 必须：CANConfig 会覆盖 parent，不设则回调取 can->parent 失效 */
            .filters = &inst->can_filter,
            .filter_num = 1,
        };
        if (CANConfig(inst->can, &can_cfg) != 0)
            return -1;
    }

    /* 协议映射：量程 → 预计算 scale（传 0 取 RS05 出厂默认量程） */
    DrvsRSMotorProtocolMap_s *map = &inst->proto_map;

    map->p_max = (cfg->pos_max > 0.0f) ? cfg->pos_max : DRVS_RS_DEFAULT_POS_MAX;
    map->v_range = (cfg->vel_range > 0.0f) ? cfg->vel_range : DRVS_RS_DEFAULT_VEL_RANGE;
    map->t_range = (cfg->t_range > 0.0f) ? cfg->t_range : DRVS_RS_DEFAULT_T_RANGE;

    /* 防止后面两次除法除零（量程为 0 属于配置错误，退化为最小值） */
    if (map->v_range < 1e-6f)
        map->v_range = 1e-6f;
    if (map->t_range < 1e-6f)
        map->t_range = 1e-6f;

    map->pos_to_float_scale = (2.0f * map->p_max) / 65535.0f;
    map->vel_to_float_scale = (2.0f * map->v_range) / 4095.0f;
    map->vel_to_uint_scale = 4095.0f / (2.0f * map->v_range);
    map->t_to_float_scale = (2.0f * map->t_range) / 4095.0f;
    map->t_to_uint_scale = 4095.0f / (2.0f * map->t_range);

    /* 标识与超时 */
    inst->can_id = cfg->can_id;
    inst->master_id = cfg->master_id;
    inst->timeout_ms = cfg->timeout_ms;

    /* 状态清零（扭矩归零；双缓冲清零后 GetData 在收到首帧前自然返回全 0） */
    inst->ref_torque = 0.0f;
    memset(inst->data, 0, sizeof(inst->data));
    inst->data_idx = 0;

    /* daemon：只当通信看门狗用，不挂回调
     * （RS05 掉线会自动退出 MIT 模式，但"要不要重发 0xFC"是策略：模块已不持有使能状态，
     *   无从判断该不该重发；由 app 在 DaemonIsOnline 变化时自己调 SendCmd 决定） */
    if (inst->daemon)
    {
        Daemon_Config_s daemon_cfg = {
            .callback = NULL,
            .fault_action = cfg->fault_action,
            .owner_id = inst,
            .reload_count = cfg->reload_count,
        };
        DaemonConfig(inst->daemon, &daemon_cfg);
    }

    return 0;
}

/*============================================
 *              扭矩设定 / 发送
 *============================================*/

void DrvsRSMotorSetRef(DrvsRSMotor_s *inst, float torque)
{
    if (!inst)
        return;
    inst->ref_torque = torque;
}

void DrvsRSMotorSend(DrvsRSMotor_s *inst)
{
    if (!inst || !inst->can)
        return;

    const DrvsRSMotorProtocolMap_s *map = &inst->proto_map;

    /* 扭矩限幅（drvs_rs_float_to_uint 内部也会按 t_range 夹一次） */
    uint16_t t_ff = drvs_rs_float_to_uint(inst->ref_torque, map->t_to_uint_scale, map->t_range);

    /* 打包：板载 PD 不用，p_des/v_des/Kp/Kd 恒为 0，只有 t_ff 有效 */
    CAN_Pack_s pack = {.id = inst->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    DrvsRSMotorControlFrame_u *cf = (DrvsRSMotorControlFrame_u *)pack.data;
    cf->parts.p_des_be = 0;
    cf->parts.v_des_hi = 0;
    cf->parts.v_des_lo_and_kp_hi = 0;
    cf->parts.kp_lo = 0;
    cf->parts.kd_hi = 0;
    cf->parts.kd_lo_and_tff_hi = (uint8_t)((t_ff >> 8) & 0x0F);
    cf->parts.tff_lo = (uint8_t)(t_ff & 0xFF);

    CANTransmit(inst->can, &pack, inst->timeout_ms, NULL, NULL);
}

/*============================================
 *              反馈获取
 *============================================*/

DrvsRSMotorData_s DrvsRSMotorGetData(const DrvsRSMotor_s *inst)
{
    DrvsRSMotorData_s zero = {0};
    if (!inst)
        return zero;

    /* ISR 写的是 data[data_idx]，就绪的是另一个。
     * 一帧都没收到时 data[] 仍是 Config 清零后的全 0，故无需 data_valid 标志。
     * 需要判"是否收到过"看 timestamp_us == 0（真实帧的时间戳不会为 0）。 */
    return inst->data[!inst->data_idx];
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRVS_RSMOTOR_USED */

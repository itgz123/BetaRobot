/**
 * @file drvs_lkmotor.c
 * @brief LK（瓴控/翎控）MF 系列一体化直驱电机纯协议驱动实现（一对一）
 * @author TRW
 * @date 2026-09-21
 *
 * @note 协议：一对一 CAN 总线通讯协议 V2.36，标准帧 11 位 ID，默认 1Mbps，8 字节帧。
 *       CAN ID = 0x140 + 电机ID(1~32)，命令与回复同 ID，无应用层 CRC。
 * @note 控制：0xA1 转矩闭环下发 iq；电机对 0xA1 的回复就是「状态2」格式，无需轮询。
 * @note 职责边界见 drvs_lkmotor.h：只做字节 ↔ 物理量，不做闭环/滤波/方向/累加/零点
 */

#include "drvs_lkmotor.h"
#include "app_cfg.h"

#ifdef DRVS_LKMOTOR_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include <math.h>
#include <string.h>

/*============================================
 *              协议常量
 *
 * @note iq 分辨率与编码器量纲对全系列 MF 固定，直接常量化；
 *       只有随型号变的 Kt 走 cfg。
 *============================================*/
#define DRVS_LK_CAN_ID_BASE 0x140u // CAN_ID = 0x140 + 电机ID
#define DRVS_LK_MOTOR_ID_MIN 1u    // 电机 ID 下限
#define DRVS_LK_MOTOR_ID_MAX 32u   // 电机 ID 上限

/* 0xA1 iqControl 限幅（一对一 ±2048，广播帧为 ±2000） */
#define DRVS_LK_IQ_RAW_MAX 2048.0f

/* MF 固定：A = raw × 33/4096（MG 为 66/4096，本驱动不支持） */
#define DRVS_LK_IQ_A_PER_LSB (33.0f / 4096.0f)

/* 编码器：整圈 raw 覆盖 0~65535（转一圈恰好回绕一次），固定按 16bit 处理 */
#define DRVS_LK_ENCODER_TO_RAD (6.283185307179586f / 65536.0f) // 2π / 65536
#define DRVS_LK_DPS_TO_RADPS 0.017453292519943295f             // π / 180

/*============================================
 *              内部辅助函数
 *============================================*/

/**
 * @brief 判断回显字节是否为「状态2 格式」的回复
 * @note  只有控制命令 0xA0~0xA8 与 0x9C 的回复是状态2 格式。
 *        本模块只发 0xA1，调试可发 0x9C；其余（0x80/0x88/0x81/0x9B 等）
 *        或为全零回显、或为其他帧格式，必须排除。
 */
static inline uint8_t DrvsLKMotorIsStatusEcho(uint8_t echo)
{
    return (echo == DRVS_LK_CMD_TORQUE || echo == DRVS_LK_CMD_READ_STATUS2) ? 1u : 0u;
}

/*============================================
 *              CAN 接收回调（ISR）
 *
 * 先校验回显，再就地解析写进双缓冲里"当前 ISR 正在写"的那一份，写完翻索引。
 * 读者读的是另一份，所以永远拿到完整的一帧，不会被半截数据打断。
 *============================================*/

/**
 * @brief LK 状态2 反馈帧回调（ISR 上下文）
 *
 * 帧格式（8 字节，小端）见 drvs_lkmotor.h
 */
static void DrvsLKMotorRxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (!can || !can->parent)
        return;

    DrvsLKMotor_s *inst = (DrvsLKMotor_s *)can->parent;
    const uint8_t echo = pack->data[0];

    /* 喂狗：只要能收到该 ID 的帧就说明通信在线（含全零回显帧） */
    if (inst->daemon)
        DaemonReload(inst->daemon);

    /* 回显校验（关键）：全零回显帧直接丢弃，不写双缓冲，
     * 否则会把位置/速度误解析为 0 造成跳变。丢弃后 GetData 仍返回上一帧。 */
    if (!DrvsLKMotorIsStatusEcho(echo))
        return;

    const DrvsLKMotorStatusFrame_u *fb = (const DrvsLKMotorStatusFrame_u *)pack->data;
    const DrvsLKMotorProtocolMap_s *map = &inst->proto_map;

    /* 整字节小端量直接取（无跨字节位域，与 DM/RS 的移位展开不同） */
    int8_t raw_temp = fb->parts.temperature;
    int16_t raw_iq = fb->parts.iq_le;
    int16_t raw_speed = fb->parts.speed_dps_le;
    uint16_t raw_encoder = fb->parts.encoder_le;

    /* 写入当前 ISR 缓冲区 */
    DrvsLKMotorData_s *out = &inst->data[inst->data_idx];

    /* 定点 → 物理量 */
    out->position = (float)raw_encoder * map->encoder_to_rad; // 单圈 [0, 2π)
    out->speed = (float)raw_speed * map->dps_to_radps;
    out->current = (float)raw_iq * map->a_per_lsb;
    out->torque = (float)raw_iq * map->nm_per_lsb;
    out->temperature = raw_temp;
    out->echo = echo;
    out->timestamp_us = DWT_GetTimeUs();

    /* flip 双缓冲索引：写完才翻，读者看不到半截帧 */
    inst->data_idx = (uint8_t)(!inst->data_idx);
}

/*============================================
 *              模式命令发送
 *============================================*/

void DrvsLKMotorSendCmd(DrvsLKMotor_s *inst, uint8_t cmd)
{
    if (!inst || !inst->can)
        return;

    /* 帧格式：DATA[0]=命令码，其余字节 0x00 */
    CAN_Pack_s pack = {.id = inst->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    memset(pack.data, 0x00, 8);
    pack.data[0] = cmd;

    CANTransmit(inst->can, &pack, inst->timeout_ms, NULL, NULL);
}

/*============================================
 *              注册 / 配置
 *============================================*/

/**
 * @brief 注册 LK 电机实例（仅调用一次）
 * @note 只注册 CAN/Daemon 实例，不配置电机参数（由 DrvsLKMotorConfig 负责）
 */
int8_t DrvsLKMotorRegister(DrvsLKMotor_s *inst)
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
 * @brief 配置 LK 电机实例（可重复调用）
 * @note 要求在 DrvsLKMotorRegister 之后调用
 */
int8_t DrvsLKMotorConfig(DrvsLKMotor_s *inst, const DrvsLKMotorConfig_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    /* 参数校验 */
    if (cfg->motor_id < DRVS_LK_MOTOR_ID_MIN || cfg->motor_id > DRVS_LK_MOTOR_ID_MAX)
        return -1;
    if (cfg->torque_constant <= 0.0f)
        return -1;

    uint16_t can_id = (uint16_t)(DRVS_LK_CAN_ID_BASE + cfg->motor_id);

    /* CAN 滤波器：一对一，tx 与 rx 同为 0x140 + motor_id。经典帧 */
    if (inst->can)
    {
        inst->can_filter.mode = CAN_FILTER_MODE_LIST;
        inst->can_filter.id0 = can_id;
        inst->can_filter.id1 = CAN_ID_UNUSED;
        inst->can_filter.frame_type = CAN_STANDARD_DATA_FRAME;
        inst->can_filter.callback = DrvsLKMotorRxCallback;

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

    /* 协议映射：预计算 raw ↔ Nm / rad 的换算因子 */
    DrvsLKMotorProtocolMap_s *map = &inst->proto_map;

    map->torque_constant = cfg->torque_constant;
    map->a_per_lsb = DRVS_LK_IQ_A_PER_LSB;
    map->nm_per_lsb = cfg->torque_constant * DRVS_LK_IQ_A_PER_LSB;
    map->inv_nm_per_lsb = 1.0f / map->nm_per_lsb;
    map->encoder_to_rad = DRVS_LK_ENCODER_TO_RAD;
    map->dps_to_radps = DRVS_LK_DPS_TO_RADPS;

    /* 标识与超时 */
    inst->motor_id = cfg->motor_id;
    inst->can_id = can_id;
    inst->timeout_ms = cfg->timeout_ms;

    /* 状态清零（扭矩归零；双缓冲清零后 GetData 在收到首帧前自然返回全 0） */
    inst->ref_torque = 0.0f;
    memset(inst->data, 0, sizeof(inst->data));
    inst->data_idx = 0;

    /* daemon：只当通信看门狗用，不挂回调
     * （模块已不持有使能状态，无从判断该不该重发 0x88；由 app 在 DaemonIsOnline
     *   变化时自己调 SendCmd 决定。注意 reload_count = 0 时 DaemonIsOnline 恒报离线） */
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

void DrvsLKMotorSetRef(DrvsLKMotor_s *inst, float torque)
{
    if (!inst)
        return;
    inst->ref_torque = torque;
}

void DrvsLKMotorSend(DrvsLKMotor_s *inst)
{
    if (!inst || !inst->can)
        return;

    const DrvsLKMotorProtocolMap_s *map = &inst->proto_map;

    /* Nm → iq raw，限到协议 ±2048（即满量程 33/4096×2048 = 16.5A，无需再单独按 Nm 限扭矩） */
    float iq_f = inst->ref_torque * map->inv_nm_per_lsb;
    if (!isfinite(iq_f))
        iq_f = 0.0f;
    if (iq_f < -DRVS_LK_IQ_RAW_MAX)
        iq_f = -DRVS_LK_IQ_RAW_MAX;
    else if (iq_f > DRVS_LK_IQ_RAW_MAX)
        iq_f = DRVS_LK_IQ_RAW_MAX;

    int16_t iq_raw = (int16_t)(iq_f >= 0.0f ? iq_f + 0.5f : iq_f - 0.5f);

    /* 打包：只有 D[0] 命令字和 D[4..5] iq（小端）有效，其余为 0 */
    CAN_Pack_s pack = {.id = inst->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    DrvsLKMotorControlFrame_u *cf = (DrvsLKMotorControlFrame_u *)pack.data;
    cf->parts.cmd = DRVS_LK_CMD_TORQUE;
    cf->parts._reserved0 = 0;
    cf->parts._reserved1 = 0;
    cf->parts._reserved2 = 0;
    cf->parts.iq_le = iq_raw;
    cf->parts._reserved3 = 0;
    cf->parts._reserved4 = 0;

    CANTransmit(inst->can, &pack, inst->timeout_ms, NULL, NULL);
}

/*============================================
 *              反馈获取
 *============================================*/

DrvsLKMotorData_s DrvsLKMotorGetData(const DrvsLKMotor_s *inst)
{
    DrvsLKMotorData_s zero = {0};
    if (!inst)
        return zero;

    /* ISR 写的是 data[data_idx]，就绪的是另一个。
     * 一帧都没收到时 data[] 仍是 Config 清零后的全 0，故无需 data_valid 标志。
     * 需要判"是否收到过"看 timestamp_us == 0（真实帧的时间戳不会为 0）。 */
    return inst->data[!inst->data_idx];
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRVS_LKMOTOR_USED */

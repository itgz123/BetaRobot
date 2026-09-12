/**
 * @file drv_lkmotor.c
 * @brief LK（瓴控/翎控）MF 系列一体化直驱电机驱动实现
 *
 * @note 协议：一对一 CAN 总线通讯协议 V2.36，标准帧 11 位 ID，默认 1Mbps，8 字节帧。
 *       CAN ID = 0x140 + 电机ID(1~32)，命令与回复同 ID，无应用层 CRC。
 *
 * @note 控制：MCU 端 PID 级联，通过 0xA1 下发 iq（int16，-2048~2048）。
 *       电机对 0xA1 的回复与「读取状态2（0x9C）」同格式，仅 DATA[0] 回显命令字。
 *
 * @note ⚠️ 0x80/0x88/0x81 的回复是「与主机发送相同」的全零回显，不是状态2格式；
 *       GetData 只接受回显为 0xA1/0x9C 的帧，避免全零帧污染位置/速度。
 *
 * @note MF 为直驱（无减速机），反馈与下发均为电机轴量，严禁乘减速比。
 */

#include "drv_lkmotor.h"
#include "app_cfg.h"

#ifdef DRV_LKMOTOR_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "lib_math.h"
#include <string.h>

/*============================================
 *              协议常量
 *============================================*/
#define LK_CAN_ID_BASE 0x140u                 // 命令与回复标识符基址：CAN_ID = 0x140 + 电机ID
#define LK_MOTOR_ID_MIN 1u                    // 电机 ID 下限
#define LK_MOTOR_ID_MAX 32u                   // 电机 ID 上限
#define LK_IQ_RAW_MAX 2048.0f                 // 0xA1 iqControl 限幅（一对一 ±2048，广播帧为 ±2000）
#define LK_DEG_TO_RADPS 0.017453292519943295f // π/180，dps → rad/s

#define LK_IQ_A_PER_LSB (33.0f / 4096.0f) // MF 固定：A = raw × 33/4096（MG 为 66/4096，本驱动不支持）

/* 编码器：实测整圈 raw 覆盖 0~65535（转一圈恰好回绕一次），固定按 16bit 处理 */
#define LK_ENCODER_TO_RAD (M_2PI / 65536.0f) // raw → rad
#define LK_WRAP_SPAN_RAD M_2PI               // 线值回绕一次的机械角跨度 = 一圈
#define LK_INV_WRAP_SPAN INV_M_2PI           // 多圈穿越检测用

/*============================================
 *              虚函数表实例
 *
 * @note  必须放在文件顶部（Register 函数中使用）
 *============================================*/
void LKMotor_Enable(void *inst);
void LKMotor_Disable(void *inst);
void LKMotor_SetRef(void *inst, float ref);
void LKMotor_Send(void *inst);
MotorData_s LKMotor_GetData(void *inst);

const static MotorVTable_s s_lk_motor_vtable = {
    .enable = LKMotor_Enable,
    .disable = LKMotor_Disable,
    .set_ref = LKMotor_SetRef,
    .send = LKMotor_Send,
    .get_data = LKMotor_GetData,
    .send_cmd = LKMotor_SendModeCmd,
};

/*============================================
 *              内部辅助函数
 *============================================*/

/**
 * @brief 判断回显字节是否为「状态2格式」的回复
 * @note  只有控制命令 0xA0~0xA8 与 0x9C 的回复是状态2格式。
 *        首版只发送 0xA1，调试可发 0x9C；其余（0x80/0x88/0x81/0x9B 等）
 *        或为全零回显、或为其他帧格式，必须排除。
 */
static inline uint8_t lk_is_status_frame(uint8_t echo)
{
    return (echo == LK_CMD_TORQUE || echo == LK_CMD_READ_STATUS2) ? 1u : 0u;
}

/*============================================
 *              模式命令发送
 *============================================*/
/**
 * @brief 发送 LK 电机模式命令
 * @param inst LK 电机实例
 * @param cmd  命令码（LKMotorModeCmd_e）
 * @note  帧格式：DATA[0]=命令码，其余字节 0x00
 */
void LKMotor_SendModeCmd(void *inst, uint8_t cmd)
{
    if (!inst)
        return;
    LKMotorInstance *motor = (LKMotorInstance *)inst;

    if (!motor->base.can)
        return;

    CAN_Pack_s pack = {.id = (uint16_t)(LK_CAN_ID_BASE + motor->motor_id),
                       .frame_type = CAN_STANDARD_DATA_FRAME,
                       .len = 8};
    memset(pack.data, 0x00, 8);
    pack.data[0] = cmd;
    CANTransmit(motor->base.can, &pack, motor->base.timeout_ms, NULL, NULL);
}

/*============================================
 *              CAN 接收回调（最小化 ISR 工作）
 *
 * 仅做：memcpy 原始 8 字节 + 记录时间戳 + flip 双缓冲 + 喂狗。
 * 回显校验与数据处理全部放到 LKMotor_GetData。
 *============================================*/
static void LKMotorRxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (!can || !can->parent)
        return;

    LKMotorInstance *motor = (LKMotorInstance *)can->parent;

    /* 写入当前 ISR 缓冲区 */
    uint8_t idx = motor->base.raw_frame_idx;
    memcpy(motor->base.raw_frames[idx].bytes, pack->data, 8);
    motor->base.raw_frames[idx].timestamp_us = DWT_GetTimeUs();

    /* flip 双缓冲索引 */
    motor->base.raw_frame_idx ^= 1u;

    /* 缓存失效 —— 下次 GetData 需重新处理 */
    motor->base.data_valid = 0;

    /* 喂狗 */
    if (motor->base.daemon)
    {
        DaemonReload(motor->base.daemon);
    }
}

/*============================================
 *              统一数据获取接口 (LKMotor_GetData)
 *
 * 处理链：回显校验 → 解析 → SI 转换 → 多圈累加 → 速度计算 → 滤波 → 偏置/方向
 *============================================*/
MotorData_s LKMotor_GetData(void *inst)
{
    MotorData_s result = {0};
    if (!inst)
        return result;

    LKMotorInstance *motor = (LKMotorInstance *)inst;
    MotorBase_s *base = &motor->base;

    /* 缓存命中：上次获取后没有新中断，直接返回缓存 */
    if (base->data_valid)
        return base->data_all.data;

    if (base->model >= LK_MODEL_NUM)
        return result;

    /* ====== Step 1: 从双缓冲读取就绪帧 ====== */
    uint8_t ready_idx = !base->raw_frame_idx;
    MotorRawFrame_s frame = base->raw_frames[ready_idx];

    /* ====== Step 2: 回显校验（关键） ====== */
    /* 全零回显帧（0x80/0x88/0x81）或非状态2格式帧直接丢弃，返回旧缓存，
     * 否则会把位置/速度误解析为 0 造成跳变。标记 data_valid=1 避免反复重解析同一脏帧。 */
    if (!lk_is_status_frame(frame.bytes[0]))
    {
        base->data_valid = 1;
        return base->data_all.data;
    }

    /* ====== Step 3: 解析位域 + SI 转换（小端） ====== */
    const LKMotorProtocolMap_s *map = &motor->proto_map;
    MotorControllerSetting_s *setting = &base->setting;

    int8_t raw_temp = (int8_t)frame.bytes[1];
    int16_t raw_iq = (int16_t)((uint16_t)frame.bytes[2] | ((uint16_t)frame.bytes[3] << 8));
    int16_t raw_speed = (int16_t)((uint16_t)frame.bytes[4] | ((uint16_t)frame.bytes[5] << 8));
    uint16_t raw_encoder = (uint16_t)((uint16_t)frame.bytes[6] | ((uint16_t)frame.bytes[7] << 8));

    // 单圈位置 (rad) [0, 2π)
    float position_single = (float)raw_encoder * LK_ENCODER_TO_RAD;
    // 转速 (rad/s)，1dps/LSB
    float velocity_raw = (float)raw_speed * LK_DEG_TO_RADPS;
    // 转矩电流 (A) 与转矩 (Nm)
    float iq_A = (float)raw_iq * LK_IQ_A_PER_LSB;
    float torque = iq_A * map->torque_constant;

    /* ====== Step 4: dt 计算 ====== */
    float dt = 0.0f;
    if (base->data_all.timestamp_last_us > 0 && frame.timestamp_us > base->data_all.timestamp_last_us)
    {
        dt = (float)(frame.timestamp_us - base->data_all.timestamp_last_us) * 1e-6f;
    }

    /* ====== Step 5: 多圈位置（累加→偏置→方向→归一化，顺序不可改） ====== */
    int64_t wraps = 0;
    if (base->data_all.timestamp_last_us > 0)
    {
        float angle_diff = position_single - base->data_all.position_single_last;
        if (dt > 0.0f)
        {
            float expected_change = velocity_raw * dt;
            float wrap_float = (expected_change - angle_diff) * LK_INV_WRAP_SPAN;
            wraps = (int64_t)(wrap_float > 0.0f ? wrap_float + 0.5f : wrap_float - 0.5f);
        }
        else
        {
            float half_span = (float)M_PI;
            if (angle_diff > half_span)
                wraps = -1;
            else if (angle_diff < -half_span)
                wraps = 1;
        }
    }

    base->data_all.position_cnt += wraps;                                                                     // ① 累加（单位 = 线值回绕次数）
    double angle = ((double)base->data_all.position_cnt * (double)LK_WRAP_SPAN_RAD) + (double)position_single // ① 累加
                   + (double)base->position_offset;                                                           // ② 偏置

    angle *= setting->feedback_direction; // ③ 方向

    if (setting->position_mode == MOTOR_POSITION_WRAP) // ④ 归一化
    {
        angle = Lib_Math_WrapAngle(angle, setting->angle_limit_min, setting->angle_limit_max);
    }

    result.position = angle;

    /* ====== Step 6: 速度计算，滤波，方向修正 ====== */
    float speed;
    if (base->speed_lpf_enable == MOTOR_SPEED_LPF_ENABLE && dt > 0.0f)
    {
        float alpha = dt / (base->speed_lpf_rc + dt);
        speed = velocity_raw * alpha + base->data_all.speed_last * (1.0f - alpha);
        base->data_all.speed_last = speed; /* 更新 LPF 状态 */
    }
    else
    {
        speed = velocity_raw;
    }
    result.speed = speed * setting->feedback_direction;

    /* ====== Step 7: 扭矩方向修正 ====== */
    result.torque = torque * setting->feedback_direction;

    /* ====== Step 8: 其他数据 + 缓存 ====== */
    result.timestamp_us = frame.timestamp_us;
    motor->temperature = raw_temp;
    motor->iq_A = iq_A;

    base->data_all.position_single_last = position_single;
    base->data_all.timestamp_last_us = frame.timestamp_us;
    base->data_all.position_single = position_single;
    base->data_all.data = result;
    base->data_valid = 1;

    return result;
}

/*============================================
 *              Daemon 回调
 *============================================*/
/**
 * @brief LK 电机离线回调
 * @param owner 守护进程所有者 (LKMotorInstance*)
 * @note  复位 PID；若处于使能状态则重发 0x88 恢复开启。
 */
static void LKMotorDaemonCallback(void *owner)
{
    if (!owner)
        return;

    LKMotorInstance *motor = (LKMotorInstance *)owner;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);

    /* 重发开启命令，恢复受控状态 */
    if (motor->base.enable)
    {
        LKMotor_SendModeCmd(motor, LK_CMD_ENABLE);
    }
}

/*============================================
 *              注册函数
 *============================================*/

/**
 * @brief 注册 LK 电机实例（仅调用一次）
 * @note 只注册 CAN/Daemon 实例，不配置电机参数（由 LKMotorConfig 负责）。
 */
int8_t LKMotorRegister(LKMotorInstance *inst)
{
    if (!inst)
        return -1;

    // 防重复注册检查
    if (inst->base.can && inst->base.can->parent == inst)
        return -1;

    // 注册 CAN 实例（仅绑定 CAN 外设，滤波器/callback 由 Config 设置）
    if (inst->base.can)
    {
        if (CANRegister(inst->base.can) != 0)
            return -1;
        inst->base.can->parent = inst;
    }

    // 初始化基本属性
    inst->base.brand = MOTOR_BRAND_LK;
    inst->base.enable = MOTOR_DISABLE;
    inst->base.vtable = &s_lk_motor_vtable;

    // 注册 daemon（占位，Config 更新运行参数）
    if (inst->base.daemon)
    {
        DaemonRegister(inst->base.daemon);
    }

    return 0;
}

/**
 * @brief 配置 LK 电机实例（可重复调用）
 * @note 配置协议映射、PID、CAN 滤波器、daemon 等，要求在 LKMotorRegister 之后调用。
 * @note iq 分辨率与编码器量纲对全系列 MF 固定（见文件顶部常量），Config 只暴露随型号变的 torque_constant。
 */
int8_t LKMotorConfig(LKMotorInstance *inst, LKMotor_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    /* 参数校验：首版仅支持 MF */
    if (cfg->model >= LK_MODEL_NUM || cfg->model != LK_MODEL_MF)
        return -1;
    if (cfg->motor_id < LK_MOTOR_ID_MIN || cfg->motor_id > LK_MOTOR_ID_MAX)
        return -1;
    if (cfg->torque_constant <= 0.0f)
        return -1;

    uint16_t can_id = (uint16_t)(LK_CAN_ID_BASE + cfg->motor_id);

    /* 配置 CAN 滤波器（收发同 ID = 0x140 + motor_id，工作模式 CLASSIC） */
    if (inst->base.can)
    {
        inst->base.can_filter.mode = CAN_FILTER_MODE_LIST;
        inst->base.can_filter.id0 = can_id;
        inst->base.can_filter.id1 = CAN_ID_UNUSED;
        inst->base.can_filter.frame_type = CAN_STANDARD_DATA_FRAME;
        inst->base.can_filter.callback = LKMotorRxCallback;

        CAN_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .mode = CAN_FRAME_FORMAT_CLASSIC,
            .parent = inst, /* 必须：CANConfig 会覆盖 parent，不设则回调取 can->parent 失效 */
            .filters = &inst->base.can_filter,
            .filter_num = 1,
        };
        if (CANConfig(inst->base.can, &can_cfg) != 0)
            return -1;
    }

    /* --- 协议映射：只保留随型号变的 Kt，其余量纲见文件顶部常量 --- */
    LKMotorProtocolMap_s *map = &inst->proto_map;

    map->torque_constant = cfg->torque_constant;
    map->inv_torque_constant = 1.0f / cfg->torque_constant;

    /* 基本属性 */
    inst->base.model = cfg->model;
    inst->motor_id = cfg->motor_id;
    inst->base.timeout_ms = cfg->timeout_ms;
    inst->base.position_offset = cfg->position_offset;
    inst->motor_state = LK_STATE_DISABLED;

    /* 控制器设置 */
    inst->base.setting = cfg->controller_setting;

    /* 控制器状态 */
    inst->base.controller.ref = 0.0f;
    inst->base.controller.output = 0.0f;

    /* 初始化速度环 PID */
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_SPEED)
    {
        cfg->pid_speed_setting.config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL;
        PIDInit(&inst->base.controller.pid_speed, &cfg->pid_speed_setting);
    }

    /* 初始化位置环 PID */
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_ANGLE)
    {
        cfg->pid_angle_setting.config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL;

        // 环绕模式：自动启用位置环误差归一化
        if (cfg->controller_setting.position_mode == MOTOR_POSITION_WRAP)
        {
            cfg->pid_angle_setting.error_normalize_range =
                cfg->controller_setting.angle_limit_max - cfg->controller_setting.angle_limit_min;
            cfg->pid_angle_setting.config_mask |= PID_ENABLE_ERROR_NORMALIZE;
        }

        PIDInit(&inst->base.controller.pid_angle, &cfg->pid_angle_setting);
    }

    inst->base.speed_lpf_enable = cfg->speed_lpf_enable;
    inst->base.speed_lpf_rc = cfg->speed_lpf_rc;

    /* 双缓冲清零 */
    memset(inst->base.raw_frames, 0, sizeof(inst->base.raw_frames));
    inst->base.raw_frame_idx = 0;
    inst->base.data_valid = 0;

    /* 处理状态清零 */
    memset(&inst->base.data_all, 0, sizeof(MotorDataAll_s));

    /* 更新 daemon 运行参数（可重入） */
    if (inst->base.daemon)
    {
        Daemon_Config_s daemon_cfg = {
            .callback = LKMotorDaemonCallback,
            .fault_action = cfg->fault_action,
            .owner_id = inst,
            .reload_count = cfg->reload_count,
        };
        DaemonConfig(inst->base.daemon, &daemon_cfg);
    }

    return 0;
}

/*============================================
 *              PID 级联计算
 *============================================*/

/**
 * @brief 单个 LK 电机 PID 级联计算
 * @param inst LK 电机实例
 */
static void LKMotor_Calculate(LKMotorInstance *inst)
{
    if (!inst || !inst->base.enable)
        return;

    MotorControllerSetting_s *setting = &inst->base.setting;
    MotorController_s *ctrl = &inst->base.controller;

    float setpoint = ctrl->ref;
    float measure;
    float output = 0.0f;

    if (inst->base.model >= LK_MODEL_NUM)
        return;

    /* 统一获取一次反馈数据（后续 GetData 走缓存） */
    MotorData_s md = LKMotor_GetData(inst);

    // 位置环 (最外环)
    if (setting->loop_type & MOTOR_LOOP_ANGLE)
    {
        switch (setting->position_mode)
        {
        case MOTOR_POSITION_LIMITED:
            if (setting->angle_limit_min < setting->angle_limit_max)
            {
                setpoint = Lib_Math_Clamp(setpoint, setting->angle_limit_min, setting->angle_limit_max);
            }
            break;
        case MOTOR_POSITION_WRAP:
            setpoint = Lib_Math_WrapAngle(setpoint, setting->angle_limit_min, setting->angle_limit_max);
            break;
        case MOTOR_POSITION_CONTINUOUS:
        default:
            break;
        }

        float position_feedforward = 0.0f;
        if (setting->position_feedforward_src == MOTOR_FEEDFORWARD_EXTERNAL && setting->position_feedforward_ptr)
        {
            position_feedforward = *setting->position_feedforward_ptr;
        }
        measure = (setting->angle_src == MOTOR_FEEDBACK_EXTERNAL && setting->angle_external_ptr) ? *setting->angle_external_ptr : md.position;
        setpoint = PIDCalculate(&ctrl->pid_angle, setpoint, measure, position_feedforward);
    }

    // 速度环
    if (setting->loop_type & MOTOR_LOOP_SPEED)
    {
        float speed_feedforward = 0.0f;
        if (setting->speed_feedforward_src == MOTOR_FEEDFORWARD_EXTERNAL && setting->speed_feedforward_ptr)
        {
            speed_feedforward = *setting->speed_feedforward_ptr;
        }
        measure = (setting->speed_src == MOTOR_FEEDBACK_EXTERNAL && setting->speed_external_ptr) ? *setting->speed_external_ptr : md.speed;
        output = PIDCalculate(&ctrl->pid_speed, setpoint, measure, speed_feedforward);
    }
    else
    {
        // 开环: setpoint 直接作为扭矩；仅位置环: setpoint 为位置环 PID 输出（扭矩 Nm）
        output = setpoint;
    }

    // 电机方向修正: motor_direction 修正安装方向, feedback_direction 已在反馈端修正
    output *= setting->motor_direction;

    ctrl->output = output;
}

/*============================================
 *              虚函数实现
 *============================================*/

void LKMotor_Enable(void *inst)
{
    if (!inst)
        return;
    LKMotorInstance *motor = (LKMotorInstance *)inst;

    /* 先发送开启命令，再设置标志 */
    LKMotor_SendModeCmd(motor, LK_CMD_ENABLE);
    motor->base.enable = MOTOR_ENABLE;
    motor->motor_state = LK_STATE_ENABLED;
}

void LKMotor_Disable(void *inst)
{
    if (!inst)
        return;
    LKMotorInstance *motor = (LKMotorInstance *)inst;

    /* 发送关闭命令 + 清除使能标志 + 复位 PID
     * 注：0x80 清的是电机内部圈数；驱动的多圈为自有单圈绝对值软件累加，二者无关，
     *     故无需重置 base.data_all.position_cnt。 */
    LKMotor_SendModeCmd(motor, LK_CMD_DISABLE);
    motor->base.enable = MOTOR_DISABLE;
    motor->motor_state = LK_STATE_DISABLED;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);
}

/**
 * @brief 设置电机控制参考值
 * @param inst LK 电机实例
 * @param ref  参考值
 *
 * 方向标定流程:
 *   1. 开环下发很小的正扭矩，观察实际转向与反馈符号
 *   2. 实际转向相反 → motor_direction = MOTOR_DIRECTION_REVERSE
 *   3. 反馈符号相反 → feedback_direction = MOTOR_DIRECTION_REVERSE
 *
 * 控制模式:
 *   - MOTOR_LOOP_OPEN（扭矩开环）：ref = 扭矩(Nm)
 *   - MOTOR_LOOP_SPEED（速度环）：ref = 速度(rad/s) → PID(扭矩)
 *   - MOTOR_LOOP_ANGLE（位置环）：ref = 位置(rad) → PID(扭矩)
 *   - MOTOR_LOOP_ANGLE | MOTOR_LOOP_SPEED（位置-速度双环）
 */
void LKMotor_SetRef(void *inst, float ref)
{
    if (!inst)
        return;
    LKMotorInstance *motor = (LKMotorInstance *)inst;
    motor->base.controller.ref = ref;
}

/**
 * @brief 发送 LK 转矩闭环控制帧（0xA1）
 * @param inst LK 电机实例
 *
 * 帧格式（8字节）：
 *   D[0]=0xA1, D[1..3]=0x00, D[4..5]=iqControl int16 小端, D[6..7]=0x00
 *
 * @note 本项目在 MCU 端做 PID，输出扭矩换算为 iq 下发；失能时不发送。
 */
void LKMotor_Send(void *inst)
{
    if (!inst)
        return;
    LKMotorInstance *motor = (LKMotorInstance *)inst;

    if (!motor->base.can)
        return;

    /* 失能时不发送控制帧 */
    if (!motor->base.enable)
        return;

    /* 控制计算 */
    LKMotor_Calculate(motor);

    const LKMotorProtocolMap_s *map = &motor->proto_map;

    /* Nm → A → iq raw，限到协议 ±2048（即满量程 33/4096×2048 = 16.5A，无需再单独按 Nm 限扭矩） */
    float iq_f = motor->base.controller.output * map->inv_torque_constant / LK_IQ_A_PER_LSB;
    if (!isfinite(iq_f))
        iq_f = 0.0f;
    iq_f = Lib_Math_Clamp(iq_f, -LK_IQ_RAW_MAX, LK_IQ_RAW_MAX);
    int16_t iq_raw = (int16_t)(iq_f >= 0.0f ? iq_f + 0.5f : iq_f - 0.5f);

    CAN_Pack_s pack = {.id = (uint16_t)(LK_CAN_ID_BASE + motor->motor_id),
                       .frame_type = CAN_STANDARD_DATA_FRAME,
                       .len = 8};
    pack.data[0] = LK_CMD_TORQUE; // 0xA1
    pack.data[1] = 0x00;
    pack.data[2] = 0x00;
    pack.data[3] = 0x00;
    pack.data[4] = (uint8_t)((uint16_t)iq_raw & 0xFF); // 小端低字节
    pack.data[5] = (uint8_t)((uint16_t)iq_raw >> 8);   // 小端高字节
    pack.data[6] = 0x00;
    pack.data[7] = 0x00;

    CANTransmit(motor->base.can, &pack, motor->base.timeout_ms, NULL, NULL);
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRV_LKMOTOR_USED */

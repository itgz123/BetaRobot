/**
 * @file drv_lkmotor_broadcast.c
 * @brief LK（瓴控/翎控）MF 系列电机「一拖四 / 广播模式」驱动实现（协议 V2.35）
 *
 * @note 发送：一帧 0x280 携带最多 4 个电机的 int16 小端扭矩，第 id 号电机占 data[(id-1)*2..+1]。
 *       接收：各电机以 0x140 + id 回复「状态2」小端帧。
 *
 * @note 控制：MCU 端 PID 级联，Calculate 输出扭矩 (Nm)，Send 时换算为 0x280 的 int16 原始值。
 *       失能电机的槽位填 0（零扭矩），与旧工程停机方式一致。
 *
 * @note MF 为直驱（无减速机），反馈与下发均为电机轴量，严禁乘减速比。
 */

#include "drv_lkmotor_broadcast.h"
#include "app_cfg.h"

#ifdef DRV_LKMOTOR_BROADCAST_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "lib_math.h"
#include <string.h>

/*============================================
 *              协议常量
 *============================================*/
#define LK_BROADCAST_TORQUE_ID 0x280u       // 力矩广播发送 ID（固定）
#define LK_BROADCAST_REPLY_ID_BASE 0x140u   // 回复 ID = 0x140 + motor_id
#define LK_BROADCAST_MOTOR_ID_MIN 1u        // 电机 ID 下限
#define LK_BROADCAST_MOTOR_ID_MAX 4u        // 一拖四：一条广播总线最多 4 个电机（ID 1~4）
#define LK_BROADCAST_TORQUE_RAW_MAX 2000.0f // 0x280 torqueValue 限幅（MF/MG ±2000）

#define LK_BROADCAST_IQ_A_PER_LSB (33.0f / 4096.0f)     // MF 固定：A = raw × 33/4096（MG 为 66/4096，本驱动不支持）
#define LK_BROADCAST_DEG_TO_RADPS 0.017453292519943295f // π/180，dps → rad/s

/* 编码器：实测整圈 raw 覆盖 0~65535（转一圈恰好回绕一次），固定按 16bit 处理 */
#define LK_BROADCAST_ENCODER_TO_RAD (M_2PI / 65536.0f) // raw → rad
#define LK_BROADCAST_WRAP_SPAN_RAD M_2PI               // 线值回绕一次的机械角跨度 = 一圈
#define LK_BROADCAST_INV_WRAP_SPAN INV_M_2PI           // 多圈穿越检测用

/*============================================
 *              虚函数表实例
 *
 * @note 必须放在文件顶部（Register 函数中使用）
 *============================================*/
void LKMotorBroadcast_Enable(void *inst);
void LKMotorBroadcast_Disable(void *inst);
void LKMotorBroadcast_SetRef(void *inst, float ref);
void LKMotorBroadcast_Send(void *inst);
MotorData_s LKMotorBroadcast_GetData(void *inst);

const static MotorVTable_s s_lk_motor_broadcast_vtable = {
    .enable = LKMotorBroadcast_Enable,
    .disable = LKMotorBroadcast_Disable,
    .set_ref = LKMotorBroadcast_SetRef,
    .send = LKMotorBroadcast_Send,
    .get_data = LKMotorBroadcast_GetData,
    .send_cmd = NULL, /* 广播模式只发 0x280，无独立模式命令 */
};

/*============================================
 *              广播组（每条总线一组）
 *
 * 槽位 = motor_id - 1；Send 时整组打包进同一帧 0x280。
 *============================================*/
static LKMotorBroadcastSendGroup_s s_broadcast_groups[CAN_NUM_MAX] = {0};

/*============================================
 *              内部辅助函数
 *============================================*/

/**
 * @brief 判断回显字节是否为「状态2格式」的回复
 * @note  只有控制命令 0xA0~0xA8 与 0x9C 的回复是状态2格式。
 *        本项目只发 0x280（0xA1 语义），其余回显或为全零回显、或为其他帧格式，必须排除。
 */
static inline uint8_t lk_broadcast_is_status_frame(uint8_t echo)
{
    return (echo == 0xA1u || echo == 0x9Cu) ? 1u : 0u;
}

/*============================================
 *              CAN 接收回调（最小化 ISR 工作）
 *
 * 仅做：memcpy 原始 8 字节 + 记录时间戳 + flip 双缓冲 + 喂狗。
 * 回显校验与数据处理全部放到 LKMotorBroadcast_GetData。
 *============================================*/
static void LKMotorBroadcastRxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (!can || !can->parent)
        return;

    LKMotorBroadcastInstance *motor = (LKMotorBroadcastInstance *)can->parent;

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
 *              统一数据获取接口 (LKMotorBroadcast_GetData)
 *
 * 处理链：回显校验 → 解析 → SI 转换 → 多圈累加 → 速度计算 → 滤波 → 偏置/方向
 *============================================*/
MotorData_s LKMotorBroadcast_GetData(void *inst)
{
    MotorData_s result = {0};
    if (!inst)
        return result;

    LKMotorBroadcastInstance *motor = (LKMotorBroadcastInstance *)inst;
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
    /* 全零回显帧或非状态2格式帧直接丢弃，返回旧缓存，
     * 否则会把位置/速度误解析为 0 造成跳变。标记 data_valid=1 避免反复重解析同一脏帧。 */
    if (!lk_broadcast_is_status_frame(frame.bytes[0]))
    {
        base->data_valid = 1;
        return base->data_all.data;
    }

    /* ====== Step 3: 解析 + SI 转换（小端） ====== */
    const LKMotorBroadcastProtocolMap_s *map = &motor->proto_map;
    MotorControllerSetting_s *setting = &base->setting;

    int8_t raw_temp = (int8_t)frame.bytes[1];
    int16_t raw_iq = (int16_t)((uint16_t)frame.bytes[2] | ((uint16_t)frame.bytes[3] << 8));
    int16_t raw_speed = (int16_t)((uint16_t)frame.bytes[4] | ((uint16_t)frame.bytes[5] << 8));
    uint16_t raw_encoder = (uint16_t)((uint16_t)frame.bytes[6] | ((uint16_t)frame.bytes[7] << 8));

    // 单圈位置 (rad) [0, 2π)
    float position_single = (float)raw_encoder * LK_BROADCAST_ENCODER_TO_RAD;
    // 转速 (rad/s)，1dps/LSB
    float velocity_raw = (float)raw_speed * LK_BROADCAST_DEG_TO_RADPS;
    // 转矩电流 (A) 与转矩 (Nm)
    float iq_A = (float)raw_iq * LK_BROADCAST_IQ_A_PER_LSB;
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
            float wrap_float = (expected_change - angle_diff) * LK_BROADCAST_INV_WRAP_SPAN;
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

    base->data_all.position_cnt += wraps;                                                                               // ① 累加（单位 = 线值回绕次数）
    double angle = ((double)base->data_all.position_cnt * (double)LK_BROADCAST_WRAP_SPAN_RAD) + (double)position_single // ① 累加
                   + (double)base->position_offset;                                                                     // ② 偏置

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
 * @brief LK 广播电机离线回调
 * @param owner 守护进程所有者 (LKMotorBroadcastInstance*)
 * @note  广播模式无独立恢复帧，仅复位 PID 防止积分累积；
 *        离线期间 Send 仍会以 0 扭矩占用槽位（安全）。
 */
static void LKMotorBroadcastDaemonCallback(void *owner)
{
    if (!owner)
        return;

    LKMotorBroadcastInstance *motor = (LKMotorBroadcastInstance *)owner;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);
}

/*============================================
 *              注册函数
 *============================================*/

/**
 * @brief 注册 LK 广播电机实例（仅调用一次）
 * @note 只注册 CAN/Daemon 实例，不配置电机参数（由 LKMotorBroadcastConfig 负责）。
 */
int8_t LKMotorBroadcastRegister(LKMotorBroadcastInstance *inst)
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
    inst->base.vtable = &s_lk_motor_broadcast_vtable;

    // 注册 daemon（占位，Config 更新运行参数）
    if (inst->base.daemon)
    {
        DaemonRegister(inst->base.daemon);
    }

    return 0;
}

/**
 * @brief 配置 LK 广播电机实例（可重复调用）
 * @note 配置协议映射、PID、CAN 滤波器、daemon、广播组等，要求在 Register 之后调用。
 * @note iq 分辨率与编码器量纲对全系列 MF 固定（见文件顶部常量），Config 只暴露随型号变的 torque_constant。
 */
int8_t LKMotorBroadcastConfig(LKMotorBroadcastInstance *inst, LKMotorBroadcast_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    /* 参数校验：只支持 MF，广播模式一拖四 ID 1~4 */
    if (cfg->model >= LK_MODEL_NUM || cfg->model != LK_MODEL_MF)
        return -1;
    if (cfg->motor_id < LK_BROADCAST_MOTOR_ID_MIN || cfg->motor_id > LK_BROADCAST_MOTOR_ID_MAX)
        return -1;
    if (cfg->torque_constant <= 0.0f)
        return -1;

    uint16_t can_id = (uint16_t)(LK_BROADCAST_REPLY_ID_BASE + cfg->motor_id);
    uint8_t slot = (uint8_t)(cfg->motor_id - 1u);
    BoardCAN_e can_e = cfg->can_e;

    /* 槽位占用检查：一条总线同一槽位只能有一个电机 */
    if (s_broadcast_groups[can_e].motor_init_flag[slot] == 1u)
        return -1;

    /* 配置 CAN 滤波器（仅接收 0x140 + motor_id 的回复，工作模式 CLASSIC） */
    if (inst->base.can)
    {
        inst->base.can_filter.mode = CAN_FILTER_MODE_LIST;
        inst->base.can_filter.id0 = can_id;
        inst->base.can_filter.id1 = CAN_ID_UNUSED;
        inst->base.can_filter.frame_type = CAN_STANDARD_DATA_FRAME;
        inst->base.can_filter.callback = LKMotorBroadcastRxCallback;

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

    /* --- 协议映射：默认值 + 预计算 scale --- */
    LKMotorBroadcastProtocolMap_s *map = &inst->proto_map;

    map->torque_constant = cfg->torque_constant;
    map->inv_torque_constant = 1.0f / cfg->torque_constant;

    /* 基本属性 */
    inst->base.model = cfg->model;
    inst->motor_id = cfg->motor_id;
    inst->base.timeout_ms = cfg->timeout_ms;
    inst->base.position_offset = cfg->position_offset;
    inst->motor_state = BROADCAST_STATE_DISABLED;

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

    /* 加入广播组（槽位 = motor_id-1） */
    s_broadcast_groups[can_e].motors[slot] = inst;
    s_broadcast_groups[can_e].motor_init_flag[slot] = 1u;
    inst->send_group = &s_broadcast_groups[can_e];
    inst->slot = slot;

    /* 更新 daemon 运行参数（可重入） */
    if (inst->base.daemon)
    {
        Daemon_Config_s daemon_cfg = {
            .callback = LKMotorBroadcastDaemonCallback,
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
 * @brief 单个 LK 广播电机 PID 级联计算
 * @param inst LK 广播电机实例
 * @note  输出 base.controller.output 为扭矩 (Nm)，由 Send 统一换算为 0x280 原始值。
 */
static void LKMotorBroadcast_Calculate(LKMotorBroadcastInstance *inst)
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
    MotorData_s md = LKMotorBroadcast_GetData(inst);

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

    ctrl->output = output; // 单位：扭矩 (Nm)
}

/*============================================
 *              虚函数实现
 *============================================*/

/**
 * @brief 使能 LK 广播电机
 * @note  广播模式无独立使能帧（电机上电默认开启），仅置标志；
 *        真正生效体现为 Send 时该槽位开始下发扭矩（失能时槽位恒 0）。
 */
void LKMotorBroadcast_Enable(void *inst)
{
    if (!inst)
        return;
    LKMotorBroadcastInstance *motor = (LKMotorBroadcastInstance *)inst;
    motor->base.enable = MOTOR_ENABLE;
    motor->motor_state = BROADCAST_STATE_ENABLED;
}

/**
 * @brief 失能 LK 广播电机
 * @note  不发帧；下次 Send 时该槽位自动填 0，即零扭矩停机。
 */
void LKMotorBroadcast_Disable(void *inst)
{
    if (!inst)
        return;
    LKMotorBroadcastInstance *motor = (LKMotorBroadcastInstance *)inst;
    motor->base.enable = MOTOR_DISABLE;
    motor->motor_state = BROADCAST_STATE_DISABLED;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);
}

/**
 * @brief 设置电机控制参考值
 * @param inst LK 广播电机实例
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
void LKMotorBroadcast_SetRef(void *inst, float ref)
{
    if (!inst)
        return;
    LKMotorBroadcastInstance *motor = (LKMotorBroadcastInstance *)inst;
    motor->base.controller.ref = ref;
}

/**
 * @brief 发送 LK 广播力矩帧（0x280）
 * @param inst LK 广播电机实例（同一总线任意一个即可）
 *
 * 帧格式（8字节，int16 小端）：
 *   data[(id-1)*2]   = torqueValue 低字节
 *   data[(id-1)*2+1] = torqueValue 高字节
 *
 * @note 组内 4 个电机共用一帧：先逐个 Calculate，再按槽位打包。
 *       未初始化或未使能的槽位填 0（零扭矩）。
 *       每个控制周期每条总线只需调用一次，重复调用会重复占用总线。
 */
void LKMotorBroadcast_Send(void *inst)
{
    if (!inst)
        return;
    LKMotorBroadcastInstance *motor = (LKMotorBroadcastInstance *)inst;

    LKMotorBroadcastSendGroup_s *group = motor->send_group;
    if (!group)
        return;

    /* ===== 控制计算：组内所有已初始化电机 ===== */
    for (uint8_t i = 0; i < LK_BROADCAST_SLOTS; i++)
    {
        if (group->motor_init_flag[i] && group->motors[i])
        {
            LKMotorBroadcast_Calculate(group->motors[i]);
        }
    }

    /* ===== 组一个 0x280 帧，按槽位打包 ===== */
    CAN_Pack_s pack = {.id = LK_BROADCAST_TORQUE_ID, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};

    for (uint8_t i = 0; i < LK_BROADCAST_SLOTS; i++)
    {
        int16_t raw = 0;
        LKMotorBroadcastInstance *m = group->motors[i];

        if (group->motor_init_flag[i] && m && m->base.enable)
        {
            const LKMotorBroadcastProtocolMap_s *map = &m->proto_map;

            /* Nm → A → 0x280 原始值，限到协议 ±2000（MF/MG 幅值上限） */
            float iq_f = m->base.controller.output * map->inv_torque_constant / LK_BROADCAST_IQ_A_PER_LSB;
            if (!isfinite(iq_f))
                iq_f = 0.0f;
            iq_f = Lib_Math_Clamp(iq_f, -LK_BROADCAST_TORQUE_RAW_MAX, LK_BROADCAST_TORQUE_RAW_MAX);
            raw = (int16_t)(iq_f >= 0.0f ? iq_f + 0.5f : iq_f - 0.5f);
        }

        /* 小端写入槽位 (id-1)*2 */
        pack.data[i * 2] = (uint8_t)((uint16_t)raw & 0xFF);
        pack.data[i * 2 + 1] = (uint8_t)((uint16_t)raw >> 8);
    }

    /* ===== 用组内第一个已初始化电机的 CAN 实例发送 ===== */
    for (uint8_t i = 0; i < LK_BROADCAST_SLOTS; i++)
    {
        LKMotorBroadcastInstance *m = group->motors[i];
        if (group->motor_init_flag[i] && m && m->base.can)
        {
            CANTransmit(m->base.can, &pack, m->base.timeout_ms, NULL, NULL);
            break;
        }
    }
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRV_LKMOTOR_BROADCAST_USED */

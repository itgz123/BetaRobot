/**
 * @file drv_rsmotor.c
 * @brief 灵足时代 RS05 准直驱电机驱动实现
 *
 * @note 协议：CAN MIT 模式，8字节帧，12位跨字节位域（与 DM 控制帧逐字节相同）
 * @note 映射：无符号整数线性映射（非有符号2的补码）
 * @note 控制：上位机做 PID 级联控制（Kp=Kd=0），输出扭矩通过 t_ff 下发
 *
 * @note 与 DM 驱动的主要区别（反馈帧）：
 *       - Byte0 = 电机 canid（DM 是 error+id）
 *       - D6[7:6]=模式状态, D6[5]=故障, D6[4]=预警（DM 无状态位）
 *       - 绕组温度 = 12 位（D6[3:0]+D7），值 = ℃×10，需 ÷10（DM 是两个 int8 温度）
 *       - 位置语义为输出轴（7.75:1 减速比由电机固件折算），严禁再乘减速比
 *
 * @note ⚠️ 硬前提：RS05 出厂默认私有协议（扩展帧），必须先使用灵足上位机
 *       将 protocol_1 标志位切换为 2（MIT），**重新上电生效**后本驱动方可工作。
 */

#include "drv_rsmotor.h"
#include "app_cfg.h"

#ifdef DRV_RSMOTOR_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_math.h"
#include <string.h>

#define CAN_TRANSMIT_TIMEOUT 1

/* RS05 默认量程（用户 Config 传 0 时自动采用） */
#define RS_DEFAULT_POS_MAX 12.57f  // 位置范围 ±12.57 rad
#define RS_DEFAULT_VEL_RANGE 50.0f // 速度范围 ±50 rad/s
#define RS_DEFAULT_T_RANGE 5.5f    // 扭矩范围 ±5.5 Nm

/*============================================
 *              虚函数表实例
 *
 * @note  必须放在文件顶部（Register 函数中使用）
 *============================================*/
void RSMotor_Enable(void *inst);
void RSMotor_Disable(void *inst);
void RSMotor_SetRef(void *inst, float ref);
void RSMotor_Send(void *inst);
MotorData_s RSMotor_GetData(void *inst);

const static MotorVTable_s s_rs_motor_vtable = {
    .enable = RSMotor_Enable,
    .disable = RSMotor_Disable,
    .set_ref = RSMotor_SetRef,
    .send = RSMotor_Send,
    .get_data = RSMotor_GetData,
    .send_cmd = RSMotor_SendModeCmd,
};

/*============================================
 *              内部辅助函数（热路径优化）
 *
 * @note  使用预计算的 scale 因子，将浮点除法变为乘法：
 *        Cortex-M7 FPU: mul=1cycle, div≈14cycles → 节省 ~13cycles/次
 *============================================*/

/**
 * @brief 无符号定点 → 浮点（热路径：每帧 3 次 / 电机）
 * @note  uint→float: raw * scale + offset  (1 mul + 1 add)
 *        scale = span / (2^bits - 1) 已在参数表预计算
 */
static inline float rs_uint_to_float(uint16_t uint_val, float scale, float offset)
{
    return (float)uint_val * scale + offset;
}

/**
 * @brief 浮点 → 无符号定点（热路径：每帧 1 次 / 电机）
 * @note  float→uint: (val + range_max) * inv_scale  (1 add + 1 mul)
 *        inv_scale = (2^bits - 1) / span 已在参数表预计算
 */
static inline uint16_t rs_float_to_uint(float float_val, float inv_scale, float range_max)
{
    /* 输入保护：NaN/Inf 和限幅 */
    if (!isfinite(float_val))
        float_val = 0.0f;
    if (float_val < -range_max)
        float_val = -range_max;
    else if (float_val > range_max)
        float_val = range_max;

    return (uint16_t)((float_val + range_max) * inv_scale);
}

/*============================================
 *              模式命令发送
 *============================================*/

/**
 * @brief 发送 RS05 电机模式命令
 * @param inst RS05 电机实例
 * @param cmd  命令码
 * @note  帧格式：前7字节 = 0xFF，第8字节 = 命令码（与 DM 相同）
 */
void RSMotor_SendModeCmd(void *inst, uint8_t cmd)
{
    if (!inst)
        return;
    RSMotorInstance *motor = (RSMotorInstance *)inst;

    if (!motor->base.can)
        return;

    CANInstance *can = motor->base.can;
    CAN_Pack_s pack = {.id = motor->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    memset(pack.data, 0xFF, 7);
    pack.data[7] = cmd;
    CANTransmit(can, &pack, CAN_TRANSMIT_TIMEOUT, NULL, NULL);
}

/*============================================
 *              调试接口：修改 CAN ID
 *
 * 依据 RS05 说明书 MIT 协议：
 *   - 指令7 (0xFA)：修改电机 CANID，Byte6 = 新电机 id
 *   - 指令9 (0x01)：修改主机 CANID，Byte6 = 新主机 id
 * 两指令帧 ID 均用电机"当前" canid 发出（一对一定址）。
 *
 * @note 改完后电机仅响应新 ID，实例的 CAN 滤波器(tx_id/rx_id)
 *       不会自动更新，需重新调用 RSMotorConfig 才能继续用新 ID 通信。
 * @note 断电保存需固件 ≥0.5.0.13 并另发保存命令（指令12, 0xF8），本驱动未封装。
 *============================================*/

void RSMotor_ChangeCanID(void *inst, uint16_t can_id)
{
    if (!inst)
        return;
    RSMotorInstance *motor = (RSMotorInstance *)inst;

    if (!motor->base.can)
        return;

    CANInstance *can = motor->base.can;
    CAN_Pack_s pack = {.id = motor->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    memset(pack.data, 0xFF, 6);          // 前 6 字节固定 0xFF
    pack.data[6] = (uint8_t)can_id;      // Byte6 = 新电机 id
    pack.data[7] = RS_CMD_CHANGE_CAN_ID; // Byte7 = 指令7 (0xFA)
    CANTransmit(can, &pack, CAN_TRANSMIT_TIMEOUT, NULL, NULL);
}

void RSMotor_ChangeMasterCanID(void *inst, uint16_t can_id)
{
    if (!inst)
        return;
    RSMotorInstance *motor = (RSMotorInstance *)inst;

    if (!motor->base.can)
        return;

    CANInstance *can = motor->base.can;
    CAN_Pack_s pack = {.id = motor->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    memset(pack.data, 0xFF, 6);             // 前 6 字节固定 0xFF
    pack.data[6] = (uint8_t)can_id;         // Byte6 = 新主机 id
    pack.data[7] = RS_CMD_CHANGE_MASTER_ID; // Byte7 = 指令9 (0x01)
    CANTransmit(can, &pack, CAN_TRANSMIT_TIMEOUT, NULL, NULL);
}

/*============================================
 *              CAN 接收回调（最小化 ISR 工作）
 *
 * 仅做：memcpy 原始 8 字节 + 记录时间戳 + flip 双缓冲 + 喂狗。
 * 所有数据处理（位域解析、SI 转换、多圈累加、速度滤波）移到 RSMotor_GetData。
 *============================================*/

/**
 * @brief RS05 电机 CAN 反馈帧回调（ISR 上下文）
 * @param can CAN 实例指针
 *
 * 反馈帧格式（8字节）：
 *   D[0]：电机 canid
 *   D[1..2]：位置 16位 无符号 大端
 *   D[3]：速度高8位 VEL[11:4]
 *   D[4]：bits[7:4]=速度低4位 VEL[3:0], bits[3:0]=扭矩高4位 T[11:8]
 *   D[5]：扭矩低8位 T[7:0]
 *   D[6]：bits[7:6]=模式状态, bit[5]=故障, bit[4]=预警, bits[3:0]=绕组温度高4位
 *   D[7]：绕组温度低8位 TEMP[7:0]（= ℃×10）
 */
static void RSMotorRxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (!can || !can->parent)
        return;

    RSMotorInstance *motor = (RSMotorInstance *)can->parent;

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
 *              统一数据获取接口 (RSMotor_GetData)
 *
 * 完整处理链：位域解析 → SI 转换 → 多圈累加 → 速度计算 → 滤波 → 偏置/方向
 *============================================*/

/**
 * @brief RS05 电机统一数据获取
 * @param inst RSMotorInstance 指针
 * @return MotorData_s 包含所有反馈数据
 */
MotorData_s RSMotor_GetData(void *inst)
{
    MotorData_s result = {0};
    if (!inst)
        return result;

    RSMotorInstance *motor = (RSMotorInstance *)inst;
    MotorBase_s *base = &motor->base;

    /* 缓存命中：上次获取后没有新中断，直接返回缓存 */
    if (base->data_valid)
        return base->data_all.data;

    /* ====== Step 1: 从双缓冲读取就绪帧 ====== */
    uint8_t ready_idx = !base->raw_frame_idx;
    MotorRawFrame_s frame = base->raw_frames[ready_idx];

    /* ====== Step 2: 解析位域 + SI 转换 ====== */
    const RS_FeedbackFrame_u *fb = (const RS_FeedbackFrame_u *)frame.bytes;
    uint16_t raw_position = ((uint16_t)fb->parts.position_be >> 8) | ((uint16_t)fb->parts.position_be << 8);
    uint16_t raw_velocity = ((uint16_t)fb->parts.vel_hi << 4) | (fb->parts.vel_lo_and_torque_hi >> 4);
    uint16_t raw_torque = ((uint16_t)(fb->parts.vel_lo_and_torque_hi & 0x0F) << 8) | fb->parts.torque_lo;
    uint16_t raw_temperature = ((uint16_t)(fb->parts.status_and_temp_hi & 0x0F) << 8) | fb->parts.temp_lo;

    RSModel_e model = base->model;
    if (model >= RS_MODEL_NUM)
        return result;

    const RSMotorProtocolMap_s *map = &motor->proto_map;
    MotorControllerSetting_s *setting = &base->setting;

    // 单圈/单范围位置 (rad) [−p_max, +p_max]
    float position_single = rs_uint_to_float(raw_position, map->pos_to_float_scale, -map->p_max);
    // 速度 (rad/s)
    float velocity_raw = rs_uint_to_float(raw_velocity, map->vel_to_float_scale, -map->v_range);
    // 扭矩 (Nm)
    float torque = rs_uint_to_float(raw_torque, map->t_to_float_scale, -map->t_range);
    // 绕组温度 (℃)，原始 12 位值 = ℃×10
    float temperature_winding = (float)raw_temperature * 0.1f;

    /* ====== Step 3: dt 计算 ====== */
    float dt = 0.0f;
    if (base->data_all.timestamp_last_us > 0 && frame.timestamp_us > base->data_all.timestamp_last_us)
    {
        dt = (float)(frame.timestamp_us - base->data_all.timestamp_last_us) * 1e-6f;
    }

    /* ====== Step 4: 位置计算（多圈，偏置，归一化，方向修正） ====== */

    /**
     * @note 多圈位置由上位机累加，不依赖电机自身。
     *       RS05 位置范围由用户配置（p_max，默认 12.57 rad ≈ ±4π），
     *       单次"穿越"跨度 = 2 * p_max，最大可达 25.14 rad（约 4 个输出轴圈）。
     *       由于 RS05 减速比 7.75:1（输出轴转速低），穿越频率极低，
     *       连续丢帧导致多圈出错的概率远小于 DJI，几乎不受影响。
     */
    // 计算顺序必须是：累加-偏置-方向-归一化
    int64_t wraps = 0;
    if (base->data_all.timestamp_last_us > 0)
    {
        float angle_diff = position_single - base->data_all.position_single_last;
        if (dt > 0.0f)
        {
            float expected_change = velocity_raw * dt;
            float wrap_float = (expected_change - angle_diff) * map->inv_wrap_span;
            wraps = (int64_t)(wrap_float > 0.0f ? wrap_float + 0.5f : wrap_float - 0.5f);
        }
        else
        {
            if (angle_diff > map->p_max)
                wraps = -1;
            else if (angle_diff < -map->p_max)
                wraps = 1;
        }
    }

    // 累加，偏置
    base->data_all.position_cnt += wraps;
    double angle = ((double)base->data_all.position_cnt * (2.0 * (double)map->p_max)) + (double)position_single // ① 累加
                   + (double)base->position_offset;                                                             // ② 偏置

    // 方向
    angle *= setting->feedback_direction; // ③ 方向

    // 归一化
    if (setting->position_mode == MOTOR_POSITION_WRAP) // ④ 归一化
    {
        angle = BSP_Math_WrapAngle(angle, setting->angle_limit_min, setting->angle_limit_max);
    }

    result.position = angle;

    /* ====== Step 5: 速度计算，滤波，方向修正 ====== */
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

    /* ====== Step 6: 扭矩方向修正 ====== */
    result.torque = torque * setting->feedback_direction;

    /* ====== Step 7: 其他数据 ====== */
    result.timestamp_us = frame.timestamp_us;
    motor->temperature_winding = temperature_winding;
    motor->mode_state = fb->parts.status_and_temp_hi >> 6;        // bits[7:6]
    motor->fault_flag = (fb->parts.status_and_temp_hi >> 5) & 1u; // bit[5]
    motor->warn_flag = (fb->parts.status_and_temp_hi >> 4) & 1u;  // bit[4]
    motor->error = motor->fault_flag;                             // 兼容字段 = fault_flag

    // 缓存到 data_all（speed 存未修正值，LPF 状态一致）
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
 * @brief RS05 电机离线回调
 * @param owner 守护进程所有者 (RSMotorInstance*)
 * @note  RS05 电机通信丢失后会自动退出 MIT 模式，
 *        因此除了复位 PID 外，还需重发 0xFC 使能命令。
 */
static void RSMotorDaemonCallback(void *owner)
{
    if (!owner)
        return;

    RSMotorInstance *motor = (RSMotorInstance *)owner;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);

    /* 重发使能命令，恢复 MIT 控制模式 */
    RSMotor_SendModeCmd(motor, RS_CMD_MOTOR_MODE);
}

/*============================================
 *              注册函数
 *============================================*/

/**
 * @brief 注册RS05电机实例（仅调用一次）
 * @note 只注册 CAN/Daemon 实例，不配置电机参数（由 RSMotorConfig 负责）。
 */
int8_t RSMotorRegister(RSMotorInstance *inst)
{
    if (!inst)
        return -1;

    // 防重复注册检查
    if (inst->base.can && inst->base.can->parent == inst)
        return -1;

    // 注册 CAN 实例（仅绑定 CAN 外设，tx_id/rx_id/callback 由 Config 设置）
    if (inst->base.can)
    {
        if (CANRegister(inst->base.can) != 0)
            return -1;
        inst->base.can->parent = inst;
    }

    // 初始化基本属性
    inst->base.brand = MOTOR_BRAND_RS;
    inst->base.enable = MOTOR_DISABLE;
    inst->base.vtable = &s_rs_motor_vtable;

    // 注册 daemon（占位，Config 更新运行参数）
    if (inst->base.daemon)
    {
        DaemonRegister(inst->base.daemon);
    }

    return 0;
}

/**
 * @brief 配置RS05电机实例（可重复调用）
 * @note 配置协议映射、PID、CAN 滤波器、daemon 等。
 *       要求在 RSMotorRegister 之后调用。
 * @note pos_max/vel_range/t_range 传 0 时自动取默认量程
 *       （12.57 rad / 50 rad/s / 5.5 Nm）。
 */
int8_t RSMotorConfig(RSMotorInstance *inst, RSMotor_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    /* 参数校验 */
    if (cfg->model >= RS_MODEL_NUM)
        return -1;

    /* 配置 CAN 滤波器（tx_id=can_id 逐帧指定, rx_id=master_id 接收回调, 工作模式 CLASSIC） */
    if (inst->base.can)
    {
        inst->base.can_filter.mode = CAN_FILTER_MODE_LIST;
        inst->base.can_filter.id0 = cfg->master_id;
        inst->base.can_filter.id1 = CAN_ID_UNUSED;
        inst->base.can_filter.frame_type = CAN_STANDARD_DATA_FRAME;
        inst->base.can_filter.callback = RSMotorRxCallback;

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

    /* --- 协议映射：校验用户配置 ≤ 硬件极限，计算 scale --- */
    RSMotorProtocolMap_s *map = &inst->proto_map;

    /* 量程传 0 时自动取 RS05 默认量程 */
    map->p_max = (cfg->pos_max > 0.0f) ? cfg->pos_max : RS_DEFAULT_POS_MAX;
    map->v_range = (cfg->vel_range > 0.0f) ? cfg->vel_range : RS_DEFAULT_VEL_RANGE;
    map->t_range = (cfg->t_range > 0.0f) ? cfg->t_range : RS_DEFAULT_T_RANGE;

    if (map->v_range < 1e-6f)
        map->v_range = 1e-6f;
    if (map->t_range < 1e-6f)
        map->t_range = 1e-6f;

    map->pos_to_float_scale = (2.0f * map->p_max) / 65535.0f;
    map->vel_to_float_scale = (2.0f * map->v_range) / 4095.0f;
    map->vel_to_uint_scale = 4095.0f / (2.0f * map->v_range);
    map->t_to_float_scale = (2.0f * map->t_range) / 4095.0f;
    map->t_to_uint_scale = 4095.0f / (2.0f * map->t_range);
    map->inv_wrap_span = 1.0f / (2.0f * map->p_max);

    /* 基本属性 */
    inst->base.model = cfg->model;
    inst->can_id = cfg->can_id;
    inst->master_id = cfg->master_id;
    inst->base.position_offset = cfg->position_offset; // 位置偏置

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
            .callback = RSMotorDaemonCallback,
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
 * @brief 单个 RS05 电机 PID 级联计算
 * @param inst RS05 电机实例
 */
static void RSMotor_Calculate(RSMotorInstance *inst)
{
    if (!inst || !inst->base.enable)
        return;

    MotorControllerSetting_s *setting = &inst->base.setting;
    MotorController_s *ctrl = &inst->base.controller;

    float setpoint = ctrl->ref;
    float measure;
    float output = 0.0f;

    if (inst->base.model >= RS_MODEL_NUM)
        return;

    /* 统一获取一次反馈数据（后续 GetData 走缓存，不重复解析） */
    MotorData_s md = RSMotor_GetData(inst);

    // 位置环 (最外环)
    if (setting->loop_type & MOTOR_LOOP_ANGLE)
    {
        // 根据 position_mode 处理 setpoint
        switch (setting->position_mode)
        {
        case MOTOR_POSITION_LIMITED:
            // 限幅模式：setpoint 限幅到 [min, max]
            if (setting->angle_limit_min < setting->angle_limit_max)
            {
                setpoint = BSP_Math_Clamp(setpoint, setting->angle_limit_min, setting->angle_limit_max);
            }
            break;
        case MOTOR_POSITION_WRAP:
            // 环绕模式：setpoint 归一化到 [min, max)
            setpoint = BSP_Math_WrapAngle(setpoint, setting->angle_limit_min, setting->angle_limit_max);
            break;
        case MOTOR_POSITION_CONTINUOUS:
        default:
            // 连续模式：不限幅
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
        // 开环模式 (MOTOR_LOOP_OPEN): setpoint 直接作为扭矩输出，依赖 RSMotor_Send 限幅保护
        // 仅位置环模式 (MOTOR_LOOP_ANGLE): setpoint 是位置环 PID 输出（扭矩 Nm）
        output = setpoint;
    }

    // 电机方向修正: motor_direction修正电机安装方向, feedback_direction已在反馈端修正
    output *= setting->motor_direction;

    ctrl->output = output;
}

/*============================================
 *              虚函数实现
 *============================================*/

void RSMotor_Enable(void *inst)
{
    if (!inst)
        return;
    RSMotorInstance *motor = (RSMotorInstance *)inst;

    /* 先发送使能命令，再设置标志 */
    RSMotor_SendModeCmd(motor, RS_CMD_MOTOR_MODE);
    motor->base.enable = MOTOR_ENABLE;
}

void RSMotor_Disable(void *inst)
{
    if (!inst)
        return;
    RSMotorInstance *motor = (RSMotorInstance *)inst;

    /* 发送停止命令 + 清除使能标志 + 复位 PID */
    RSMotor_SendModeCmd(motor, RS_CMD_RESET_MODE);
    motor->base.enable = MOTOR_DISABLE;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);
}

/**
 * @brief 设置电机控制参考值
 * @param inst RSMotorInstance 指针
 * @param ref  参考值
 *
 * 方向标定流程:
 *   1. 人为设定正方向（顺时针或逆时针）
 *   2. 开环控制，发送正的较小扭矩值，观察实际旋转方向和反馈方向
 *   3. 如果实际旋转方向与正方向相反，设置 motor_direction = MOTOR_DIRECTION_REVERSE
 *   4. 如果反馈正负与实际旋转方向相反，设置 feedback_direction = MOTOR_DIRECTION_REVERSE
 *
 * 方向处理逻辑:
 *   - motor_direction: 修正电机安装方向，在输出端翻转扭矩方向
 *   - feedback_direction: 修正反馈极性，在反馈获取函数中翻转反馈值符号
 *   - PID 计算在统一的坐标系下进行，setpoint 和 measure 都已正确处理方向
 *
 * 控制模式说明:
 *   - MOTOR_LOOP_OPEN（扭矩开环）：ref = 扭矩(Nm) → CAN发送
 *   - MOTOR_LOOP_SPEED（速度环）：ref = 速度(rad/s) → PID(扭矩) → CAN发送
 *   - MOTOR_LOOP_ANGLE（位置环）：ref = 位置(rad) → PID(扭矩) → CAN发送
 *   - MOTOR_LOOP_ANGLE | MOTOR_LOOP_SPEED（位置-速度双环）：
 *       ref = 位置(rad) → PID(速度) → PID(扭矩) → CAN发送
 *
 * 最终输出统一限幅到 ±t_range (Nm) 再发送到 CAN 总线
 */
void RSMotor_SetRef(void *inst, float ref)
{
    if (!inst)
        return;
    RSMotorInstance *motor = (RSMotorInstance *)inst;
    motor->base.controller.ref = ref;
}

/**
 * @brief 发送 RS05 电机控制帧
 * @param inst RSMotorInstance 指针
 *
 * 控制帧格式（MIT 模式，8字节，与 DM 逐字节相同）：
 *   D[0..1]：p_des 16位 大端
 *   D[2]：v_des[11:4]
 *   D[3]：bits[7:4]=v_des[3:0], bits[3:0]=Kp[11:8]
 *   D[4]：Kp[7:0]
 *   D[5]：Kd[11:4]
 *   D[6]：bits[7:4]=Kd[3:0], bits[3:0]=t_ff[11:8]
 *   D[7]：t_ff[7:0]
 *
 * @note  本项目在上位机做 PID，板载 PD 不使用（p_des=0, v_des=0, Kp=0, Kd=0）。
 *        仅通过 t_ff 下发扭矩输出。
 *
 * @note  RS05 电机无需分组发送，每个电机独立发送一帧 CAN 报文。
 */
void RSMotor_Send(void *inst)
{
    if (!inst)
        return;
    RSMotorInstance *motor = (RSMotorInstance *)inst;

    if (!motor->base.can)
        return;

    /* 失能时不发送控制帧 */
    if (!motor->base.enable)
        return;

    /* 控制计算 */
    RSMotor_Calculate(motor);

    /* 扭矩限幅 (Nm)，根据电机型号的 t_range 保护 */
    const RSMotorProtocolMap_s *map = &motor->proto_map;
    float output_clamped = BSP_Math_Clamp(motor->base.controller.output, -map->t_range, map->t_range);

    /* 扭矩输出：浮点(Nm) → 12位无符号定点 */
    uint16_t p_des = 0; /* 不使用板载位置控制 */
    uint16_t v_des = 0; /* 不使用板载速度控制 */
    uint16_t kp = 0;    /* 不使用板载 PD */
    uint16_t kd = 0;    /* 不使用板载 PD */
    uint16_t t_ff = rs_float_to_uint(output_clamped,
                                     motor->proto_map.t_to_uint_scale,
                                     motor->proto_map.t_range);

    /* 使用控制帧联合体打包（ID 逐帧指定 = can_id） */
    CAN_Pack_s pack = {.id = motor->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    RS_ControlFrame_u *cf = (RS_ControlFrame_u *)pack.data;
    cf->parts.p_des_be = (uint16_t)((p_des >> 8) | ((p_des & 0xFF) << 8));
    cf->parts.v_des_hi = (uint8_t)(v_des >> 4);
    cf->parts.v_des_lo_and_kp_hi = (uint8_t)(((v_des & 0xF) << 4) | ((kp >> 8) & 0xF));
    cf->parts.kp_lo = (uint8_t)(kp & 0xFF);
    cf->parts.kd_hi = (uint8_t)(kd >> 4);
    cf->parts.kd_lo_and_tff_hi = (uint8_t)(((kd & 0xF) << 4) | ((t_ff >> 8) & 0xF));
    cf->parts.tff_lo = (uint8_t)(t_ff & 0xFF);

    CANTransmit(motor->base.can, &pack, CAN_TRANSMIT_TIMEOUT, NULL, NULL);
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRV_RSMOTOR_USED */

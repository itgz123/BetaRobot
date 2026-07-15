/**
 * @file drv_dmmotor.c
 * @brief DM 电机驱动实现（4310 / 4310P）
 *
 * @note 协议：CAN MIT 模式，8字节帧，12位跨字节位域
 * @note 映射：无符号整数线性映射（非有符号2的补码）
 * @note 控制：上位机做 PID 级联控制（Kp=Kd=0），输出扭矩通过 t_ff 下发
 *
 * @note DM 与 DJI 的关键区别：
 *       - 每电机独立 CAN ID（无需分组）
 *       - 使能需发 0xFC 命令（通信丢失后需重发）
 *       - 输出是扭矩 (Nm)，非电流 (A)
 *       - 位置范围 [-p_max, p_max]（非 [0, 2π)）
 */

#include "drv_dmmotor.h"
#include "app_cfg.h"

#ifdef DRV_DMMOTOR_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_math.h"
#include <string.h>

#define CAN_TRANSMIT_TIMEOUT 1

/*============================================
 *              电机参数表（硬件极限）
 *============================================*/
/**
 * @brief DM 电机硬件参数
 * @note  4310: 扭矩 3.5Nm，转速 200RPM (20.944 rad/s)
 */
const DMMotorParams_s dm_motor_params[DM_MODEL_NUM] = {
    [DM_MODEL_DM4310] =
        {
            .v_max = 20.944f, /* 200 RPM × 2π/60 */
            .t_max = 3.5f,    /* 额定扭矩 Nm */
        },
};

/*============================================
 *              虚函数表实例
 *
 * @note  必须放在文件顶部（Register 函数中使用）
 *============================================*/
void DMMotor_Enable(void *inst);
void DMMotor_Disable(void *inst);
void DMMotor_SetRef(void *inst, float ref);
void DMMotor_Send(void *inst);
MotorData_s DMMotor_GetData(void *inst);
void DMMotor_SetOffset(void *inst, float offset);

const static MotorVTable_s s_dm_motor_vtable = {
    .enable = DMMotor_Enable,
    .disable = DMMotor_Disable,
    .set_ref = DMMotor_SetRef,
    .send = DMMotor_Send,
    .get_data = DMMotor_GetData,
    .set_offset = DMMotor_SetOffset,
    .send_cmd = DMMotor_SendModeCmd,
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
static inline float dm_uint_to_float(uint16_t uint_val, float scale, float offset)
{
    return (float)uint_val * scale + offset;
}

/**
 * @brief 浮点 → 无符号定点（热路径：每帧 1 次 / 电机）
 * @note  float→uint: (val + range_max) * inv_scale  (1 add + 1 mul)
 *        inv_scale = (2^bits - 1) / span 已在参数表预计算
 */
static inline uint16_t dm_float_to_uint(float float_val, float inv_scale, float range_max)
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
 * @brief 发送 DM 电机模式命令
 * @param inst DM 电机实例
 * @param cmd  命令码
 * @note  帧格式：前7字节 = 0xFF，第8字节 = 命令码
 */
void DMMotor_SendModeCmd(void *inst, uint8_t cmd)
{
    if (!inst)
        return;
    DMMotorInstance *motor = (DMMotorInstance *)inst;

    if (!motor->base.can)
        return;

    CANInstance *can = motor->base.can;
    memset(can->tx_buff, 0xFF, 7);
    can->tx_buff[7] = cmd;
    CANTransmit(can, CAN_TRANSMIT_TIMEOUT);
}

/*============================================
 *              CAN 接收回调（最小化 ISR 工作）
 *
 * 仅做：memcpy 原始 8 字节 + 记录时间戳 + flip 双缓冲 + 喂狗。
 * 所有数据处理（位域解析、SI 转换、多圈累加、速度滤波）移到 DMMotor_GetData。
 *============================================*/

/**
 * @brief DM 电机 CAN 反馈帧回调（ISR 上下文）
 * @param can CAN 实例指针
 *
 * 反馈帧格式（8字节）：
 *   D[0]：bits[3:0]=电机ID, bits[7:4]=错误状态
 *   D[1..2]：位置 16位 无符号 大端
 *   D[3]：速度高8位 VEL[11:4]
 *   D[4]：bits[7:4]=速度低4位 VEL[3:0], bits[3:0]=扭矩高4位 T[11:8]
 *   D[5]：扭矩低8位 T[7:0]
 *   D[6]：MOS温度 (°C)
 *   D[7]：线圈温度 (°C)
 */
static void DMMotorRxCallback(CANInstance *can)
{
    if (!can || !can->parent)
        return;

    DMMotorInstance *motor = (DMMotorInstance *)can->parent;

    /* 写入当前 ISR 缓冲区 */
    uint8_t idx = motor->base.raw_frame_idx;
    memcpy(motor->base.raw_frames[idx].bytes, can->rx_buff, 8);
    motor->base.raw_frames[idx].timestamp_us = DWT_GetTimeUs();

    /* flip 双缓冲索引（volatile 保证顺序） */
    motor->base.raw_frame_idx ^= 1u;

    /* 喂狗 */
    if (motor->base.daemon)
    {
        DaemonReload(motor->base.daemon);
    }
}

/*============================================
 *              统一数据获取接口 (DMMotor_GetData)
 *
 * 完整处理链：位域解析 → SI 转换 → 多圈累加 → 速度计算 → 滤波 → 偏置/方向
 *============================================*/

/**
 * @brief DM 电机统一数据获取
 * @param inst DMMotorInstance 指针
 * @return MotorData_s 包含所有反馈数据
 *
 * 处理顺序：
 *   1. 从双缓冲读取就绪帧
 *   2. 解析位域 → SI 转换
 *   3. dt 计算
 *   4. 多圈位置累加（wraps 检测）
 *   5. 速度计算（反馈/微分）
 *   6. 速度低通滤波
 *   7. 更新处理状态
 *   8. 应用偏置、方向修正、WRAP 归一化
 */
MotorData_s DMMotor_GetData(void *inst)
{
    MotorData_s result = {0};
    if (!inst)
        return result;

    DMMotorInstance *motor = (DMMotorInstance *)inst;
    MotorBase_s *base = &motor->base;

    /* ====== Step 1: 从双缓冲读取就绪帧 ====== */
    uint8_t ready_idx = !base->raw_frame_idx;
    MotorRawFrame_s frame = base->raw_frames[ready_idx];

    /* ====== Step 2: 解析位域 ====== */
    uint8_t error = frame.bytes[0] >> 4;
    uint16_t raw_position = ((uint16_t)frame.bytes[1] << 8) | frame.bytes[2];
    uint16_t raw_velocity = ((uint16_t)frame.bytes[3] << 4) | (frame.bytes[4] >> 4);
    uint16_t raw_torque = ((uint16_t)(frame.bytes[4] & 0x0F) << 8) | frame.bytes[5];
    int8_t raw_temperature_mos = (int8_t)frame.bytes[6];
    int8_t raw_temperature_coil = (int8_t)frame.bytes[7];

    DMModel_e model = base->model;
    if (model >= DM_MODEL_NUM)
        return result;

    const DMMotorProtocolMap_s *map = &motor->proto_map;
    MotorControllerSetting_s *setting = &base->setting;

    /* SI 转换 */
    float position_single = dm_uint_to_float(raw_position, map->pos_to_float_scale, -map->p_max);
    float velocity_raw_val = dm_uint_to_float(raw_velocity, map->vel_to_float_scale, -map->v_range);
    float torque = dm_uint_to_float(raw_torque, map->t_to_float_scale, -map->t_range);

    /* ====== Step 3: dt 计算 ====== */
    float dt = 0.0f;
    if (base->timestamp_last_us > 0 && frame.timestamp_us > base->timestamp_last_us)
    {
        dt = (float)(frame.timestamp_us - base->timestamp_last_us) * 1e-6f;
    }

    /* ====== Step 4: 多圈位置累加 ====== */

    /**
     * @note 多圈位置由上位机累加，不依赖电机自身。
     *       DM 电机位置范围由用户配置（p_max，最大 12.5 rad），
     *       单次"穿越"跨度 = 2 * p_max，最大可达 25 rad（约 4 个机械圈）。
     *       由于 DM 转速低（DM4310 空载 ~21 rad/s），穿越频率极低，
     *       连续丢帧导致多圈出错的概率远小于 DJI，几乎不受影响。
     */
    int8_t wraps = 0;
    if (base->timestamp_last_us > 0)
    {
        float angle_diff = position_single - base->position_single_last;
        if (dt > 0.0f)
        {
            /* 用速度反馈校验穿越 */
            float expected_change = velocity_raw_val * dt;
            float wrap_float = (expected_change - angle_diff) * (1.0f / (2.0f * map->p_max));
            wraps = (int8_t)(wrap_float > 0.0f ? wrap_float + 0.5f : wrap_float - 0.5f);
        }
        else
        {
            if (angle_diff > map->p_max)
                wraps = -1;
            else if (angle_diff < -map->p_max)
                wraps = 1;
        }
    }
    base->position_cnt += wraps;
    float position_multi =
        (float)base->position_cnt * (2.0f * map->p_max) + position_single;

    /* ====== Step 5: 速度计算（统一使用反馈速度） ====== */
    float speed;
    if (base->speed_lpf_enable == MOTOR_SPEED_LPF_ENABLE && dt > 0.0f)
    {
        float alpha = dt / (base->speed_lpf_rc + dt);
        speed = velocity_raw_val * alpha + base->speed_last * (1.0f - alpha);
    }
    else
    {
        speed = velocity_raw_val;
    }

    /* ====== Step 6: 更新处理状态 ====== */
    base->position_single_last = position_single;
    base->position_multi_last = position_multi;
    base->speed_last = speed;
    base->timestamp_last_us = frame.timestamp_us;

    /* ====== Step 7: 存储 DM 特有数据 ====== */
    motor->error = error;
    motor->temperature_mos = (float)raw_temperature_mos;
    motor->temperature_coil = (float)raw_temperature_coil;

    /* ====== Step 8: 构建返回值 ====== */
    result.position_single = position_single;

    float angle = position_multi + base->position_offset;
    if (setting->angle_src == MOTOR_FEEDBACK_EXTERNAL && setting->angle_external_ptr)
    {
        angle = *setting->angle_external_ptr;
    }
    else if (setting->position_mode == MOTOR_POSITION_WRAP)
    {
        angle = BSP_Math_WrapAngle(angle, setting->angle_limit_min, setting->angle_limit_max);
    }
    result.position = angle * setting->feedback_direction;

    if (setting->speed_src == MOTOR_FEEDBACK_EXTERNAL && setting->speed_external_ptr)
    {
        result.speed = *setting->speed_external_ptr * setting->feedback_direction;
    }
    else
    {
        result.speed = speed * setting->feedback_direction;
    }

    result.torque_current = torque * setting->feedback_direction;
    result.timestamp_us = frame.timestamp_us;

    /* ====== Step 9: 缓存到 base.data ====== */
    base->data = result;

    return result;
}

/*============================================
 *              Daemon 回调
 *============================================*/

/**
 * @brief DM 电机离线回调
 * @param owner 守护进程所有者 (DMMotorInstance*)
 * @note  DM 电机通信丢失后会自动退出 MIT 模式，
 *        因此除了复位 PID 外，还需重发 0xFC 使能命令。
 */
static void DMMotorDaemonCallback(void *owner)
{
    if (!owner)
        return;

    DMMotorInstance *motor = (DMMotorInstance *)owner;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);

    /* 重发使能命令，恢复 MIT 控制模式 */
    DMMotor_SendModeCmd(motor, DM_CMD_MOTOR_MODE);
}

/*============================================
 *              注册函数
 *============================================*/

int8_t DMMotorRegister(DMMotorInstance *inst, DMMotor_Init_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    /* 参数校验 */
    if (cfg->model >= DM_MODEL_NUM)
        return -1;

    /* --- 协议映射：校验用户配置 ≤ 硬件极限，计算 scale --- */
    const DMMotorParams_s *params = &dm_motor_params[cfg->model];
    DMMotorProtocolMap_s *map = &inst->proto_map;

    map->p_max = cfg->pos_max;
    map->v_range = BSP_Math_Clamp(cfg->vel_range, 0.0f, params->v_max);
    map->t_range = BSP_Math_Clamp(cfg->t_range, 0.0f, params->t_max);

    /* 防止零范围导致后续除法产生 inf */
    if (map->v_range < 1e-6f)
        map->v_range = 1e-6f;
    if (map->t_range < 1e-6f)
        map->t_range = 1e-6f;

    /* 计算预计算 scale 因子（运行时仅乘法，零除法） */
    map->pos_to_float_scale = (2.0f * map->p_max) / 65535.0f;
    map->vel_to_float_scale = (2.0f * map->v_range) / 4095.0f;
    map->vel_to_uint_scale = 4095.0f / (2.0f * map->v_range);
    map->t_to_float_scale = (2.0f * map->t_range) / 4095.0f;
    map->t_to_uint_scale = 4095.0f / (2.0f * map->t_range);

    /* 基本属性 */
    inst->base.brand = MOTOR_BRAND_DM;
    inst->base.model = cfg->model;
    inst->base.enable = MOTOR_DISABLE;
    inst->base.vtable = &s_dm_motor_vtable;
    inst->motor_can_id = cfg->motor_can_id;
    inst->master_id = cfg->master_id;

    /* 控制器设置 */
    inst->base.setting = cfg->controller_setting;

    /* 控制器状态 */
    inst->base.controller.ref = 0.0f;
    inst->base.controller.output = 0.0f;

    /* 初始化速度环 PID（输出限幅：用户配置的扭矩范围） */
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_SPEED)
    {
        cfg->pid_speed_setting.config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL | PID_ENABLE_OUTPUT_LIMIT;
        cfg->pid_speed_setting.out_max = map->t_range;
        cfg->pid_speed_setting.out_min = -map->t_range;

        PIDInit(&inst->base.controller.pid_speed, &cfg->pid_speed_setting);
    }

    /* 初始化位置环 PID（输出限幅：扭矩/速度） */
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_ANGLE)
    {
        cfg->pid_angle_setting.config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL | PID_ENABLE_OUTPUT_LIMIT;

        if (!(cfg->controller_setting.loop_type & MOTOR_LOOP_SPEED))
        {
            cfg->pid_angle_setting.out_max = map->t_range;
            cfg->pid_angle_setting.out_min = -map->t_range;
        }
        else
        {
            cfg->pid_angle_setting.out_max = map->v_range;
            cfg->pid_angle_setting.out_min = -map->v_range;
        }

        PIDInit(&inst->base.controller.pid_angle, &cfg->pid_angle_setting);
    }

    inst->base.speed_lpf_enable = cfg->speed_lpf_enable;
    inst->base.speed_lpf_rc = cfg->speed_lpf_rc;

    /* 双缓冲清零 */
    memset(inst->base.raw_frames, 0, sizeof(inst->base.raw_frames));
    inst->base.raw_frame_idx = 0;

    /* 处理状态清零 */
    inst->base.position_single_last = 0.0f;
    inst->base.position_multi_last = 0.0f;
    inst->base.position_cnt = 0;
    inst->base.speed_last = 0.0f;
    inst->base.timestamp_last_us = 0;
    inst->base.position_offset = 0.0f;
    memset(&inst->base.data, 0, sizeof(MotorData_s));

    /* 注册 CAN 实例 */
    if (inst->base.can)
    {
        CAN_Init_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .tx_id = cfg->motor_can_id,
            .filter_mode = CAN_FILTER_MODE_LIST,
            .rx_id_list = {cfg->master_id, CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED},
            .rx_mask = 0,
            .rx_callback = DMMotorRxCallback,
        };
        CANRegister(inst->base.can, &can_cfg);
        inst->base.can->parent = inst;
    }

    /* 注册守护进程 */
    if (inst->base.daemon)
    {
        Daemon_Init_Config_s config = {
            .callback = DMMotorDaemonCallback,
            .fault_action = cfg->fault_action,
            .owner_id = inst,
            .reload_count = cfg->reload_count};
        DaemonRegister(inst->base.daemon, &config);
    }

    return 0;
}

/*============================================
 *              PID 级联计算
 *============================================*/

/**
 * @brief 单个 DM 电机 PID 级联计算
 * @param inst DM 电机实例
 * @note  与 DJIMotor_Calculate 相同的级联逻辑：
 *        位置环（最外环）→ 速度环 → 扭矩输出
 *        开环模式：setpoint 直通，由下方限幅保护
 */
static void DMMotor_Calculate(DMMotorInstance *inst)
{
    if (!inst || !inst->base.enable)
        return;

    MotorControllerSetting_s *setting = &inst->base.setting;
    MotorController_s *ctrl = &inst->base.controller;

    float setpoint = ctrl->ref;
    float measure;
    float output = 0.0f;

    if (inst->base.model >= DM_MODEL_NUM)
        return;

    const DMMotorProtocolMap_s *map = &inst->proto_map;

    /* 位置环（最外环） */
    if (setting->loop_type & MOTOR_LOOP_ANGLE)
    {
        switch (setting->position_mode)
        {
        case MOTOR_POSITION_LIMITED:
            /* 限幅模式：setpoint 限幅到 [min, max] */
            if (setting->angle_limit_min < setting->angle_limit_max)
            {
                setpoint = BSP_Math_Clamp(setpoint, setting->angle_limit_min, setting->angle_limit_max);
            }
            break;
        case MOTOR_POSITION_WRAP:
            /* 环绕模式：setpoint 归一化到 [min, max) */
            setpoint = BSP_Math_WrapAngle(setpoint, setting->angle_limit_min, setting->angle_limit_max);
            break;
        case MOTOR_POSITION_CONTINUOUS:
        default:
            /* 连续模式：不限幅 */
            break;
        }

        float position_feedforward = 0.0f;
        if (setting->position_feedforward_src == MOTOR_FEEDFORWARD_EXTERNAL &&
            setting->position_feedforward_ptr)
        {
            position_feedforward = *setting->position_feedforward_ptr;
        }
        MotorData_s md = DMMotor_GetData(inst);
        measure = md.position;
        setpoint = PIDCalculate(&ctrl->pid_angle, setpoint, measure, position_feedforward);
    }

    /* 速度环 */
    if (setting->loop_type & MOTOR_LOOP_SPEED)
    {
        float speed_feedforward = 0.0f;
        if (setting->speed_feedforward_src == MOTOR_FEEDFORWARD_EXTERNAL &&
            setting->speed_feedforward_ptr)
        {
            speed_feedforward = *setting->speed_feedforward_ptr;
        }
        MotorData_s md = DMMotor_GetData(inst);
        measure = md.speed;
        output = PIDCalculate(&ctrl->pid_speed, setpoint, measure, speed_feedforward);
    }
    else
    {
        /* 开环模式：setpoint 直通作为扭矩输出 */
        output = setpoint;
    }

    /* 电机方向修正 */
    output *= setting->motor_direction;

    /* 扭矩限幅 (Nm) */
    output = BSP_Math_Clamp(output, -map->t_range, map->t_range);

    ctrl->output = output;
}

/*============================================
 *              虚函数实现
 *============================================*/

void DMMotor_Enable(void *inst)
{
    if (!inst)
        return;
    DMMotorInstance *motor = (DMMotorInstance *)inst;

    /* 先发送使能命令，再设置标志 */
    DMMotor_SendModeCmd(motor, DM_CMD_MOTOR_MODE);
    motor->base.enable = MOTOR_ENABLE;
}

void DMMotor_Disable(void *inst)
{
    if (!inst)
        return;
    DMMotorInstance *motor = (DMMotorInstance *)inst;

    /* 发送停止命令 + 清除使能标志 + 复位 PID */
    DMMotor_SendModeCmd(motor, DM_CMD_RESET_MODE);
    motor->base.enable = MOTOR_DISABLE;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);
}

/**
 * @brief 设置电机控制参考值
 * @param inst DMMotorInstance 指针
 * @param ref  参考值
 *
 * 控制模式说明：
 *   - MOTOR_LOOP_OPEN（扭矩开环）：ref = 扭矩(Nm) → CAN发送
 *   - MOTOR_LOOP_SPEED（速度环）：ref = 速度(rad/s) → PID(扭矩) → CAN发送
 *   - MOTOR_LOOP_ANGLE（位置环）：ref = 位置(rad) → PID(扭矩) → CAN发送
 *   - MOTOR_LOOP_ANGLE | MOTOR_LOOP_SPEED（位置-速度双环）：
 *       ref = 位置(rad) → PID(速度) → PID(扭矩) → CAN发送
 */
void DMMotor_SetRef(void *inst, float ref)
{
    if (!inst)
        return;
    DMMotorInstance *motor = (DMMotorInstance *)inst;
    motor->base.controller.ref = ref;
}

void DMMotor_SetOffset(void *inst, float offset)
{
    if (!inst)
        return;
    DMMotorInstance *motor = (DMMotorInstance *)inst;
    motor->base.position_offset = offset;
}

/**
 * @brief 发送 DM 电机控制帧
 * @param inst DMMotorInstance 指针
 *
 * 控制帧格式（MIT 模式，8字节）：
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
 * @note  DM 电机无需分组发送，每个电机独立发送一帧 CAN 报文。
 */
void DMMotor_Send(void *inst)
{
    if (!inst)
        return;
    DMMotorInstance *motor = (DMMotorInstance *)inst;

    if (!motor->base.can)
        return;

    CANInstance *can = motor->base.can;

    /* 失能时不发送控制帧 */
    if (!motor->base.enable)
        return;

    /* 控制计算 */
    DMMotor_Calculate(motor);

    /* 扭矩输出：浮点(Nm) → 12位无符号定点 */
    uint16_t p_des = 0; /* 不使用板载位置控制 */
    uint16_t v_des = 0; /* 不使用板载速度控制 */
    uint16_t kp = 0;    /* 不使用板载 PD */
    uint16_t kd = 0;    /* 不使用板载 PD */
    uint16_t t_ff = dm_float_to_uint(motor->base.controller.output,
                                     motor->proto_map.t_to_uint_scale,
                                     motor->proto_map.t_range);

    /* 打包控制帧 */
    uint8_t *buf = can->tx_buff;
    buf[0] = (uint8_t)(p_des >> 8);
    buf[1] = (uint8_t)(p_des & 0xFF);
    buf[2] = (uint8_t)(v_des >> 4);
    buf[3] = (uint8_t)(((v_des & 0xF) << 4) | ((kp >> 8) & 0xF));
    buf[4] = (uint8_t)(kp & 0xFF);
    buf[5] = (uint8_t)(kd >> 4);
    buf[6] = (uint8_t)(((kd & 0xF) << 4) | ((t_ff >> 8) & 0xF));
    buf[7] = (uint8_t)(t_ff & 0xFF);

    CANTransmit(can, CAN_TRANSMIT_TIMEOUT);
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRV_DMMOTOR_USED */

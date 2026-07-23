#include "drv_djimotor.h"
#include "app_cfg.h"

#ifdef DRV_DJIMOTOR_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_math.h"
#include <string.h>

#define CAN_TRANSMIT_TIMEOUT 1

/*
 * ============================================
 *              DJI 电机分组策略说明
 * ============================================
 *
 * 【问题背景】
 * DJI 电机通过 CAN 总线通信，一帧 CAN 数据最多携带 4 个电机的控制值。
 * 因此需要将电机分组，每组共用一帧发送报文。
 *
 * 【两种分组策略】
 *
 * 策略 A：按发送 ID 分组（参考代码采用）
 *   - 优点：发送简单，每组固定使用一个 tx_id
 *   - 缺点：M3508/M2006 ID 5-8 和 GM6020 ID 1-4 的 rx_id 重叠（都是 0x205-0x208）
 *           如果挂载在同一 CAN 总线上，会导致接收 ID 冲突
 *
 * 策略 B：按接收 ID 分组（本代码采用）✓
 *   - 优点：天然避免接收 ID 冲突，同一 CAN 总线上不会有两个电机响应同一个 rx_id
 *   - 缺点：同一组内可能有不同的 tx_id，需要在发送时额外处理
 *
 * 【本代码实现】
 * 采用策略 B，按接收 ID 分组：
 *   - Group 0: rx_id 0x201-0x204（M3508/M2006 ID 1-4，tx_id=0x200）
 *   - Group 1: rx_id 0x205-0x208（M3508/M2006 ID 5-8 tx_id=0x1FF，或 GM6020 ID 1-4 tx_id=0x1FE）
 *   - Group 2: rx_id 0x209-0x20B（GM6020 ID 5-7，tx_id=0x2FE）
 *
 * 发送处理：
 *   由于 Group 1 可能包含不同 tx_id 的电机，DJIMotorSend() 会检查组内电机的 tx_id，
 *   将相同 tx_id 的电机数据打包到同一帧发送，不同 tx_id 则分别发送多帧。
 *
 * 【关键约束】
 *   如果两个电机的 rx_id 不同，它们的 tx_id 必然不同，不会共用同一发送帧。
 *   因此按 rx_id 分组是安全的，只需在发送时处理 tx_id 差异。
 */

#define DJI_MOTOR_GROUP_0 0   // 接收ID: 0x201-0x204
#define DJI_MOTOR_GROUP_1 1   // 接收ID: 0x205-0x208
#define DJI_MOTOR_GROUP_2 2   // 接收ID: 0x209-0x20b
#define DJI_MOTOR_GROUP_NUM 3 // 3组电机

// 编码器分辨率 (14位)
#define DJI_ENCODER_RESOLUTION 8192

// 电调电流范围 (原始值)
#define C610_CURRENT_MAX 10000   // C610 (M2006) 电流范围 ±10000 → ±10A
#define C620_CURRENT_MAX 16384   // C620 (M3508) 电流范围 ±16384 → ±20A
#define GM6020_CURRENT_MAX 16384 // GM6020 电流范围 ±16384 → ±3A

// 电调电流范围 (安培)
#define C610_CURRENT_MAX_A 10.0f  // C610 (M2006) 最大电流 10A
#define C620_CURRENT_MAX_A 20.0f  // C620 (M3508) 最大电流 20A
#define GM6020_CURRENT_MAX_A 3.0f // GM6020 最大电流 3A

// 空载转速 (rad/s), rpm * 2π / 60
#define M3508_NO_LOAD_SPEED (9400.0f * M_2PI / 60.0f)  // ≈983.7 rad/s
#define M2006_NO_LOAD_SPEED (18000.0f * M_2PI / 60.0f) // ≈1884.0 rad/s
#define GM6020_NO_LOAD_SPEED (320.0f * M_2PI / 60.0f)  // ≈33.5 rad/s

/*============================================
 *              电机参数表定义
 *============================================*/
/* pos_scale = M_2PI / 8192 ≈ 0.000767, current_scale = 电流量程A / 电流量程raw */
const DJIMotorParams_s dji_motor_params[DJI_MODEL_NUM] = {
    [DJI_MODEL_M3508] = {
        .current_max = C620_CURRENT_MAX,
        .current_max_a = C620_CURRENT_MAX_A,
        .encoder_resolution = DJI_ENCODER_RESOLUTION,
        .no_load_speed = M3508_NO_LOAD_SPEED,
        .pos_scale = M_2PI / DJI_ENCODER_RESOLUTION,
        .current_scale = C620_CURRENT_MAX_A / C620_CURRENT_MAX,
    },
    [DJI_MODEL_M2006] = {
        .current_max = C610_CURRENT_MAX,
        .current_max_a = C610_CURRENT_MAX_A,
        .encoder_resolution = DJI_ENCODER_RESOLUTION,
        .no_load_speed = M2006_NO_LOAD_SPEED,
        .pos_scale = M_2PI / DJI_ENCODER_RESOLUTION,
        .current_scale = C610_CURRENT_MAX_A / C610_CURRENT_MAX,
    },
    [DJI_MODEL_GM6020] = {
        .current_max = GM6020_CURRENT_MAX,
        .current_max_a = GM6020_CURRENT_MAX_A,
        .encoder_resolution = DJI_ENCODER_RESOLUTION,
        .no_load_speed = GM6020_NO_LOAD_SPEED,
        .pos_scale = M_2PI / DJI_ENCODER_RESOLUTION,
        .current_scale = GM6020_CURRENT_MAX_A / GM6020_CURRENT_MAX,
    },
};

const uint16_t can_tx_id[DJI_MODEL_NUM][2] = {
    [DJI_MODEL_M3508][0] = 0x200,  // 1-4号
    [DJI_MODEL_M3508][1] = 0x1ff,  // 5-8号
    [DJI_MODEL_M2006][0] = 0x200,  // 1-4号
    [DJI_MODEL_M2006][1] = 0x1ff,  // 5-8号
    [DJI_MODEL_GM6020][0] = 0x1fe, // 1-4号
    [DJI_MODEL_GM6020][1] = 0x2fe, // 5-7号
};

const uint16_t can_rx_id_base[DJI_MODEL_NUM] = {
    [DJI_MODEL_M3508] = 0x200,
    [DJI_MODEL_M2006] = 0x200,
    [DJI_MODEL_GM6020] = 0x204,
};

static DJIMotorSendGroup_s s_send_groups[CAN_NUM_MAX][DJI_MOTOR_GROUP_NUM] = {0};

/*============================================
 *              虚函数表实例
 *============================================*/
void DJIMotor_Enable(void *inst);
void DJIMotor_Disable(void *inst);
void DJIMotor_SetRef(void *inst, float ref);
void DJIMotor_Send(void *inst);
MotorData_s DJIMotor_GetData(void *inst);
void DJIMotor_SetOffset(void *inst, float offset);

const static MotorVTable_s s_dji_motor_vtable = {
    .enable = DJIMotor_Enable,
    .disable = DJIMotor_Disable,
    .set_ref = DJIMotor_SetRef,
    .send = DJIMotor_Send,
    .get_data = DJIMotor_GetData,
    .set_offset = DJIMotor_SetOffset,
    .send_cmd = NULL, /* DJI 电机无需模式命令 */
};

/**
 * @brief DJI电机接收回调函数（ISR 上下文 — 最小化工作）
 * @param can CAN实例指针
 * @note 仅 memcpy 原始字节 + 时间戳 + flip 双缓冲 + 喂狗。
 *       所有数据处理移到 DJIMotor_GetData。
 */
static void DJIMotorRxCallback(CANInstance *can)
{
    if (!can || !can->parent)
        return;

    DJIMotorInstance *motor = (DJIMotorInstance *)can->parent;

    /* 写入当前 ISR 缓冲区 */
    uint8_t idx = motor->base.raw_frame_idx;
    memcpy(motor->base.raw_frames[idx].bytes, can->rx_buff, 8);
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
 *              统一数据获取接口 (DJIMotor_GetData)
 *
 * 完整处理链：位域解析 → SI 转换 → 多圈累加 → 速度计算 → 滤波 → 偏置/方向
 *============================================*/

/**
 * @brief DJI 电机统一数据获取
 * @param inst DJIMotorInstance 指针
 * @return MotorData_s 包含所有反馈数据
 */
MotorData_s DJIMotor_GetData(void *inst)
{
    MotorData_s result = {0};
    if (!inst)
        return result;

    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    MotorBase_s *base = &motor->base;

    /* 缓存命中：上次获取后没有新中断，直接返回缓存 */
    if (base->data_valid)
        return base->data;

    /* ====== Step 1: 从双缓冲读取就绪帧 ====== */
    uint8_t ready_idx = !base->raw_frame_idx;
    MotorRawFrame_s frame = base->raw_frames[ready_idx];

    /* ====== Step 2: 解析位域 + SI 转换 ====== */
    DJIMotorCanFrame_u can_frame;
    memcpy(can_frame.raw, frame.bytes, 8);

    DJIModel_e model = base->model;
    if (model >= DJI_MODEL_NUM)
        return result;

    const DJIMotorParams_s *params = &dji_motor_params[model];
    MotorControllerSetting_s *setting = &base->setting;

    uint16_t raw_encoder = ((uint16_t)can_frame.rx.encoder_h << 8) | can_frame.rx.encoder_l;
    int16_t raw_velocity = ((int16_t)can_frame.rx.velocity_h << 8) | can_frame.rx.velocity_l;
    int16_t raw_current = ((int16_t)can_frame.rx.current_h << 8) | can_frame.rx.current_l;

    // 单圈位置 (rad) [0, 2π) — 乘法代替除法
    float position_single = (float)raw_encoder * params->pos_scale;
    // 速度 (rad/s) — 乘法代替除法
    float velocity_raw = (float)raw_velocity * RPM_SCALE;
    // 电流 (A) — 乘法代替除法
    float current = (float)raw_current * params->current_scale;

    /* ====== Step 3: dt 计算 ====== */
    float dt = 0.0f;
    if (base->timestamp_last_us > 0 && frame.timestamp_us > base->timestamp_last_us)
    {
        dt = (float)(frame.timestamp_us - base->timestamp_last_us) * 1e-6f;
    }

    /* ====== Step 4: 多圈位置累加 ====== */

    /**
     * @note 多圈位置由上位机累加，不依赖电机自身。
     *       编码器分辨率 14 位（8192），单圈范围 [0, 2π)。
     *       DJI 电机（M2006/M3508）转速极高（空载 1884/984 rad/s），
     *       如果在高转速下 CAN 连续丢帧，可能导致多圈计算错误。
     *       尤其是 GetData 调用频率低于 CAN 回报率（如 500Hz < 1kHz）时风险增加。
     *       受限于 daemon 超时（通常 ≤ 10ms），实际丢帧不会超过 ~10 帧，
     *       此时速度校验仍能正确检测穿越，daemon 触发后 PID 复位、位置重建。
     */
    int8_t wraps = 0;
    if (base->timestamp_last_us > 0)
    {
        float angle_diff = position_single - base->position_single_last;
        if (dt > 0.0f)
        {
            float expected_change = velocity_raw * dt;
            float wrap_float = (expected_change - angle_diff) * INV_M_2PI;
            wraps = (int8_t)(wrap_float > 0.0f ? wrap_float + 0.5f : wrap_float - 0.5f);
        }
        else
        {
            if (angle_diff > M_PI)
                wraps = -1;
            else if (angle_diff < -M_PI)
                wraps = 1;
        }
    }
    base->position_cnt += wraps;
    float position_multi = (float)base->position_cnt * M_2PI + position_single;

    /* ====== Step 5: 速度计算（统一使用反馈速度）+ 低通滤波 ====== */
    float speed;
    if (base->speed_lpf_enable == MOTOR_SPEED_LPF_ENABLE && dt > 0.0f)
    {
        float alpha = dt / (base->speed_lpf_rc + dt);
        speed = velocity_raw * alpha + base->speed_last * (1.0f - alpha);
    }
    else
    {
        speed = velocity_raw;
    }

    /* ====== Step 6: 更新处理状态 ====== */
    base->position_single_last = position_single;
    base->position_multi_last = position_multi;
    base->speed_last = speed;
    base->timestamp_last_us = frame.timestamp_us;

    /* ====== Step 7: 存储 DJI 特有数据 ====== */
    motor->motor_temperature = (float)can_frame.rx.temperature;
    motor->error_code = can_frame.rx.error_code;

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

    result.torque_current = current * setting->feedback_direction;
    result.timestamp_us = frame.timestamp_us;

    /* ====== Step 9: 缓存到 base.data ====== */
    base->data = result;
    base->data_valid = 1;

    return result;
}

/**
 * @brief DJI电机守护进程回调函数
 * @param owner 守护进程所有者指针 (DJIMotorInstance*)
 * @note 电机离线时调用，清空 PID 状态避免积分累积
 */
static void DJIMotorDaemonCallback(void *owner)
{
    if (!owner)
        return;

    DJIMotorInstance *motor = (DJIMotorInstance *)owner;
    PIDReset(&motor->base.controller.pid_speed);
    PIDReset(&motor->base.controller.pid_angle);
}

/**
 * @brief 配置DJI电机实例（可重复调用，不修改 static 变量）
 * @note 可运行时重新调用以修改 PID 参数、控制器设置等
 */
int8_t DJIMotorConfig(DJIMotorInstance *inst, DJIMotor_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    // 检查参数有效性
    if (cfg->motor_id < 1 || cfg->motor_id > 8)
        return -1;
    if (cfg->model >= DJI_MODEL_NUM)
        return -1;
    // GM6020 只有 ID 1-7
    if (cfg->model == DJI_MODEL_GM6020 && cfg->motor_id > 7)
        return -1;

    // 计算分组索引（用于保存 sender_group 指针）
    uint16_t rx_id = can_rx_id_base[cfg->model] + cfg->motor_id;
    uint8_t group_idx = (rx_id - 0x201) / 4;
    uint8_t motor_idx_in_group = (rx_id - 0x201) % 4;
    BoardCAN_e can_e = cfg->can_e;

    // 初始化基本属性
    inst->base.brand = MOTOR_BRAND_DJI;
    inst->base.model = cfg->model;
    inst->motor_id = cfg->motor_id;
    inst->base.enable = MOTOR_DISABLE;
    inst->base.vtable = &s_dji_motor_vtable;

    // 初始化控制器设置
    inst->base.setting = cfg->controller_setting;

    // 初始化控制器状态
    inst->base.controller.ref = 0.0f;
    inst->base.controller.output = 0.0f;

    // 获取输出限幅
    float current_max_a = dji_motor_params[cfg->model].current_max_a;
    float speed_max = dji_motor_params[cfg->model].no_load_speed;

    // 初始化速度环 PID (输出限幅: 电流安培值)
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_SPEED)
    {
        // 添加固定掩码
        cfg->pid_speed_setting.config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL | PID_ENABLE_OUTPUT_LIMIT;
        // 设置输出限幅
        cfg->pid_speed_setting.out_max = current_max_a;
        cfg->pid_speed_setting.out_min = -current_max_a;

        PIDInit(&inst->base.controller.pid_speed, &cfg->pid_speed_setting);
    }

    // 初始化位置环 PID (输出限幅: 空载速度 rad/s)。这里限幅是速度，不要直接把位置环输出给电流
    if (cfg->controller_setting.loop_type & MOTOR_LOOP_ANGLE)
    {
        // 添加固定掩码
        cfg->pid_angle_setting.config_mask |= PID_ENABLE_TRAPEZOID_INTEGRAL | PID_ENABLE_OUTPUT_LIMIT;

        // 仅位置环模式：输出限幅为电流；位置-速度双环模式：输出限幅为速度
        if (!(cfg->controller_setting.loop_type & MOTOR_LOOP_SPEED))
        {
            cfg->pid_angle_setting.out_max = current_max_a;
            cfg->pid_angle_setting.out_min = -current_max_a;
        }
        else
        {
            cfg->pid_angle_setting.out_max = speed_max;
            cfg->pid_angle_setting.out_min = -speed_max;
        }

        PIDInit(&inst->base.controller.pid_angle, &cfg->pid_angle_setting);
    }

    inst->base.speed_lpf_enable = cfg->speed_lpf_enable;
    inst->base.speed_lpf_rc = cfg->speed_lpf_rc;

    // 双缓冲清零
    memset(inst->base.raw_frames, 0, sizeof(inst->base.raw_frames));
    inst->base.raw_frame_idx = 0;
    inst->base.data_valid = 0;

    // 处理状态清零
    inst->base.position_single_last = 0.0f;
    inst->base.position_multi_last = 0.0f;
    inst->base.position_cnt = 0;
    inst->base.speed_last = 0.0f;
    inst->base.timestamp_last_us = 0;
    inst->base.position_offset = 0.0f;
    memset(&inst->base.data, 0, sizeof(MotorData_s));

    // 保存分组信息
    inst->sender_group = &s_send_groups[can_e][group_idx];
    inst->motor_idx_in_group = motor_idx_in_group;

    return 0;
}

int8_t DJIMotorRegister(DJIMotorInstance *inst, const DJIMotor_Register_Config_s *reg_cfg)
{
    if (!inst || !reg_cfg)
        return -1;

    DJIMotor_Config_s *cfg = (DJIMotor_Config_s *)&reg_cfg->motor_config;

    // 检查参数有效性
    if (cfg->motor_id < 1 || cfg->motor_id > 8)
        return -1;
    if (cfg->model >= DJI_MODEL_NUM)
        return -1;
    // GM6020 只有 ID 1-7
    if (cfg->model == DJI_MODEL_GM6020 && cfg->motor_id > 7)
        return -1;

    // 计算发送ID和接收ID（需在 Config 调用前完成，用于防重复检查）
    uint16_t tx_id = can_tx_id[cfg->model][(cfg->motor_id - 1) / 4];
    uint16_t rx_id = can_rx_id_base[cfg->model] + cfg->motor_id;
    uint8_t group_idx = (rx_id - 0x201) / 4;
    uint8_t motor_idx_in_group = (rx_id - 0x201) % 4;
    BoardCAN_e can_e = cfg->can_e;

    // 检查是否已经初始化
    if (1 == s_send_groups[can_e][group_idx].motor_init_flag[motor_idx_in_group])
        return -1;

    // 调用 Config 完成实例配置
    if (DJIMotorConfig(inst, cfg) != 0)
    {
        return -1;
    }

    // 注册到发送分组
    s_send_groups[can_e][group_idx].motors[motor_idx_in_group] = inst;
    s_send_groups[can_e][group_idx].motor_init_flag[motor_idx_in_group] = 1;

    // 注册CAN实例
    if (inst->base.can)
    {
        CAN_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .tx_id = tx_id,
            .filter_mode = CAN_FILTER_MODE_LIST,
            .rx_id_list = {rx_id, CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED},
            .rx_mask = 0,
            .rx_callback = DJIMotorRxCallback,
        };
        CANRegister(inst->base.can, &can_cfg);
        inst->base.can->parent = inst;
    }

    // 注册守护进程
    if (inst->base.daemon)
    {
        Daemon_Config_s config = {
            .callback = DJIMotorDaemonCallback,
            .fault_action = reg_cfg->fault_action,
            .owner_id = inst,
            .reload_count = reg_cfg->reload_count};
        DaemonRegister(inst->base.daemon, &config);
    }

    return 0;
}

/**
 * @brief 单个电机控制计算
 */
static void DJIMotor_Calculate(DJIMotorInstance *inst)
{
    if (!inst || !inst->base.enable)
        return;

    MotorControllerSetting_s *setting = &inst->base.setting;
    MotorController_s *ctrl = &inst->base.controller;

    float setpoint = ctrl->ref;
    float measure;
    float output = 0.0f;

    DJIModel_e model = inst->base.model;
    if (model >= DJI_MODEL_NUM)
        return;

    /* 统一获取一次反馈数据（后续 GetData 走缓存，不重复解析） */
    MotorData_s md = DJIMotor_GetData(inst);

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
        if (setting->position_feedforward_src == MOTOR_FEEDFORWARD_EXTERNAL &&
            setting->position_feedforward_ptr)
        {
            position_feedforward = *setting->position_feedforward_ptr;
        }
        measure = md.position;
        setpoint = PIDCalculate(&ctrl->pid_angle, setpoint, measure, position_feedforward);
    }

    // 速度环
    if (setting->loop_type & MOTOR_LOOP_SPEED)
    {
        float speed_feedforward = 0.0f;
        if (setting->speed_feedforward_src == MOTOR_FEEDFORWARD_EXTERNAL &&
            setting->speed_feedforward_ptr)
        {
            speed_feedforward = *setting->speed_feedforward_ptr;
        }
        measure = md.speed;
        output = PIDCalculate(&ctrl->pid_speed, setpoint, measure, speed_feedforward);
    }
    else
    {
        // 开环模式 (MOTOR_LOOP_OPEN): setpoint 直接作为电流输出，依赖下方电流限幅保护
        // 仅位置环模式 (MOTOR_LOOP_ANGLE): setpoint 是位置环 PID 输出，已在 PID 初始化时限幅为电流范围
        output = setpoint;
    }

    // 电机方向修正: motor_direction修正电机安装方向, feedback_direction已在反馈端修正
    output *= setting->motor_direction;

    // 电流限幅 (安培) - 开环模式必须限幅，其他模式作为保险
    float current_max_a = dji_motor_params[model].current_max_a;
    output = BSP_Math_Clamp(output, -current_max_a, current_max_a);

    // 转换为 CAN 发送原始值 (安培 -> 原始值)
    uint16_t current_max = dji_motor_params[model].current_max;
    ctrl->output = output / current_max_a * (float)current_max;
}

/*============================================
 *              虚函数实现
 *============================================*/
void DJIMotor_Enable(void *inst)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    motor->base.enable = MOTOR_ENABLE;
}

void DJIMotor_Disable(void *inst)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    motor->base.enable = MOTOR_DISABLE;
}

/**
 * @brief 设置电机控制参考值
 * @param inst DJIMotorInstance 指针
 * @param ref 参考值
 *
 * 方向标定流程:
 *   1. 人为设定正方向（顺时针或逆时针）
 *   2. 开环控制，发送正的较小电流值，观察实际旋转方向和反馈方向
 *   3. 如果实际旋转方向与正方向相反，设置 motor_direction = MOTOR_DIRECTION_REVERSE
 *   4. 如果反馈正负与实际旋转方向相反，设置 feedback_direction = MOTOR_DIRECTION_REVERSE
 *
 * 方向处理逻辑:
 *   - motor_direction: 修正电机安装方向，在输出端翻转电流方向
 *   - feedback_direction: 修正反馈极性，在反馈获取函数中翻转反馈值符号
 *   - PID 计算在统一的坐标系下进行，setpoint 和 measure 都已正确处理方向
 *
 * 控制模式说明:
 *   - MOTOR_LOOP_OPEN (电流开环): ref = 电流(A) → 原始值 → CAN发送
 *   - MOTOR_LOOP_SPEED (速度环): ref = 速度(rad/s) → PID(电流) → 原始值 → CAN发送
 *   - MOTOR_LOOP_ANGLE (位置环): ref = 位置(rad) → PID(电流) → 原始值 → CAN发送
 *   - MOTOR_LOOP_ANGLE | MOTOR_LOOP_SPEED (位置-速度双环):
 *       ref = 位置(rad) → PID(速度) → PID(电流) → 原始值 → CAN发送
 *
 * 最终电流统一限幅到 ±current_max_a (安培) 再转换为 CAN 原始值
 */
void DJIMotor_SetRef(void *inst, float ref)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    motor->base.controller.ref = ref;
}

/**
 * @brief 设置电机位置偏置
 * @param inst DJIMotorInstance 指针
 * @param offset 位置偏置值 (rad)
 * @note 增量编码器置零：MotorSetOffset(&motor, -motor.data.position_multi)
 *       绝对式编码器设偏置：MotorSetOffset(&motor, fixed_offset)
 */
void DJIMotor_SetOffset(void *inst, float offset)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    motor->base.position_offset = offset;
}

void DJIMotor_Send(void *inst)
{
    if (!inst)
        return;
    DJIMotorInstance *motor = (DJIMotorInstance *)inst;

    if (!motor->sender_group || !motor->base.can)
        return;

    DJIMotorSendGroup_s *group = motor->sender_group;

    // ===== 控制计算：组内所有已初始化电机 =====
    for (int i = 0; i < 4; i++)
    {
        if (group->motor_init_flag[i] && group->motors[i])
        {
            DJIMotor_Calculate(group->motors[i]);
        }
    }

    // ===== 按 tx_id 分帧发送 =====
    // 由于按 rx_id 分组，同一组内可能存在不同 tx_id 的电机
    // 例如 Group 1 (rx_id 0x205-0x208) 可能包含：
    //   - M3508/M2006 ID 5-8 (tx_id=0x1FF)
    //   - GM6020 ID 1-4 (tx_id=0x1FE)
    // 因此需要收集组内所有不同的 tx_id，分别打包发送
    // 发送时使用组内对应 tx_id 的电机的 CAN 实例，避免修改 tx_id

    // Step 1: 收集组内不同的 tx_id 及对应的 CAN 实例
    uint16_t tx_ids[4] = {0};
    CANInstance *tx_cans[4] = {NULL};
    uint8_t tx_id_count = 0;

    for (int i = 0; i < 4; i++)
    {
        if (group->motor_init_flag[i] && group->motors[i] && group->motors[i]->base.can)
        {
            uint16_t id = group->motors[i]->base.can->tx_id;
            uint8_t found = 0;
            for (int j = 0; j < tx_id_count; j++)
            {
                if (tx_ids[j] == id)
                {
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                tx_ids[tx_id_count] = id;
                tx_cans[tx_id_count] = group->motors[i]->base.can;
                tx_id_count++;
            }
        }
    }

    // Step 2: 按 tx_id 分别打包发送
    // 每个 tx_id 发送一帧 CAN 报文，包含对应的 4 个电机位置（部分可能为空）
    for (int t = 0; t < tx_id_count; t++)
    {
        uint16_t current_tx_id = tx_ids[t];
        CANInstance *tx_can = tx_cans[t];
        DJIMotorCanFrame_u *frame = (DJIMotorCanFrame_u *)tx_can->tx_buff;

        for (int i = 0; i < 4; i++)
        {
            int16_t cur = 0;
            DJIMotorInstance *m = group->motors[i];
            // 只有匹配当前 tx_id 的电机才填充数据
            if (group->motor_init_flag[i] && m && m->base.enable &&
                m->base.can && m->base.can->tx_id == current_tx_id)
            {
                // 根据电机型号限幅到电流原始值范围
                uint16_t current_max = dji_motor_params[m->base.model].current_max;
                float out = BSP_Math_Clamp(m->base.controller.output, -(float)current_max, (float)current_max);
                cur = (int16_t)out;
            }
            // CAN 总线需要大端字节序（MSB first）
            frame->raw[i * 2] = (uint8_t)(cur >> 8);
            frame->raw[i * 2 + 1] = (uint8_t)(cur & 0xFF);
        }

        CANTransmit(tx_can, CAN_TRANSMIT_TIMEOUT);
    }
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRV_DJIMOTOR_USED */

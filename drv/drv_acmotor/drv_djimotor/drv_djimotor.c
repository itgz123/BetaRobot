/**
 * @file drv_djimotor.c
 * @brief DJI 智能电机驱动实现
 *
 * @note 数据流：
 *       CAN中断 → DJIMotorDecode → raw_data + data（含滤波+多圈）
 *       APP任务 → DJIMotorSend → ApplyLimits + 打包CAN + 发送
 */

#include "drv_djimotor.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"

/*============================================
 *              内部变量
 *============================================*/

// 共享发送 CAN 实例（6组：CAN1/CAN2 各 3 个发送 ID）
// 0: CAN1 0x1FF, 1: CAN1 0x200, 2: CAN1 0x2FF
// 3: CAN2 0x1FF, 4: CAN2 0x200, 5: CAN2 0x2FF
static CANInstance sender_group[6] = {0};
static uint8_t sender_init_flag[6] = {0};

/*============================================
 *              内部函数声明
 *============================================*/

// 单电机虚函数实现
static int8_t DJIMotorRegister(ACMotorInstance *inst, ACMotor_Init_Config_s *cfg);
static void DJIMotorEnable(ACMotorInstance *inst);
static void DJIMotorDisable(ACMotorInstance *inst);
static void DJIMotorSetRef(ACMotorInstance *inst, float ref);
static void DJIMotorSend(ACMotorInstance *inst);

// 内部辅助函数
static void DJIMotorDecode(CANInstance *can_inst);
static uint8_t DJIMotorGetSenderGroup(BoardCAN_e can_e, uint8_t motor_id, DJIModel_e model);
static CANInstance *DJIMotorGetOrCreateSender(uint8_t group);

/*============================================
 *              虚函数表定义（导出）
 *============================================*/

const ACMotorInterface_s djimotor_vtable = {
    .register_ = DJIMotorRegister,
    .enable = DJIMotorEnable,
    .disable = DJIMotorDisable,
    .set_ref = DJIMotorSetRef,
    .send = DJIMotorSend,
};

/*============================================
 *              单电机虚函数实现
 *============================================*/

static int8_t DJIMotorRegister(ACMotorInstance *inst, ACMotor_Init_Config_s *cfg)
{
    if (!inst || !cfg)
        return -1;

    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    DJIMotorPriv_t *priv = &motor->priv;

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

    // 设置 parent 指针
    priv->rx_can->parent = motor;

    // 同步基类 can 指针
    inst->can = priv->rx_can;

    // 记录 motor_id
    priv->motor_id = cfg->motor_id;

    // 计算接收 ID
    DJIModel_e model = (DJIModel_e)inst->model;
    uint32_t rx_id = (model == DJI_MODEL_GM6020) ? (0x204 + priv->motor_id) : (0x200 + priv->motor_id);

    CAN_Init_Config_s can_cfg = {
        .tx_id = CAN_ID_UNUSED,
        .filter_mode = CAN_FILTER_MODE_LIST,
        .rx_id_count = 1,
        .rx_id_list = {rx_id, CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED},
        .rx_mask = 0,
        .rx_callback = DJIMotorDecode,
    };

    // 注册接收 CAN 实例
    if (CANRegister(priv->rx_can, &can_cfg) != 0)
        return -1;

    // 获取 CAN 总线
    BoardCAN_e can_e = priv->rx_can->can_e;

    // 计算发送分组
    uint8_t group = DJIMotorGetSenderGroup(can_e, priv->motor_id, model);

    // 获取或创建发送 CAN 实例
    priv->tx_can = DJIMotorGetOrCreateSender(group);

    // 计算发送槽位
    priv->tx_slot = (priv->motor_id <= 4) ? (priv->motor_id - 1) : (priv->motor_id - 5);

    // 注册 daemon
    if (inst->daemon)
    {
        Daemon_Init_Config_s daemon_cfg = {
            .reload_count = cfg->daemon_reload,
            .fault_action = cfg->daemon_fault_action,
            .callback = cfg->lost_callback,
            .owner_id = motor,
        };
        DaemonRegister(inst->daemon, &daemon_cfg);
    }

    // 初始化完成
    inst->enable = 1;
    inst->data.fresh = 0;

    return 0;
}

static void DJIMotorEnable(ACMotorInstance *inst)
{
    if (inst)
        inst->enable = 1;
}

static void DJIMotorDisable(ACMotorInstance *inst)
{
    if (inst)
        inst->enable = 0;
}

static void DJIMotorSetRef(ACMotorInstance *inst, float ref)
{
    if (inst)
        inst->controller.pid_ref = ref;
}

/**
 * @brief 单电机发送：限幅 + 打包 + 发送
 */
static void DJIMotorSend(ACMotorInstance *inst)
{
    if (!inst || !inst->enable)
        return;

    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    DJIMotorPriv_t *priv = &motor->priv;

    if (!priv->tx_can)
        return;

    // 应用限制
    ACMotorApplyLimits(inst);

    // 打包电流值到共享发送缓冲区
    int16_t set = (int16_t)inst->controller.pid_ref;
    uint8_t slot = priv->tx_slot;
    priv->tx_can->tx_buff[2 * slot] = (uint8_t)(set >> 8);
    priv->tx_can->tx_buff[2 * slot + 1] = (uint8_t)(set & 0xFF);

    // 立即发送
    CANTransmit(priv->tx_can, 1);
}

/*============================================
 *              内部辅助函数实现
 *============================================*/

/**
 * @brief DJI CAN 接收解码（中断上下文）
 * @note 填入 raw_data 并转换为 data（单位转换+低通滤波+多圈累积）
 */
static void DJIMotorDecode(CANInstance *can_inst)
{
    DJIMotorInstance *motor = (DJIMotorInstance *)can_inst->parent;
    if (!motor)
        return;

    ACMotorInstance *base = &motor->base;
    DJIMotorPriv_t *priv = &motor->priv;
    uint8_t *rx = can_inst->rx_buff;

    // 喂狗
    if (base->daemon)
        DaemonReload(base->daemon);

    // 计算 dt
    uint64_t now = DWT_GetTimeUs();
    base->dt = (float)(now - priv->feed_cnt) * 1e-6f;
    priv->feed_cnt = now;

    // ---- 1. 填 raw_data ----
    MotorRawData_t *raw = &base->raw_data;
    raw->raw_encoder = ((uint16_t)rx[0] << 8) | rx[1];   // ecd 0-8191
    raw->raw_velocity = (int16_t)((rx[2] << 8) | rx[3]); // rpm
    raw->raw_current = (int16_t)((rx[4] << 8) | rx[5]);  // 电流原始值
    raw->raw_temperature_motor = (int8_t)rx[6];                // °C
    raw->raw_temperature_mos = 0;                           // DJI 无 MOS 温度
    raw->error_code = rx[7];                             // 错误码
    raw->fresh = 1;

    // ---- 2. 多圈累积（ecd 越界检测） ----
    uint16_t last_ecd = priv->last_ecd;
    priv->last_ecd = raw->raw_encoder;
    if (raw->raw_encoder - last_ecd > 4096)
        priv->total_round--;
    else if (raw->raw_encoder - last_ecd < -4096)
        priv->total_round++;

    // ---- 3. 单位转换 → data ----
    MotorData_t *d = &base->data;
    float ratio = base->reduction_ratio;

    // 单圈位置 (rad) = ecd * (360/8192) * (pi/180) / ratio
    d->position_single = DJI_ECD_ANGLE_COEF * (float)raw->raw_encoder * DJI_DEG_TO_RAD / ratio;

    // 多圈位置 (rad)
    d->position_multi = (priv->total_round * 360.0f + DJI_ECD_ANGLE_COEF * (float)raw->raw_encoder) * DJI_DEG_TO_RAD / ratio;

    // 速度 RPM→rad/s，低通滤波
    float speed_raw = (float)raw->raw_velocity * DJI_RPM_TO_RADPS / ratio;
    d->speed = DJI_SPEED_LPF_ALPHA * speed_raw + (1.0f - DJI_SPEED_LPF_ALPHA) * d->speed;

    // 电流 原始值→A，低通滤波
    float cur_raw = (float)raw->raw_current * DJI_CURRENT_COEF;
    d->current = DJI_CURRENT_LPF_ALPHA * cur_raw + (1.0f - DJI_CURRENT_LPF_ALPHA) * d->current;

    // 力矩 = 电流 * 扭矩系数 * 减速比
    d->torque = d->current * base->torque_coef * ratio;

    // 温度
    d->temperature = (float)raw->raw_temperature_motor;
    d->temperature_mos = 0.0f;

    // 错误标志
    d->error_flags = raw->error_code;

    d->fresh = 1;
}

/**
 * @brief 获取发送分组索引
 */
static uint8_t DJIMotorGetSenderGroup(BoardCAN_e can_e, uint8_t motor_id, DJIModel_e model)
{
    uint8_t can_offset = (can_e == CAN_1) ? 0 : 3;

    switch (model)
    {
    case DJI_MODEL_M3508:
    case DJI_MODEL_M2006:
        if (motor_id <= 4)
            return can_offset + 1; // 0x200
        else
            return can_offset + 0; // 0x1FF

    case DJI_MODEL_GM6020:
        if (motor_id <= 4)
            return can_offset + 0; // 0x1FF
        else
            return can_offset + 2; // 0x2FF

    default:
        return can_offset + 0;
    }
}

/**
 * @brief 获取或创建发送 CAN 实例
 */
static CANInstance *DJIMotorGetOrCreateSender(uint8_t group)
{
    if (sender_init_flag[group])
        return &sender_group[group];

    uint32_t tx_id = 0x1FF;
    BoardCAN_e can_e = (group < 3) ? CAN_1 : CAN_2;

    switch (group % 3)
    {
    case 0:
        tx_id = 0x1FF;
        break;
    case 1:
        tx_id = 0x200;
        break;
    case 2:
        tx_id = 0x2FF;
        break;
    }

    sender_group[group].can_e = can_e;

    CAN_Init_Config_s snd_cfg = {
        .tx_id = tx_id,
        .filter_mode = CAN_FILTER_MODE_MASK,
        .rx_id_count = 0,
        .rx_id_list = {CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED},
        .rx_mask = 0,
        .rx_callback = NULL,
    };

    CANRegister(&sender_group[group], &snd_cfg);
    sender_init_flag[group] = 1;

    return &sender_group[group];
}

#endif // BSP_CAN_MODULE_ENABLED
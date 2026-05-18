/**
 * @file drv_djimotor.c
 * @brief DJI 智能电机驱动实现
 *
 * @note 多态实现：
 *       - 虚函数表导出供宏定义使用
 *       - 注册函数设置 CAN 回调和共享发送实例
 *       - MotorControl 接收实例句柄，立即发送 CAN
 */

#include "drv_djimotor.h"

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include <string.h>

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
static int8_t DJIMotorReg(MotorInstance *inst);
static int8_t DJIMotorInit(MotorInstance *inst);
static void DJIMotorEnable(MotorInstance *inst);
static void DJIMotorStop(MotorInstance *inst);
static void DJIMotorSetRef(MotorInstance *inst, float ref);
static void DJIMotorSetOuterLoop(MotorInstance *inst, MotorLoopType_e loop);
static void DJIMotorGetStatus(MotorInstance *inst, MotorStatus_t *status);
static int8_t DJIMotorSetPID(MotorInstance *inst, MotorLoopType_e loop, float kp, float ki, float kd, float integral_limit, float max_out);
static void DJIMotorChangeFeedback(MotorInstance *inst, MotorLoopType_e loop, FeedbackSource_e src);
static void DJIMotorControl(MotorInstance *inst);

// 电机组虚函数实现
static int8_t DJIMotorGroupReg(MotorGroupInstance *inst);
static int8_t DJIMotorGroupInit(MotorGroupInstance *inst);
static void DJIMotorGroupEnable(MotorGroupInstance *inst, uint8_t motor_idx);
static void DJIMotorGroupStop(MotorGroupInstance *inst, uint8_t motor_idx);
static void DJIMotorGroupSetRef(MotorGroupInstance *inst, uint8_t motor_idx, float ref);
static void DJIMotorGroupGetStatus(MotorGroupInstance *inst, uint8_t motor_idx, MotorStatus_t *status);
static void DJIMotorGroupControl(MotorGroupInstance *inst);

// 内部辅助函数
static void DJIMotorDecode(CANInstance *can_inst);
static void DJIMotorGroupDecode(CANInstance *can_inst);
static uint8_t DJIMotorGetSenderGroup(BoardCAN_e can_e, uint8_t motor_id, DJIModel_e model);
static CANInstance *DJIMotorGetOrCreateSender(uint8_t group);

/*============================================
 *              虚函数表定义（导出）
 *============================================*/

const MotorInterface_s djimotor_vtable = {
    .reg = DJIMotorReg,
    .init = DJIMotorInit,
    .enable = DJIMotorEnable,
    .stop = DJIMotorStop,
    .set_ref = DJIMotorSetRef,
    .set_outer_loop = DJIMotorSetOuterLoop,
    .get_status = DJIMotorGetStatus,
    .set_pid = DJIMotorSetPID,
    .change_feedback = DJIMotorChangeFeedback,
    .control = DJIMotorControl,
};

const MotorGroupInterface_s djimotor_group_vtable = {
    .reg = DJIMotorGroupReg,
    .init = DJIMotorGroupInit,
    .enable = DJIMotorGroupEnable,
    .stop = DJIMotorGroupStop,
    .set_ref = DJIMotorGroupSetRef,
    .get_status = DJIMotorGroupGetStatus,
    .control = DJIMotorGroupControl,
};

/*============================================
 *              单电机虚函数实现
 *============================================*/

/**
 * @brief 注册 DJI 单电机实例（虚函数）
 */
static int8_t DJIMotorReg(MotorInstance *inst)
{
    if (!inst)
        return -1;

    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    DJIMotorPriv_t *priv = &motor->priv;

    // 设置 parent 指针
    priv->rx_can->parent = motor;

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

    return 0;
}

static int8_t DJIMotorInit(MotorInstance *inst)
{
    if (!inst)
        return -1;

    inst->status.enable = 1;
    inst->status.online = 0;
    return 0;
}

static void DJIMotorEnable(MotorInstance *inst)
{
    if (inst)
        inst->status.enable = 1;
}

static void DJIMotorStop(MotorInstance *inst)
{
    if (inst)
        inst->status.enable = 0;
}

static void DJIMotorSetRef(MotorInstance *inst, float ref)
{
    if (inst)
        inst->controller.pid_ref = ref;
}

static void DJIMotorSetOuterLoop(MotorInstance *inst, MotorLoopType_e loop)
{
    if (inst)
        inst->settings.outer_loop_type = loop;
}

static void DJIMotorGetStatus(MotorInstance *inst, MotorStatus_t *status)
{
    if (inst && status)
        *status = inst->status;
}

static int8_t DJIMotorSetPID(MotorInstance *inst, MotorLoopType_e loop, float kp, float ki, float kd, float integral_limit, float max_out)
{
    return MotorSetPID(inst, loop, kp, ki, kd, integral_limit, max_out);
}

static void DJIMotorChangeFeedback(MotorInstance *inst, MotorLoopType_e loop, FeedbackSource_e src)
{
    if (!inst)
        return;

    if (loop == MOTOR_LOOP_ANGLE)
        inst->settings.angle_feedback_src = src;
    else if (loop == MOTOR_LOOP_SPEED)
        inst->settings.speed_feedback_src = src;
}

/**
 * @brief 单电机控制并发送 CAN
 * @note 立即发送一帧 CAN 数据
 */
static void DJIMotorControl(MotorInstance *inst)
{
    if (!inst || !inst->status.enable)
        return;

    DJIMotorInstance *motor = (DJIMotorInstance *)inst;
    DJIMotorPriv_t *priv = &motor->priv;

    if (!priv->tx_can)
        return;

    // 串级 PID 计算
    float output = MotorCascadePID(inst, inst->dt);
    int16_t set = (int16_t)output;

    // 填充发送缓冲区
    uint8_t slot = priv->tx_slot;
    priv->tx_can->tx_buff[2 * slot] = (uint8_t)(set >> 8);
    priv->tx_can->tx_buff[2 * slot + 1] = (uint8_t)(set & 0xFF);

    // 立即发送（非阻塞，超时0ms）
    CANTransmit(priv->tx_can, 1);
}

/*============================================
 *              电机组虚函数实现
 *============================================*/

/**
 * @brief 注册 DJI 电机组实例（虚函数）
 */
static int8_t DJIMotorGroupReg(MotorGroupInstance *inst)
{
    if (!inst)
        return -1;

    DJIMotorGroupInstance *group = (DJIMotorGroupInstance *)inst;
    DJIMotorGroupPriv_t *priv = &group->priv;
    DJIModel_e model = (DJIModel_e)inst->model;
    BoardCAN_e can_e = CAN_1;

    // 注册所有接收 CAN 实例
    for (uint8_t i = 0; i < inst->motor_count; i++)
    {
        if (priv->motor_id[i] == 0 || !priv->rx_can[i])
            continue;

        priv->rx_can[i]->parent = group;

        uint32_t g_rx_id = (model == DJI_MODEL_GM6020) ? (0x204 + priv->motor_id[i]) : (0x200 + priv->motor_id[i]);

        CAN_Init_Config_s g_can_cfg = {
            .tx_id = CAN_ID_UNUSED,
            .filter_mode = CAN_FILTER_MODE_LIST,
            .rx_id_count = 1,
            .rx_id_list = {g_rx_id, CAN_ID_UNUSED, CAN_ID_UNUSED, CAN_ID_UNUSED},
            .rx_mask = 0,
            .rx_callback = DJIMotorGroupDecode,
        };

        if (CANRegister(priv->rx_can[i], &g_can_cfg) != 0)
            return -1;

        // 获取 CAN 总线（假设所有电机在同一总线）
        if (i == 0)
            can_e = priv->rx_can[i]->can_e;
    }

    // 计算发送分组（使用第一个电机的 ID）
    uint8_t group_idx = DJIMotorGetSenderGroup(can_e, priv->motor_id[0], model);

    // 获取或创建发送 CAN 实例
    priv->tx_can = DJIMotorGetOrCreateSender(group_idx);

    return 0;
}

static int8_t DJIMotorGroupInit(MotorGroupInstance *inst)
{
    if (!inst)
        return -1;

    for (uint8_t i = 0; i < inst->motor_count; i++)
    {
        inst->status[i].enable = 1;
        inst->status[i].online = 0;
    }
    return 0;
}

static void DJIMotorGroupEnable(MotorGroupInstance *inst, uint8_t motor_idx)
{
    if (inst && motor_idx < inst->motor_count)
        inst->status[motor_idx].enable = 1;
}

static void DJIMotorGroupStop(MotorGroupInstance *inst, uint8_t motor_idx)
{
    if (inst && motor_idx < inst->motor_count)
        inst->status[motor_idx].enable = 0;
}

static void DJIMotorGroupSetRef(MotorGroupInstance *inst, uint8_t motor_idx, float ref)
{
    if (inst && motor_idx < inst->motor_count)
        inst->controller[motor_idx].pid_ref = ref;
}

static void DJIMotorGroupGetStatus(MotorGroupInstance *inst, uint8_t motor_idx, MotorStatus_t *status)
{
    if (inst && status && motor_idx < inst->motor_count)
        *status = inst->status[motor_idx];
}

/**
 * @brief 电机组控制并发送 CAN
 * @note 统一发送一帧 CAN 控制 4 个电机
 */
static void DJIMotorGroupControl(MotorGroupInstance *inst)
{
    if (!inst)
        return;

    DJIMotorGroupInstance *group = (DJIMotorGroupInstance *)inst;
    DJIMotorGroupPriv_t *priv = &group->priv;

    if (!priv->tx_can)
        return;

    // 遍历所有电机，计算 PID 并填充缓冲区
    for (uint8_t i = 0; i < inst->motor_count; i++)
    {
        if (!inst->status[i].enable)
            continue;

        // 串级 PID 计算（使用电机组专用的 MotorGroupCascadePID）
        float output = MotorCascadePID((MotorInstance *)inst, inst->dt);
        int16_t set = (int16_t)output;

        priv->tx_can->tx_buff[2 * i] = (uint8_t)(set >> 8);
        priv->tx_can->tx_buff[2 * i + 1] = (uint8_t)(set & 0xFF);
    }

    // 立即发送（非阻塞，超时0ms）
    CANTransmit(priv->tx_can, 1);
}

/*============================================
 *              内部辅助函数实现
 *============================================*/

/**
 * @brief DJI 单电机 CAN 接收解码
 */
static void DJIMotorDecode(CANInstance *can_inst)
{
    DJIMotorInstance *motor = (DJIMotorInstance *)can_inst->parent;
    if (!motor)
        return;

    uint8_t *rxbuff = can_inst->rx_buff;
    DJIMotorMeasure_t *measure = &motor->priv.measure;

    // 更新在线状态
    motor->base.status.online = 1;

    // 计算 dt
    motor->base.dt = DWT_GetDeltaT(&motor->priv.feed_cnt);

    // 保存上一次编码器值
    measure->last_ecd = measure->ecd;

    // 解析 CAN 数据
    measure->ecd = ((uint16_t)rxbuff[0] << 8) | rxbuff[1];
    measure->angle_single_round = DJI_ECD_ANGLE_COEF * (float)measure->ecd;

    // 速度 (RPM -> rad/s)
    int16_t speed_rpm = ((int16_t)rxbuff[2] << 8) | rxbuff[3];
    motor->base.status.speed = speed_rpm * DJI_RPM_TO_RADPS;

    // 电流
    int16_t current = ((int16_t)rxbuff[4] << 8) | rxbuff[5];
    motor->base.status.current = (float)current;

    // 温度
    motor->base.status.temperature = (float)rxbuff[6];

    // 多圈角度计算
    if (measure->ecd - measure->last_ecd > 4096)
        measure->total_round--;
    else if (measure->ecd - measure->last_ecd < -4096)
        measure->total_round++;

    measure->total_angle = measure->total_round * 360.0f + measure->angle_single_round;
    motor->base.status.angle = measure->total_angle * 0.017453f; // 度转弧度
}

/**
 * @brief DJI 电机组 CAN 接收解码
 */
static void DJIMotorGroupDecode(CANInstance *can_inst)
{
    DJIMotorGroupInstance *group = (DJIMotorGroupInstance *)can_inst->parent;
    if (!group)
        return;

    // 根据 rx_id 确定是哪个电机
    uint32_t rx_id = can_inst->rx_id_list[0];
    uint8_t motor_idx = 0;
    DJIModel_e model = (DJIModel_e)group->base.model;

    // 从 rx_id 反推 motor_id 和索引
    uint8_t motor_id = 0;
    if (model == DJI_MODEL_GM6020)
        motor_id = rx_id - 0x204;
    else
        motor_id = rx_id - 0x200;

    // 在 motor_id 数组中查找索引
    for (uint8_t i = 0; i < group->base.motor_count; i++)
    {
        if (group->priv.motor_id[i] == motor_id)
        {
            motor_idx = i;
            break;
        }
    }

    uint8_t *rxbuff = can_inst->rx_buff;
    DJIMotorMeasure_t *measure = &group->priv.measure[motor_idx];

    // 更新在线状态
    group->base.status[motor_idx].online = 1;

    // 保存上一次编码器值
    measure->last_ecd = measure->ecd;

    // 解析 CAN 数据
    measure->ecd = ((uint16_t)rxbuff[0] << 8) | rxbuff[1];
    measure->angle_single_round = DJI_ECD_ANGLE_COEF * (float)measure->ecd;

    // 速度 (RPM -> rad/s)
    int16_t speed_rpm = ((int16_t)rxbuff[2] << 8) | rxbuff[3];
    group->base.status[motor_idx].speed = speed_rpm * DJI_RPM_TO_RADPS;

    // 电流
    int16_t current = ((int16_t)rxbuff[4] << 8) | rxbuff[5];
    group->base.status[motor_idx].current = (float)current;

    // 温度
    group->base.status[motor_idx].temperature = (float)rxbuff[6];

    // 多圈角度计算
    if (measure->ecd - measure->last_ecd > 4096)
        measure->total_round--;
    else if (measure->ecd - measure->last_ecd < -4096)
        measure->total_round++;

    measure->total_angle = measure->total_round * 360.0f + measure->angle_single_round;
    group->base.status[motor_idx].angle = measure->total_angle * 0.017453f;
}

/**
 * @brief 获取发送分组索引
 * @param can_e CAN 总线
 * @param motor_id 电机 ID (1-8)
 * @param model 电机型号
 * @return 分组索引 (0-5)
 */
static uint8_t DJIMotorGetSenderGroup(BoardCAN_e can_e, uint8_t motor_id, DJIModel_e model)
{
    uint8_t can_offset = (can_e == CAN_1) ? 0 : 3;

    switch (model)
    {
    case DJI_MODEL_M3508:
    case DJI_MODEL_M2006:
        // M3508/M2006: 发送 ID 0x200 (ID 1-4) 或 0x1FF (ID 5-8)
        if (motor_id <= 4)
            return can_offset + 1; // 0x200
        else
            return can_offset + 0; // 0x1FF

    case DJI_MODEL_GM6020:
        // GM6020: 发送 ID 0x1FF (ID 1-4) 或 0x2FF (ID 5-7)
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
 * @param group 分组索引 (0-5)
 * @return CAN 实例指针
 */
static CANInstance *DJIMotorGetOrCreateSender(uint8_t group)
{
    if (sender_init_flag[group])
        return &sender_group[group];

    // 确定发送 ID 和 CAN 总线
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

    // 初始化发送 CAN 实例
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

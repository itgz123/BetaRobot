/**
 * @file drv_djimotor.c
 * @brief DJI 智能电机驱动实现
 *
 * @note 多态实现：
 *       - 所有虚函数表函数为 static，仅通过 vtable 调用
 *       - 对外只提供 DJIMotorRegister() 初始化函数
 */

#include "drv_djimotor.h"

#ifdef BSP_CAN_MODULE_ENABLED

#include "bsp_dwt.h"
#include <string.h>

/*============================================
 *              内部变量
 *============================================*/

// 已注册的 DJI 电机实例数组
static DJIMotorInstance *dji_motor_list[DJI_MOTOR_MAX_COUNT] = {NULL};
static uint8_t motor_count = 0;

// DJI 电机发送分组（6组：CAN1的0x1FF/0x200/0x2FF + CAN2的0x1FF/0x200/0x2FF）
static CANInstance sender_group[6] = {0};
static uint8_t sender_enable_flag[6] = {0};

/*============================================
 *              内部函数声明
 *============================================*/

// 虚函数表实现
static int8_t DJIMotorInit(MotorInstance *inst);
static void DJIMotorEnable(MotorInstance *inst);
static void DJIMotorStop(MotorInstance *inst);
static void DJIMotorSetRef(MotorInstance *inst, float ref);
static void DJIMotorSetOuterLoop(MotorInstance *inst, MotorLoopType_e loop);
static void DJIMotorGetStatus(MotorInstance *inst, MotorStatus_t *status);
static int8_t DJIMotorSetPID(MotorInstance *inst, MotorLoopType_e loop,
                              float kp, float ki, float kd, float integral_limit, float max_out);
static void DJIMotorChangeFeedback(MotorInstance *inst, MotorLoopType_e loop, FeedbackSource_e src);

// 内部辅助函数
static void DJIMotorDecode(CANInstance *can_inst);
static void DJIMotorSenderGrouping(DJIMotorInstance *motor, DJIMotorInitConfig_t *config);

/*============================================
 *              虚函数表定义
 *============================================*/

static const MotorInterface_s djimotor_vtable = {
    .init = DJIMotorInit,
    .enable = DJIMotorEnable,
    .stop = DJIMotorStop,
    .set_ref = DJIMotorSetRef,
    .set_outer_loop = DJIMotorSetOuterLoop,
    .get_status = DJIMotorGetStatus,
    .set_pid = DJIMotorSetPID,
    .change_feedback = DJIMotorChangeFeedback,
};

/*============================================
 *              虚函数实现
 *============================================*/

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

static int8_t DJIMotorSetPID(MotorInstance *inst, MotorLoopType_e loop,
                              float kp, float ki, float kd, float integral_limit, float max_out)
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

/*============================================
 *              内部辅助函数实现
 *============================================*/

/**
 * @brief DJI 电机 CAN 接收解码
 */
static void DJIMotorDecode(CANInstance *can_inst)
{
    DJIMotorInstance *motor = (DJIMotorInstance *)can_inst->parent;
    if (!motor)
        return;

    uint8_t *rxbuff = can_inst->rx_buff;
    DJIMotorMeasure_t *measure = &motor->measure;

    // 更新在线状态
    motor->base.status.online = 1;

    // 计算 dt
    motor->base.dt = DWT_GetDeltaT(&motor->feed_cnt);

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
 * @brief DJI 电机发送分组
 */
static void DJIMotorSenderGrouping(DJIMotorInstance *motor, DJIMotorInitConfig_t *config)
{
    uint8_t motor_id = config->motor_id - 1; // ID 从 0 开始
    uint8_t can_offset = (config->can_e == CAN_1) ? 0 : 3;

    switch (config->type)
    {
    case MOTOR_TYPE_DJI_M3508:
    case MOTOR_TYPE_DJI_M2006:
        // M3508/M2006: 发送 ID 0x200 (ID 1-4) 或 0x1FF (ID 5-8)
        if (motor_id < 4)
        {
            motor->sender_group = can_offset + 1; // 0x200
            motor->message_num = motor_id;
        }
        else
        {
            motor->sender_group = can_offset + 0; // 0x1FF
            motor->message_num = motor_id - 4;
        }
        break;

    case MOTOR_TYPE_DJI_GM6020:
        // GM6020: 发送 ID 0x1FF (ID 1-4) 或 0x2FF (ID 5-7)
        if (motor_id < 4)
        {
            motor->sender_group = can_offset + 0; // 0x1FF
            motor->message_num = motor_id;
        }
        else
        {
            motor->sender_group = can_offset + 2; // 0x2FF
            motor->message_num = motor_id - 4;
        }
        break;

    default:
        return;
    }

    sender_enable_flag[motor->sender_group] = 1;
}

/*============================================
 *              发送控制函数
 *============================================*/

/**
 * @brief DJI 电机控制发送（周期调用）
 * @note 遍历所有电机，计算 PID 并填充发送缓冲区
 */
void DJIMotorControl(void)
{
    for (uint8_t i = 0; i < motor_count; i++)
    {
        DJIMotorInstance *motor = dji_motor_list[i];
        if (!motor || !motor->base.status.enable)
            continue;

        // 串级 PID 计算
        float output = MotorCascadePID(&motor->base, motor->base.dt);
        int16_t set = (int16_t)output;

        // 填充发送缓冲区
        uint8_t group = motor->sender_group;
        uint8_t num = motor->message_num;

        sender_group[group].tx_buff[2 * num] = (uint8_t)(set >> 8);
        sender_group[group].tx_buff[2 * num + 1] = (uint8_t)(set & 0xFF);
    }

    // 发送所有启用的分组
    for (uint8_t i = 0; i < 6; i++)
    {
        if (sender_enable_flag[i])
        {
            CANTransmit(&sender_group[i], 10);
        }
    }
}

/*============================================
 *              对外接口
 *============================================*/

/**
 * @brief 注册 DJI 电机实例
 * @param motor  DJI 电机实例指针（需由调用者分配内存）
 * @param config 初始化配置
 * @retval 0 成功
 * @retval -1 失败
 */
int8_t DJIMotorRegister(DJIMotorInstance *motor, DJIMotorInitConfig_t *config)
{
    if (!motor || !config || motor_count >= DJI_MOTOR_MAX_COUNT)
        return -1;

    // 清零结构体
    memset(motor, 0, sizeof(DJIMotorInstance));

    // 设置虚函数表
    motor->base.vtable = &djimotor_vtable;
    motor->base.type = config->type;
    motor->base.impl = motor; // impl 指向自身

    // 初始化基类配置
    MotorInitConfig_t base_config = {
        .type = config->type,
        .outer_loop_type = config->outer_loop_type,
        .close_loop_type = config->close_loop_type,
        .motor_reverse = config->motor_reverse,
        .feedback_reverse = config->feedback_reverse,
        .current_kp = config->current_kp,
        .current_ki = config->current_ki,
        .current_kd = config->current_kd,
        .speed_kp = config->speed_kp,
        .speed_ki = config->speed_ki,
        .speed_kd = config->speed_kd,
        .angle_kp = config->angle_kp,
        .angle_ki = config->angle_ki,
        .angle_kd = config->angle_kd,
        .angle_feedback_ptr = config->angle_feedback_ptr,
        .speed_feedback_ptr = config->speed_feedback_ptr,
        .speed_ff_ptr = config->speed_ff_ptr,
        .current_ff_ptr = config->current_ff_ptr,
    };
    MotorBaseInit(&motor->base, &base_config);

    // 初始化 CAN 实例（静态定义）
    static CANInstance can_instances[DJI_MOTOR_MAX_COUNT];
    CANInstance *can_inst = &can_instances[motor_count];

    // 计算接收 ID
    uint32_t rx_id;
    if (config->type == MOTOR_TYPE_DJI_GM6020)
        rx_id = 0x204 + config->motor_id;
    else
        rx_id = 0x200 + config->motor_id;

    // 配置 CAN 实例
    can_inst->parent = motor;
    can_inst->can_e = config->can_e;
    can_inst->tx_id = CAN_ID_UNUSED; // DJI 电机使用分组发送
    can_inst->filter_mode = CAN_FILTER_MODE_LIST;
    can_inst->rx_id_count = 1;  // 列表模式下必须设置有效接收 ID 数量
    can_inst->rx_id_list[0] = rx_id;
    can_inst->rx_id_list[1] = CAN_ID_UNUSED;
    can_inst->rx_id_list[2] = CAN_ID_UNUSED;
    can_inst->rx_id_list[3] = CAN_ID_UNUSED;
    can_inst->rx_callback = DJIMotorDecode;

    // 注册 CAN 实例
    if (CANRegister(can_inst) != 0)
        return -1;

    motor->can_inst = can_inst;

    // 发送分组
    DJIMotorSenderGrouping(motor, config);

    // 初始化发送分组 CAN 实例（首次使用时）
    static uint8_t sender_init_flag[6] = {0};
    uint8_t group = motor->sender_group;

    if (!sender_init_flag[group])
    {
        uint32_t tx_id;
        BoardCAN_e can_e = (group < 3) ? CAN_1 : CAN_2;

        switch (group % 3)
        {
        case 0: tx_id = 0x1FF; break;
        case 1: tx_id = 0x200; break;
        case 2: tx_id = 0x2FF; break;
        }

        sender_group[group].parent = NULL;
        sender_group[group].can_e = can_e;
        sender_group[group].tx_id = tx_id;
        sender_group[group].filter_mode = CAN_FILTER_MODE_MASK;
        sender_group[group].rx_id_count = 0;  // 掩码模式，仅发送不接收
        sender_group[group].rx_id_list[0] = CAN_ID_UNUSED;
        sender_group[group].rx_callback = NULL;

        CANRegister(&sender_group[group]);
        sender_init_flag[group] = 1;
    }

    // 添加到电机列表
    dji_motor_list[motor_count++] = motor;

    // 调用虚函数 init
    if (motor->base.vtable->init)
        motor->base.vtable->init(&motor->base);

    return 0;
}

#endif // BSP_CAN_MODULE_ENABLED

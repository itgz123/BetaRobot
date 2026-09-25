/**
 * @file drvs_dmmotor.c
 * @brief DM 电机（4310 / 4310P）纯协议驱动实现
 * @author TRW
 * @date 2026-09-21
 *
 * @note 协议：CAN MIT 模式，8 字节帧，12 位跨字节位域
 * @note 映射：无符号整数线性映射（不是有符号补码）
 * @note 职责边界见 drvs_dmmotor.h：只做字节 ↔ 物理量，不做闭环/滤波/方向/累加/零点
 */

#include "drvs_dmmotor.h"
#include "app_cfg.h"

#ifdef DRVS_DMMOTOR_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_log.h"
#include <math.h>
#include <string.h>

#ifndef DRVS_DMMOTOR_LOG_LIMIT
#define DRVS_DMMOTOR_LOG_LIMIT 10
#endif // !DRVS_DMMOTOR_LOG_LIMIT
LOG_INSTANCE_DEF(g_drvs_dmmotor_log, "drvs_dmmotor", DRVS_DMMOTOR_LOG_LIMIT);

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
static inline float dm_uint_to_float(uint16_t uint_val, float scale, float offset)
{
    return (float)uint_val * scale + offset;
}

/**
 * @brief 浮点 → 无符号定点（每帧 1 次 / 电机）
 * @note  (val + range) * inv_scale  (1 add + 1 mul)
 */
static inline uint16_t dm_float_to_uint(float float_val, float inv_scale, float range)
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
 * @brief DM 反馈帧回调（ISR 上下文）
 *
 * 反馈帧格式（8 字节）见 drvs_dmmotor.h
 */
static void DrvsDMMotorRxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (!can || !can->parent)
        return;

    DrvsDMMotor_s *inst = (DrvsDMMotor_s *)can->parent;
    const DrvsDMMotorFeedbackFrame_u *fb = (const DrvsDMMotorFeedbackFrame_u *)pack->data;
    const DrvsDMMotorProtocolMap_s *map = &inst->proto_map;

    /* 位域展开（位置是大端 16 位；速度/扭矩各 12 位跨字节） */
    uint16_t raw_position = ((uint16_t)fb->parts.position_be >> 8) | ((uint16_t)fb->parts.position_be << 8);
    uint16_t raw_velocity = ((uint16_t)fb->parts.vel_hi << 4) | (fb->parts.vel_lo_and_torque_hi >> 4);
    uint16_t raw_torque = ((uint16_t)(fb->parts.vel_lo_and_torque_hi & 0x0F) << 8) | fb->parts.torque_lo;

    /* 写入当前 ISR 缓冲区 */
    DrvsDMMotorData_s *out = &inst->data[inst->data_idx];

    /* 定点 → 物理量 */
    out->position = dm_uint_to_float(raw_position, map->pos_to_float_scale, -map->p_max);
    out->speed = dm_uint_to_float(raw_velocity, map->vel_to_float_scale, -map->v_range);
    out->torque = dm_uint_to_float(raw_torque, map->t_to_float_scale, -map->t_range);

    /* 器件状态 */
    out->error = fb->parts.id_and_error >> 4;
    out->temperature_mos = fb->parts.temp_mos;
    out->temperature_coil = fb->parts.temp_coil;
    out->timestamp_us = DWT_GetTimeUs();

    /* flip 双缓冲索引：写完才翻，读者看不到半截帧 */
    inst->data_idx = (uint8_t)(!inst->data_idx);

    /* 喂狗 */
    if (inst->daemon)
        DaemonReload(inst->daemon);
}

/*============================================
 *        CAN 错误 / 逐帧发送结果（drv 层错误处理）
 *
 * app 不主动检查错误，bsp 两条上报通道就都得由本层接住：
 *   - err_callback        ：总线/硬件级事件。它是**外设级广播**（同一条总线上的所有实例
 *                           都收到同一个原因，见 bsp_can.h 契约），故本层只做限频日志，
 *                           不做"只对自己成立"的状态复位（那会误伤同总线的其他实例）；
 *   - tx_complete_callback：**逐帧**结果。result != BSP_OK 即这一帧确实没发出去
 *                           （仲裁失败 / 发送错误 / 被取消 / 溯源丢失）——
 *                           旧实现只计"入队失败"（CANTransmit 的返回值），
 *                           帧入队之后的失败是全静默的。
 * 本层不做策略性动作：控制帧是周期性的，丢一帧下一周期自然补上；重试只会拖乱控制周期。
 *============================================*/

static void DrvsDMMotorErrHook(CANInstance *can, CAN_ErrReason_e reason)
{
    (void)can; /* 广播：错误归整条总线，不属于某个实例；分类计数在 bsp 的 s_*_status[] 里 */

    /* 限频靠 LOG_INSTANCE_DEF 的 times_per_second（CAN_ERR_PROTOCOL 在总线异常时可每帧一次，
     * 没有这个上限会刷屏），细分原因/错误计数器见 bsp/bsp_can.md 的状态变量表 */
    BSPLOG(&g_drvs_dmmotor_log, (reason == CAN_ERR_BUS_OFF) ? LOG_LEVEL_ERROR : LOG_LEVEL_WARNING,
           "CAN bus error, reason=%u", (unsigned)reason);
}

static void DrvsDMMotorTxHook(CANInstance *can, uint32_t tx_mailbox, BSP_Status_e result)
{
    /* 本模块一个实例一条 CAN（Config 里 .parent = inst），失败归属由 can->parent 唯一确定，
     * 无需按 tx_mailbox 区分帧来源 */
    DrvsDMMotor_s *inst = (can != NULL) ? (DrvsDMMotor_s *)can->parent : NULL;

    (void)tx_mailbox;
    if (inst == NULL || result == BSP_OK)
        return;

    inst->tx_fail++; /* 逐帧失败，与发送点记的"入队失败"同一个计数器（只增不清，调试用） */
    BSPLOG(&g_drvs_dmmotor_log, LOG_LEVEL_WARNING, "frame not sent (result=%d)", (int)result);
}

/*============================================
 *              模式命令发送
 *============================================*/

void DrvsDMMotorSendCmd(DrvsDMMotor_s *inst, uint8_t cmd)
{
    if (!inst || !inst->can)
        return;

    /* 帧格式：前 7 字节 0xFF，第 8 字节命令码 */
    CAN_Pack_s pack = {.id = inst->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    memset(pack.data, 0xFF, 7);
    pack.data[7] = cmd;

    /* 一次性命令帧没有"下一周期补发"：失败只体现在 tx_fail，调用方要可靠下发改自己重试 */
    if (CANTransmit(inst->can, &pack, inst->timeout_ms, NULL, NULL) != BSP_OK)
        inst->tx_fail++;
}

/*============================================
 *              注册 / 配置
 *============================================*/

/**
 * @brief 注册 DM 电机实例（仅调用一次）
 * @note 只注册 CAN/Daemon 实例，不配置量程等参数（由 DrvsDMMotorConfig 负责）
 */
int8_t DrvsDMMotorRegister(DrvsDMMotor_s *inst)
{
    if (!inst)
        return -1;

    /* 防重复注册 */
    if (inst->can && inst->can->parent == inst)
        return -1;

    if (inst->can)
    {
        if (CANRegister(inst->can) != BSP_OK)
            return -1;
        inst->can->parent = inst;
    }

    if (inst->daemon)
        DaemonRegister(inst->daemon);

    return 0;
}

/**
 * @brief 配置 DM 电机实例（可重复调用）
 * @note 要求在 DrvsDMMotorRegister 之后调用
 */
int8_t DrvsDMMotorConfig(DrvsDMMotor_s *inst, const DrvsDMMotorConfig_s *cfg)
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
        inst->can_filter.callback = DrvsDMMotorRxCallback;

        CAN_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .mode = CAN_FRAME_FORMAT_CLASSIC,
            .parent = inst, /* 必须：CANConfig 会覆盖 parent，不设则回调取 can->parent 失效 */
            .filters = &inst->can_filter,
            .filter_num = 1,
            .tx_complete_callback = DrvsDMMotorTxHook, /* 逐帧结果：这一帧到底发出去没有 */
            .err_callback = DrvsDMMotorErrHook,        /* 总线/硬件级事件（ISR 广播） */
        };
        if (CANConfig(inst->can, &can_cfg) != BSP_OK)
            return -1;
    }

    /* 协议映射：量程 → 预计算 scale */
    DrvsDMMotorProtocolMap_s *map = &inst->proto_map;

    map->p_max = cfg->pos_max;
    map->v_range = cfg->vel_range;
    map->t_range = cfg->t_range;

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

    /* daemon：只当通信看门狗用，**刻意不挂离线回调**。
     * ① 离线事件的检测、状态转换与日志都由 drv_daemon 自己完成（OFFLINE / back ONLINE，
     *    见 drv_daemon.c），本层再挂一个每次 tick 都被调用的回调只会重复动作、刷日志；
     * ② 本层能对这个事件做的只有策略性动作（DM 掉线会自动退出 MIT 模式，要恢复控制
     *    就得重发 0xFC 使能），而本模块**不持有使能状态**（见头文件），无从判断该不该
     *    重发——挂了也是在回调里瞎猜；
     * ③ 真正的"发送侧出问题"已由 ErrHook / TxHook 接住（错误与逐帧结果都进 tx_fail 与日志）。 */
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

void DrvsDMMotorSetRef(DrvsDMMotor_s *inst, float torque)
{
    if (!inst)
        return;
    inst->ref_torque = torque;
}

void DrvsDMMotorSend(DrvsDMMotor_s *inst)
{
    if (!inst || !inst->can)
        return;

    const DrvsDMMotorProtocolMap_s *map = &inst->proto_map;

    /* 扭矩限幅（dm_float_to_uint 内部也会按 t_range 夹一次） */
    uint16_t t_ff = dm_float_to_uint(inst->ref_torque, map->t_to_uint_scale, map->t_range);

    /* 打包：板载 PD 不用，p_des/v_des/Kp/Kd 恒为 0，只有 t_ff 有效 */
    CAN_Pack_s pack = {.id = inst->can_id, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};
    DrvsDMMotorControlFrame_u *cf = (DrvsDMMotorControlFrame_u *)pack.data;
    cf->parts.p_des_be = 0;
    cf->parts.v_des_hi = 0;
    cf->parts.v_des_lo_and_kp_hi = 0;
    cf->parts.kp_lo = 0;
    cf->parts.kd_hi = 0;
    cf->parts.kd_lo_and_tff_hi = (uint8_t)((t_ff >> 8) & 0x0F);
    cf->parts.tff_lo = (uint8_t)(t_ff & 0xFF);

    /* 控制帧是周期性的：丢一帧下一周期自然补上，故只计数不重试（重试会拖乱控制周期） */
    if (CANTransmit(inst->can, &pack, inst->timeout_ms, NULL, NULL) != BSP_OK)
        inst->tx_fail++;
}

/*============================================
 *              反馈获取
 *============================================*/

DrvsDMMotorData_s DrvsDMMotorGetData(const DrvsDMMotor_s *inst)
{
    DrvsDMMotorData_s zero = {0};
    if (!inst)
        return zero;

    /* ISR 写的是 data[data_idx]，就绪的是另一个。
     * 一帧都没收到时 data[] 仍是 Config 清零后的全 0，故无需 data_valid 标志。
     * 需要判"是否收到过"看 timestamp_us == 0（真实帧的时间戳不会为 0）。 */
    return inst->data[!inst->data_idx];
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRVS_DMMOTOR_USED */

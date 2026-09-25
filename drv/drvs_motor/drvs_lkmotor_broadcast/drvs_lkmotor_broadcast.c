/**
 * @file drvs_lkmotor_broadcast.c
 * @brief LK（瓴控/翎控）MF 系列纯协议广播驱动实现（一拖四组帧）
 * @author TRW
 * @date 2026-09-21
 *
 * @note 职责边界见 drvs_lkmotor_broadcast.h：只做字节 ↔ 物理量，不做闭环/滤波/方向/累加/零点
 *
 * @note 广播模式一拖四必须由 LK 上位机（motor tool）开启并保存重启，总线 ≥ 500Kbps。
 */

#include "drvs_lkmotor_broadcast.h"
#include "app_cfg.h"

#ifdef DRVS_LKMOTOR_BROADCAST_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include "bsp_log.h"
#include <math.h>
#include <string.h>

#ifndef DRVS_LKMOTOR_BC_LOG_LIMIT
#define DRVS_LKMOTOR_BC_LOG_LIMIT 10
#endif // !DRVS_LKMOTOR_BC_LOG_LIMIT
LOG_INSTANCE_DEF(g_drvs_lkmotor_bc_log, "drvs_lkmotor_bc", DRVS_LKMOTOR_BC_LOG_LIMIT);

/*============================================
 *              协议常量
 *============================================*/
#define DRVS_LK_BC_TORQUE_ID 0x280u     // 力矩广播发送 ID（固定）
#define DRVS_LK_BC_REPLY_ID_BASE 0x140u // 回复 ID = 0x140 + motor_id

#define DRVS_LK_BC_MOTOR_ID_MIN 1u // 一拖四：ID 1~4
#define DRVS_LK_BC_MOTOR_ID_MAX 4u

#define DRVS_LK_BC_RAW_MAX 2000.0f // 0x280 iqControl 限幅（MF/MG ±2000，与一对一 ±2048 不同）

/* MF 固定：A = raw × 33/4096（MG 为 66/4096，本驱动不支持） */
#define DRVS_LK_BC_A_PER_LSB (33.0f / 4096.0f)

/* 编码器：实测整圈 raw 覆盖 0~65535（转一圈恰好回绕一次），固定按 16bit 处理 */
#define DRVS_LK_BC_ENCODER_TO_RAD (6.283185307179586f / 65536.0f) // 2π / 65536
#define DRVS_LK_BC_DPS_TO_RADPS 0.017453292519943295f             // π / 180

/* 状态2 帧的回显字节（只有 0xA0~0xA8 与 0x9C 的回复是该格式） */
#define DRVS_LK_BC_ECHO_TORQUE 0xA1u
#define DRVS_LK_BC_ECHO_READ_STATUS2 0x9Cu

/*============================================
 *              内部辅助函数
 *============================================*/

/**
 * @brief 判断回显字节是否为「状态2 格式」的回复
 * @note  广播模式只发 0x280（0xA1 语义）；其余回显或为全零回显、或为其他帧格式，必须排除。
 */
static inline uint8_t DrvsLKBroadcastIsStatusEcho(uint8_t echo)
{
    return (echo == DRVS_LK_BC_ECHO_TORQUE || echo == DRVS_LK_BC_ECHO_READ_STATUS2) ? 1u : 0u;
}

/*============================================
 *              CAN 接收回调（ISR）
 *
 * 先校验回显，再就地解析写进双缓冲里"当前 ISR 正在写"的那一份，写完翻索引。
 * 读者读的是另一份，所以永远拿到完整的一帧，不会被半截数据打断。
 *============================================*/
static void DrvsLKMotorBroadcastRxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (!can || !can->parent)
        return;

    DrvsLKMotorBroadcast_s *inst = (DrvsLKMotorBroadcast_s *)can->parent;

    /* 喂狗：只要能收到该 ID 的帧就说明通信在线（含全零回显帧），故放在回显过滤之前 */
    if (inst->daemon)
        DaemonReload(inst->daemon);

    /* 回显校验（关键）：全零回显帧直接丢弃，不写双缓冲，
     * 否则会把位置/速度误解析为 0 造成跳变。丢弃后 GetData 仍返回上一帧。 */
    if (!DrvsLKBroadcastIsStatusEcho(pack->data[0]))
        return;

    const DrvsLKBroadcastStatusFrame_u *fb = (const DrvsLKBroadcastStatusFrame_u *)pack->data;
    const DrvsLKMotorBroadcastProtocolMap_s *map = &inst->proto_map;

    /* 整字节小端量直接取（无跨字节位域，与 DM/RS 的移位展开不同） */
    int8_t raw_temp = fb->parts.temperature;
    int16_t raw_iq = fb->parts.iq_le;
    int16_t raw_speed = fb->parts.speed_dps_le;
    uint16_t raw_encoder = fb->parts.encoder_le;

    /* 写入当前 ISR 缓冲区 */
    DrvsLKMotorBroadcastData_s *out = &inst->data[inst->data_idx];

    out->position = (float)raw_encoder * map->encoder_to_rad; // 单圈 [0, 2π)
    out->speed = (float)raw_speed * map->dps_to_radps;
    out->current = (float)raw_iq * map->a_per_lsb;
    out->torque = (float)raw_iq * map->nm_per_lsb;
    out->temperature = raw_temp;
    out->echo = fb->parts.echo;
    out->timestamp_us = DWT_GetTimeUs();

    /* flip 双缓冲索引：写完才翻，读者看不到半截帧 */
    inst->data_idx = (uint8_t)(!inst->data_idx);
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

static void DrvsLKMotorBroadcastErrHook(CANInstance *can, CAN_ErrReason_e reason)
{
    (void)can; /* 广播：错误归整条总线，不属于某个实例；分类计数在 bsp 的 s_*_status[] 里 */

    /* 限频靠 LOG_INSTANCE_DEF 的 times_per_second（CAN_ERR_PROTOCOL 在总线异常时可每帧一次，
     * 没有这个上限会刷屏），细分原因/错误计数器见 bsp/bsp_can.md 的状态变量表 */
    BSPLOG(&g_drvs_lkmotor_bc_log, (reason == CAN_ERR_BUS_OFF) ? LOG_LEVEL_ERROR : LOG_LEVEL_WARNING,
           "CAN bus error, reason=%u", (unsigned)reason);
}

static void DrvsLKMotorBroadcastTxHook(CANInstance *can, uint32_t tx_mailbox, BSP_Status_e result)
{
    /* 组播帧经组内某个成员的 CAN 实例发出（GroupSend 的 tx_can），故 can->parent
     * 必然落回组内某个成员，再由它取回组 */
    DrvsLKMotorBroadcast_s *inst = (can != NULL) ? (DrvsLKMotorBroadcast_s *)can->parent : NULL;

    (void)tx_mailbox; /* 一帧带 4 个槽位，失败不摊到具体电机，也不需要区分帧来源 */
    if (inst == NULL || inst->group == NULL || result == BSP_OK)
        return;

    inst->group->tx_fail++; /* 与 GroupSend 记的"入队失败"共用组计数器（只增不清，调试用） */
    BSPLOG(&g_drvs_lkmotor_bc_log, LOG_LEVEL_WARNING, "group frame not sent (result=%d)", (int)result);
}

/*============================================
 *              注册
 *============================================*/
/**
 * @brief 注册 LK 广播电机实例（仅调用一次）
 * @note 只注册 CAN/Daemon 实例，不配置电机参数（由 Config 负责）
 */
int8_t DrvsLKMotorBroadcastRegister(DrvsLKMotorBroadcast_s *inst)
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

/*============================================
 *              配置
 *============================================*/
/**
 * @brief 配置 LK 广播电机并挂到组（可重复调用）
 * @note 要求先 Register。失败路径不动组成员关系。
 */
int8_t DrvsLKMotorBroadcastConfig(DrvsLKMotorBroadcast_s *inst,
                                  const DrvsLKMotorBroadcastConfig_s *cfg)
{
    if (!inst || !cfg || !cfg->group)
        return -1;

    DrvsLKMotorBroadcastGroup_s *group = cfg->group;

    /* 组表按 can_e 索引，索引发生在 CANConfig 之前，必须显式边界检查 */
    if (cfg->can_e >= CAN_NUM_MAX)
        return -1;
    if (cfg->motor_id < DRVS_LK_BC_MOTOR_ID_MIN || cfg->motor_id > DRVS_LK_BC_MOTOR_ID_MAX)
        return -1;
    if (cfg->torque_constant <= 0.0f)
        return -1;

    uint16_t rx_id = (uint16_t)(DRVS_LK_BC_REPLY_ID_BASE + cfg->motor_id);
    uint8_t slot = (uint8_t)(cfg->motor_id - 1u);

    /* 组归属：一条总线一组，成员必须同 can_e */
    if (group->member_count == 0u)
        group->can_e = cfg->can_e;
    else if (group->can_e != cfg->can_e)
        return -1;

    /* 槽位冲突：空 → 占；属于自己 → 原地更新；别人 → 拒绝 */
    if (group->slots[slot] && group->slots[slot] != inst)
        return -1;

    /* CAN 滤波器 + 工作模式：只收本电机 0x140 + id 的回复（经典帧） */
    if (inst->can)
    {
        inst->can_filter.mode = CAN_FILTER_MODE_LIST;
        inst->can_filter.id0 = rx_id;
        inst->can_filter.id1 = CAN_ID_UNUSED;
        inst->can_filter.frame_type = CAN_STANDARD_DATA_FRAME;
        inst->can_filter.callback = DrvsLKMotorBroadcastRxCallback;

        CAN_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .mode = CAN_FRAME_FORMAT_CLASSIC,
            .parent = inst, /* 必须：CANConfig 会覆盖 parent，不设则回调取 can->parent 失效 */
            .filters = &inst->can_filter,
            .filter_num = 1,
            .tx_complete_callback = DrvsLKMotorBroadcastTxHook, /* 逐帧结果：这一帧到底发出去没有 */
            .err_callback = DrvsLKMotorBroadcastErrHook,        /* 总线/硬件级事件（ISR 广播） */
        };
        if (CANConfig(inst->can, &can_cfg) != BSP_OK)
            return -1;
    }

    /* 协议映射：预计算 raw ↔ Nm / rad 的换算因子 */
    DrvsLKMotorBroadcastProtocolMap_s *map = &inst->proto_map;

    map->torque_constant = cfg->torque_constant;
    map->a_per_lsb = DRVS_LK_BC_A_PER_LSB;
    map->nm_per_lsb = cfg->torque_constant * DRVS_LK_BC_A_PER_LSB;
    map->inv_nm_per_lsb = 1.0f / map->nm_per_lsb;
    map->encoder_to_rad = DRVS_LK_BC_ENCODER_TO_RAD;
    map->dps_to_radps = DRVS_LK_BC_DPS_TO_RADPS;

    /* 标识与超时 */
    inst->motor_id = cfg->motor_id;
    inst->rx_id = rx_id;
    inst->timeout_ms = cfg->timeout_ms;

    /* 槽位簿记：先释放旧槽位（仅当旧组该槽位仍指向自己），再占新槽位。
     * 顺序不能反：新槽位校验通过之前若先释放旧槽位，失败时成员关系就残缺了。 */
    if (inst->group && inst->group->slots[inst->slot] == inst)
    {
        inst->group->slots[inst->slot] = NULL;
        if (inst->group->member_count > 0u)
            inst->group->member_count--;
    }
    group->slots[slot] = inst;
    group->member_count++; /* 上一步释放的若是同一槽位，这里补回，净效果不变 */

    inst->group = group;
    inst->slot = slot;

    /* 状态清零（无使能开关，扭矩一律归零：重新绑定/换槽后必须重新 SetRef 才会再输出；
     * 双缓冲清零后 GetData 在收到首帧前自然返回全 0） */
    inst->ref_torque = 0.0f;
    memset(inst->data, 0, sizeof(inst->data));
    inst->data_idx = 0;

    /* daemon：只当通信看门狗用，**刻意不挂离线回调**。
     * ① 离线事件的检测、状态转换与日志都由 drv_daemon 自己完成（OFFLINE / back ONLINE，
     *    见 drv_daemon.c），本层再挂一个每次 tick 都被调用的回调只会重复动作、刷日志；
     * ② 广播协议没有"恢复帧"（无协议级使能/清错可重发），本层也没有算法层状态可复位
     *    （旧驱动的回调是 reset PID，那属于算法层）；
     * ③ 真正的"发送侧出问题"已由 ErrHook / TxHook 接住（错误与逐帧结果都进组计数与日志）。
     * @note reload_count = 0 表示"不监控"：DaemonTask 整条跳过该实例，DaemonIsOnline
     *       随之恒报在线（不复位、也不报掉线）。app 若要轮询真实的在线状态必须配非 0。 */
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
 *              扭矩设定
 *============================================*/
void DrvsLKMotorBroadcastSetRef(DrvsLKMotorBroadcast_s *inst, float torque)
{
    if (!inst)
        return;
    inst->ref_torque = torque;
}

/*============================================
 *              组发送
 *============================================*/
/**
 * @brief 发送一组 0x280 力矩广播帧
 * @param group 广播组（app 持有）
 *
 * @note 一条总线只有一帧：槽位 = motor_id - 1，未占用槽位填 0（零扭矩）。
 */
void DrvsLKMotorBroadcastGroupSend(DrvsLKMotorBroadcastGroup_s *group)
{
    if (!group || group->member_count == 0u)
        return;

    CAN_Pack_s pack = {.id = DRVS_LK_BC_TORQUE_ID, .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};

    CANInstance *tx_can = NULL;
    uint32_t tx_timeout = 0;

    for (uint8_t i = 0; i < DRVS_LK_BC_SLOTS; i++)
    {
        int16_t raw = 0;
        DrvsLKMotorBroadcast_s *inst = group->slots[i];

        if (inst && inst->can)
        {
            const DrvsLKMotorBroadcastProtocolMap_s *map = &inst->proto_map;

            /* Nm → iq raw，先钳位再四舍五入（保证钳位后的值不会溢出 int16）
             * 限到协议 ±2000（= 满量程 33/4096×2000 ≈ 16.1A），无需再单独按 Nm 限扭矩 */
            float raw_f = inst->ref_torque * map->inv_nm_per_lsb;
            if (!isfinite(raw_f))
                raw_f = 0.0f;
            if (raw_f < -DRVS_LK_BC_RAW_MAX)
                raw_f = -DRVS_LK_BC_RAW_MAX;
            else if (raw_f > DRVS_LK_BC_RAW_MAX)
                raw_f = DRVS_LK_BC_RAW_MAX;

            raw = (int16_t)(raw_f >= 0.0f ? raw_f + 0.5f : raw_f - 0.5f);

            /* 用第一个成员实例发送（同组同 can_e，用谁都一样） */
            if (!tx_can)
            {
                tx_can = inst->can;
                tx_timeout = inst->timeout_ms;
            }
        }

        /* 小端写入槽位 (id-1)*2：低字节在前 */
        pack.data[i * 2] = (uint8_t)((uint16_t)raw & 0xFFu);
        pack.data[i * 2 + 1] = (uint8_t)((uint16_t)raw >> 8);
    }

    if (tx_can)
    {
        /* 组播帧一帧带 4 个槽位，失败摊到哪个电机都不对 → 记在组上 */
        if (CANTransmit(tx_can, &pack, tx_timeout, NULL, NULL) != BSP_OK)
            group->tx_fail++;
    }
}

/*============================================
 *              反馈获取
 *============================================*/
DrvsLKMotorBroadcastData_s DrvsLKMotorBroadcastGetData(const DrvsLKMotorBroadcast_s *inst)
{
    DrvsLKMotorBroadcastData_s zero = {0};
    if (!inst)
        return zero;

    /* ISR 写的是 data[data_idx]，就绪的是另一个。
     * 一帧都没收到时 data[] 仍是 Config 清零后的全 0，故无需 data_valid 标志。
     * 需要判"是否收到过"看 timestamp_us == 0（真实帧的时间戳不会为 0）。 */
    return inst->data[!inst->data_idx];
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRVS_LKMOTOR_BROADCAST_USED */

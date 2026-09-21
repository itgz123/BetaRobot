/**
 * @file drvs_djimotor_broadcast.c
 * @brief DJI 电机（M3508 / M2006 / GM6020）纯协议广播驱动实现（一拖四组帧）
 * @author TRW
 * @date 2026-09-21
 *
 * @note 职责边界见 drvs_djimotor_broadcast.h：只做字节 ↔ 物理量，不做闭环/滤波/方向/累加/零点
 *
 * @note **大端陷阱**：DJI 总线是大端（MSB first），而 Cortex-M 是小端。
 *       打包/解析一律用显式字节移位，绝不把控制帧建模成 int16_t[4] 之类的联合体
 *       —— 那种写法在小端机上会整帧字节序反掉，且因为上位机测试也是小端而抓不到。
 */

#include "drvs_djimotor_broadcast.h"
#include "app_cfg.h"

#ifdef DRVS_DJIMOTOR_BROADCAST_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include <math.h>
#include <string.h>

/*============================================
 *              协议常量
 *============================================*/
/* 组/槽位推导基准：slot = (rx_id - 0x201) % 4，group = (rx_id - 0x201) / 4 */
#define DRVS_DJI_GROUP_ID_BASE 0x201u

#define DRVS_DJI_ENCODER_RESOLUTION 8192u  // 14 位编码器，整圈 8192
#define DRVS_DJI_TWO_PI 6.283185307179586f // 2π

#define DRVS_DJI_ENCODER_TO_RAD (DRVS_DJI_TWO_PI / 8192.0f) // 编码器原始值 → rad
#define DRVS_DJI_RPM_TO_RADPS (DRVS_DJI_TWO_PI / 60.0f)     // 转速 rpm → rad/s

/*============================================
 *              电机参数表
 *
 * rx_id_base + tx_id 只由型号决定；电流量程差异同样只由型号决定
 * （raw 不同、安培数不同，二者的比值即 A/raw）。
 *============================================*/
typedef struct
{
    uint16_t rx_id_base; // 反馈 ID 基址：rx_id = rx_id_base + motor_id
    uint16_t tx_id[2];   // 控制帧 ID：tx_id[0] 用于 id1-4，tx_id[1] 用于 id5-8
    float raw_max;       // 电流原始值量程（±raw_max）
    float current_max_a; // 对应的安培数（±current_max_a）
} DrvsDJIMotorBroadcastParams_s;

static const DrvsDJIMotorBroadcastParams_s s_dji_params[DRVS_DJI_MODEL_NUM] = {
    [DRVS_DJI_MODEL_M3508] =
        {
            .rx_id_base = 0x200u,
            .tx_id = {0x200u, 0x1FFu},
            .raw_max = 16384.0f,
            .current_max_a = 20.0f,
        },
    [DRVS_DJI_MODEL_M2006] =
        {
            .rx_id_base = 0x200u,
            .tx_id = {0x200u, 0x1FFu},
            .raw_max = 10000.0f,
            .current_max_a = 10.0f,
        },
    [DRVS_DJI_MODEL_GM6020] =
        {
            .rx_id_base = 0x204u,
            .tx_id = {0x1FEu, 0x2FEu},
            .raw_max = 16384.0f,
            .current_max_a = 3.0f,
        },
};

_Static_assert(sizeof(s_dji_params) / sizeof(s_dji_params[0]) == DRVS_DJI_MODEL_NUM,
               "DJI 参数表长度必须等于型号数");

/*============================================
 *              CAN 接收回调（ISR）
 *
 * 就地解析写进双缓冲里"当前 ISR 正在写"的那一份，写完翻索引。
 * 读者读的是另一份，所以永远拿到完整的一帧，不会被半截数据打断。
 *============================================*/
static void DrvsDJIMotorBroadcastRxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (!can || !can->parent)
        return;

    DrvsDJIMotorBroadcast_s *inst = (DrvsDJIMotorBroadcast_s *)can->parent;

    /* 喂狗：DJI 反馈帧无回显字节，收到该 rx_id 的帧即说明通信在线 */
    if (inst->daemon)
        DaemonReload(inst->daemon);

    const uint8_t *d = pack->data;

    /* 大端解码：高字节在前，与端序无关（移位天然得到补码） */
    uint16_t raw_encoder = (uint16_t)(((uint16_t)d[0] << 8) | (uint16_t)d[1]);
    int16_t raw_speed = (int16_t)(((uint16_t)d[2] << 8) | (uint16_t)d[3]);
    int16_t raw_current = (int16_t)(((uint16_t)d[4] << 8) | (uint16_t)d[5]);

    const DrvsDJIMotorBroadcastProtocolMap_s *map = &inst->proto_map;
    DrvsDJIMotorBroadcastData_s *out = &inst->data[inst->data_idx];

    out->position = (float)raw_encoder * map->encoder_to_rad; // 单圈 [0, 2π)
    out->speed = (float)raw_speed * map->rpm_to_radps;
    out->current = (float)raw_current * map->a_per_lsb;
    out->torque = (float)raw_current * map->nm_per_lsb;
    out->temperature = (int8_t)d[6];
    out->error = d[7];
    out->timestamp_us = DWT_GetTimeUs();

    /* flip 双缓冲索引：写完才翻，读者看不到半截帧 */
    inst->data_idx = (uint8_t)(!inst->data_idx);
}

/*============================================
 *              注册
 *============================================*/
/**
 * @brief 注册 DJI 广播电机实例（仅调用一次）
 * @note 只注册 CAN/Daemon 实例，不配置电机参数（由 Config 负责）
 */
int8_t DrvsDJIMotorBroadcastRegister(DrvsDJIMotorBroadcast_s *inst)
{
    if (!inst)
        return -1;

    /* 防重复注册 */
    if (inst->can && inst->can->parent == inst)
        return -1;

    if (inst->can)
    {
        if (CANRegister(inst->can) != 0)
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
 * @brief 配置 DJI 广播电机并挂到组（可重复调用）
 * @note 要求先 Register。失败路径不动组成员关系。
 */
int8_t DrvsDJIMotorBroadcastConfig(DrvsDJIMotorBroadcast_s *inst,
                                   const DrvsDJIMotorBroadcastConfig_s *cfg)
{
    if (!inst || !cfg || !cfg->group)
        return -1;

    DrvsDJIMotorBroadcastGroup_s *group = cfg->group;

    /* 组表按 can_e 索引，索引发生在 CANConfig 之前，必须显式边界检查 */
    if (cfg->can_e >= CAN_NUM_MAX)
        return -1;
    if (cfg->model >= DRVS_DJI_MODEL_NUM)
        return -1;
    if (cfg->torque_constant <= 0.0f)
        return -1;

    /* GM6020 只有 ID 1~7（8 号会和 0x20C 之外的保留区冲突） */
    uint8_t id_max = (cfg->model == DRVS_DJI_MODEL_GM6020) ? 7u : 8u;
    if (cfg->motor_id < 1u || cfg->motor_id > id_max)
        return -1;

    const DrvsDJIMotorBroadcastParams_s *params = &s_dji_params[cfg->model];

    uint16_t rx_id = (uint16_t)(params->rx_id_base + cfg->motor_id);
    uint16_t tx_id = params->tx_id[(cfg->motor_id - 1u) / 4u];
    uint8_t slot = (uint8_t)((rx_id - DRVS_DJI_GROUP_ID_BASE) % DRVS_DJI_BC_SLOTS);

    /* 组归属：一条总线一组，成员必须同 can_e */
    if (group->member_count == 0u)
        group->can_e = cfg->can_e;
    else if (group->can_e != cfg->can_e)
        return -1;

    /* 槽位冲突：空 → 占；属于自己 → 原地更新；别人 → 拒绝
     * （同族 id 冲突靠这里拦：M3508 id5 与 GM6020 id1 都落 rx 0x205 / 槽位 0） */
    if (group->slots[slot] && group->slots[slot] != inst)
        return -1;

    /* CAN 滤波器 + 工作模式：只收本电机 rx_id 的反馈帧 */
    if (inst->can)
    {
        inst->can_filter.mode = CAN_FILTER_MODE_LIST;
        inst->can_filter.id0 = rx_id;
        inst->can_filter.id1 = CAN_ID_UNUSED;
        inst->can_filter.frame_type = CAN_STANDARD_DATA_FRAME;
        inst->can_filter.callback = DrvsDJIMotorBroadcastRxCallback;

        CAN_Config_s can_cfg = {
            .can_e = cfg->can_e,
            .mode = CAN_FRAME_FORMAT_CLASSIC,
            .parent = inst, /* 必须：CANConfig 会覆盖 parent，不设则回调取 can->parent 失效 */
            .filters = &inst->can_filter,
            .filter_num = 1,
        };
        if (CANConfig(inst->can, &can_cfg) != 0)
            return -1;
    }

    /* 协议映射：预计算 raw ↔ 物理量换算因子 */
    DrvsDJIMotorBroadcastProtocolMap_s *map = &inst->proto_map;

    map->torque_constant = cfg->torque_constant;
    map->raw_max = params->raw_max;
    map->a_per_lsb = params->current_max_a / params->raw_max;
    map->nm_per_lsb = cfg->torque_constant * map->a_per_lsb;
    map->inv_nm_per_lsb = 1.0f / map->nm_per_lsb;
    map->encoder_to_rad = DRVS_DJI_ENCODER_TO_RAD;
    map->rpm_to_radps = DRVS_DJI_RPM_TO_RADPS;

    /* 标识与超时 */
    inst->model = cfg->model;
    inst->motor_id = cfg->motor_id;
    inst->rx_id = rx_id;
    inst->tx_id = tx_id;
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

    /* daemon：只当通信看门狗用，不挂回调
     * （广播模式无协议级恢复帧；旧驱动的回调是 reset PID，那属于算法层，
     *   且掉线期间 daemon 每个 tick 都会调用一次回调，不适合做重发） */
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
void DrvsDJIMotorBroadcastSetRef(DrvsDJIMotorBroadcast_s *inst, float torque)
{
    if (!inst)
        return;
    inst->ref_torque = torque;
}

/*============================================
 *              组发送
 *============================================*/
/**
 * @brief 发送一组 DJI 控制帧
 * @param group 广播组（app 持有）
 *
 * @note 按 rx_id 分组时，组 1（rx 0x205~0x208）可能同时挂：
 *         - M3508/M2006 id5-8 → tx 0x1FF
 *         - GM6020 id1-4      → tx 0x1FE
 *       两者 rx 不冲突可以共组，但**不能共帧**（帧 ID 不同），故按 tx_id 分别发。
 *       tx_id 不匹配的槽位必须留 0，否则会把一个电机的电流写进另一型号的帧里。
 */
void DrvsDJIMotorBroadcastGroupSend(DrvsDJIMotorBroadcastGroup_s *group)
{
    if (!group || group->member_count == 0u)
        return;

    /* Step 1: 收集组内出现过的不同 tx_id 及发送所需的 CAN 实例/超时（实际最多 2 种） */
    uint16_t tx_ids[DRVS_DJI_BC_SLOTS];
    CANInstance *tx_cans[DRVS_DJI_BC_SLOTS];
    uint32_t tx_timeouts[DRVS_DJI_BC_SLOTS];
    uint8_t tx_count = 0;

    for (uint8_t i = 0; i < DRVS_DJI_BC_SLOTS; i++)
    {
        DrvsDJIMotorBroadcast_s *inst = group->slots[i];
        if (!inst || !inst->can)
            continue;

        uint8_t found = 0;
        for (uint8_t j = 0; j < tx_count; j++)
        {
            if (tx_ids[j] == inst->tx_id)
            {
                found = 1;
                break;
            }
        }
        if (found)
            continue;

        tx_ids[tx_count] = inst->tx_id;
        tx_cans[tx_count] = inst->can;            /* 同组同 can_e，用哪个成员的实例都一样 */
        tx_timeouts[tx_count] = inst->timeout_ms; /* 沿用该成员的 Config 超时 */
        tx_count++;
    }

    /* Step 2: 每个 tx_id 一个控制帧，槽位 = 帧内通道序号 */
    for (uint8_t t = 0; t < tx_count; t++)
    {
        CAN_Pack_s pack = {.id = tx_ids[t], .frame_type = CAN_STANDARD_DATA_FRAME, .len = 8};

        for (uint8_t i = 0; i < DRVS_DJI_BC_SLOTS; i++)
        {
            int16_t raw = 0;
            DrvsDJIMotorBroadcast_s *inst = group->slots[i];

            if (inst && inst->can && inst->tx_id == tx_ids[t])
            {
                const DrvsDJIMotorBroadcastProtocolMap_s *map = &inst->proto_map;

                /* Nm → 电流原始值，先钳位再四舍五入（保证钳位后的值不会溢出 int16） */
                float raw_f = inst->ref_torque * map->inv_nm_per_lsb;
                if (!isfinite(raw_f))
                    raw_f = 0.0f;
                if (raw_f < -map->raw_max)
                    raw_f = -map->raw_max;
                else if (raw_f > map->raw_max)
                    raw_f = map->raw_max;

                raw = (int16_t)(raw_f >= 0.0f ? raw_f + 0.5f : raw_f - 0.5f);
            }

            /* 大端写入（MSB first）：负数取补码后天然得到正确的两个字节 */
            pack.data[i * 2] = (uint8_t)((uint16_t)raw >> 8);
            pack.data[i * 2 + 1] = (uint8_t)((uint16_t)raw & 0xFFu);
        }

        CANTransmit(tx_cans[t], &pack, tx_timeouts[t], NULL, NULL);
    }
}

/*============================================
 *              反馈获取
 *============================================*/
DrvsDJIMotorBroadcastData_s DrvsDJIMotorBroadcastGetData(const DrvsDJIMotorBroadcast_s *inst)
{
    DrvsDJIMotorBroadcastData_s zero = {0};
    if (!inst)
        return zero;

    /* ISR 写的是 data[data_idx]，就绪的是另一个。
     * 一帧都没收到时 data[] 仍是 Config 清零后的全 0，故无需 data_valid 标志。
     * 需要判"是否收到过"看 timestamp_us == 0（真实帧的时间戳不会为 0）。 */
    return inst->data[!inst->data_idx];
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */

#endif /* DRVS_DJIMOTOR_BROADCAST_USED */

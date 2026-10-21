/**
 * @file drv_super_cap_yq2027.c
 * @brief YQ2027 超级电容 CAN 驱动实现（机器人主控侧）
 *
 * @note 协议依据：drv/drv_super_cap_yq2027/supercap-can-interface.md（2026-10-21 版）
 *       - 发送：0x061，8 字节控制命令（按 D0~D7 显式打包，不依赖位域布局）
 *       - 接收：0x052，8 字节紧凑反馈（仅新格式）
 *       - 错误：只做流程分流与计数，日志/总线上报归 bsp 层
 */

#include "app_cfg.h"
#include "drv_super_cap_yq2027.h"

#ifdef DRV_SUPER_CAP_YQ2027_USED

#if defined(HAL_CAN_MODULE_ENABLED) || defined(HAL_FDCAN_MODULE_ENABLED)

#include "bsp_dwt.h"
#include <string.h>

/* 功率编码：raw = P*64 + 16384，解码 W = raw/64 - 256 */
#define SUPER_CAP_YQ2027_POWER_SCALE 64.0f
#define SUPER_CAP_YQ2027_POWER_OFFSET 256.0f

/* capEnergy 满量程：250 = 100%（命令 D5 的分母才是 255） */
#define SUPER_CAP_YQ2027_CAP_FULL_SCALE 250.0f
#define SUPER_CAP_YQ2027_CAP_PERCENT_PER_RAW (100.0f / SUPER_CAP_YQ2027_CAP_FULL_SCALE)

/* 标准帧 ID 上限（11 位） */
#define SUPER_CAP_YQ2027_STD_ID_MAX 0x7FFu

/* D0 位掩码 */
#define SUPER_CAP_YQ2027_D0_ENABLE_DCDC 0x01u
#define SUPER_CAP_YQ2027_D0_SYSTEM_RESTART 0x02u
#define SUPER_CAP_YQ2027_D0_CLEAR_ERROR 0x20u
#define SUPER_CAP_YQ2027_D0_ENABLE_ACTIVE_LIMIT 0x40u

/*============================================
 *              内部辅助
 *============================================*/

/* 显式小端读 16 位：不依赖目标机字节序，也不会对未对齐地址解引用 */
static inline uint16_t SuperCapYq2027ReadU16Le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* 功率 raw → W（协议要求用浮点除法） */
static inline float SuperCapYq2027DecodePower(uint16_t raw)
{
    return (float)raw / SUPER_CAP_YQ2027_POWER_SCALE - SUPER_CAP_YQ2027_POWER_OFFSET;
}

/* capEnergy raw → %；raw > 250 时结果 > 100（表示超过额定满电） */
static inline float SuperCapYq2027DecodeCapPercent(uint8_t raw)
{
    return (float)raw * SUPER_CAP_YQ2027_CAP_PERCENT_PER_RAW;
}

/*============================================
 *              反馈解析（D0~D7 显式偏移）
 *============================================*/

static void SuperCapYq2027ParseFeedback(const uint8_t *d, SuperCapYq2027Data_s *out)
{
    uint8_t status = d[0];

    out->status_code = status;
    out->dcdc_enabled = (uint8_t)((status >> 7) & 0x01u);
    out->protocol_valid = (uint8_t)((status >> 6) & 0x01u);
    out->wireless_state = (SuperCapYq2027WirelessState_e)((status >> 4) & 0x03u);
    out->limit_reason = (SuperCapYq2027LimitReason_e)((status >> 2) & 0x03u);
    out->fault_level = (SuperCapYq2027FaultLevel_e)(status & 0x03u);

    out->chassis_power_w = SuperCapYq2027DecodePower(SuperCapYq2027ReadU16Le(&d[1]));
    out->referee_power_w = SuperCapYq2027DecodePower(SuperCapYq2027ReadU16Le(&d[3]));
    out->chassis_power_limit_w = SuperCapYq2027ReadU16Le(&d[5]);
    out->cap_energy_raw = d[7];
    out->cap_energy_percent = SuperCapYq2027DecodeCapPercent(d[7]);
}

/*============================================
 *              CAN 接收回调（ISR）
 *
 * 就地解析，写进双缓冲里"当前 ISR 正在写"的那一份，写完翻索引。
 * 读者读另一份，永远拿到完整的一帧。
 *============================================*/
static void SuperCapYq2027RxCallback(CANInstance *can, const CAN_Pack_s *pack)
{
    if (can == NULL || can->parent == NULL || pack == NULL)
        return;

    SuperCapYq2027_s *inst = (SuperCapYq2027_s *)can->parent;

    /* 只接受标准数据帧 */
    if (pack->frame_type != CAN_STANDARD_DATA_FRAME)
    {
        inst->rx_id_err++;
        return;
    }

    if (pack->len != SUPER_CAP_YQ2027_FRAME_SIZE)
    {
        inst->rx_len_err++;
        return;
    }

    /* 过滤之外再校验一次 ID：软件过滤器是共享的，不能假设一定命中 */
    if (pack->id != inst->rx_id)
    {
        inst->rx_id_err++;
        return;
    }

    SuperCapYq2027Data_s *out = &inst->data[inst->data_idx];

    /* 先清零：字段增删时不会残留上一帧数据 */
    memset(out, 0, sizeof(*out));
    SuperCapYq2027ParseFeedback(pack->data, out);

    out->timestamp_us = DWT_GetTimeUs();

    inst->last_status_code = out->status_code;
    inst->rx_cnt++;

    /* 写完才翻索引：读者看不到半截帧 */
    inst->data_idx = (uint8_t)(!inst->data_idx);

    /* 只有通过校验的有效帧才喂狗 */
    if (inst->daemon != NULL)
        DaemonReload(inst->daemon);
}

/*============================================
 *              发送完成 / 总线错误（drv 层分流）
 *============================================*/

/* 逐帧发送结果：result != BSP_OK 表示这一帧确实没发出去 */
static void SuperCapYq2027TxHook(CANInstance *can, uint32_t tx_mailbox, BSP_Status_e result)
{
    (void)tx_mailbox;

    if (can == NULL || can->parent == NULL || result == BSP_OK)
        return;

    SuperCapYq2027_s *inst = (SuperCapYq2027_s *)can->parent;
    inst->tx_fail++;
}

/* 总线/硬件级事件：err_callback 是同一条总线的广播，
 * 不属于单个实例，故只计数、不做实例级复位或总线恢复。 */
static void SuperCapYq2027ErrHook(CANInstance *can, CAN_ErrReason_e reason)
{
    if (can == NULL || can->parent == NULL)
        return;

    SuperCapYq2027_s *inst = (SuperCapYq2027_s *)can->parent;

    inst->bus_err_cnt++;
    if (reason == CAN_ERR_BUS_OFF)
        inst->bus_off_cnt++;
}

/*============================================
 *              公共接口
 *============================================*/

int8_t SuperCapYq2027Register(SuperCapYq2027_s *inst)
{
    if (inst == NULL || inst->can == NULL)
        return -1;

    if (inst->registered != 0u)
        return -1;

    if (CANRegister(inst->can) != BSP_OK)
        return -1;

    /* Config 也会设置 parent；先在这里绑定，保证过滤回调可用 */
    inst->can->parent = inst;
    inst->registered = 1;

    if (inst->daemon != NULL)
        DaemonRegister(inst->daemon);

    return 0;
}

int8_t SuperCapYq2027Config(SuperCapYq2027_s *inst, const SuperCapYq2027Config_s *cfg)
{
    if (inst == NULL || cfg == NULL || inst->can == NULL)
        return -1;

    if (inst->registered == 0u)
        return -1;

    /* 标准帧 ID 校验 */
    if (cfg->tx_id == CAN_ID_UNUSED || cfg->tx_id > SUPER_CAP_YQ2027_STD_ID_MAX)
        return -1;
    if (cfg->rx_id == CAN_ID_UNUSED || cfg->rx_id > SUPER_CAP_YQ2027_STD_ID_MAX)
        return -1;

    inst->can_filter = (CAN_Filter_s){
        .mode = CAN_FILTER_MODE_LIST,
        .id0 = cfg->rx_id,
        .id1 = CAN_ID_UNUSED,
        .frame_type = CAN_STANDARD_DATA_FRAME,
        .callback = SuperCapYq2027RxCallback,
    };

    CAN_Config_s can_cfg = {
        .can_e = cfg->can_e,
        .mode = CAN_FRAME_FORMAT_CLASSIC,
        .parent = inst, /* CANConfig 会覆盖 parent，必须显式设置 */
        .filters = &inst->can_filter,
        .filter_num = 1,
        .tx_complete_callback = SuperCapYq2027TxHook,
        .err_callback = SuperCapYq2027ErrHook,
    };

    if (CANConfig(inst->can, &can_cfg) != BSP_OK)
        return -1;

    inst->tx_id = cfg->tx_id;
    inst->rx_id = cfg->rx_id;
    inst->timeout_ms = cfg->timeout_ms;

    /* 清空反馈快照；计数保留（生命周期累计值） */
    memset(inst->data, 0, sizeof(inst->data));
    inst->data_idx = 0;
    inst->last_status_code = 0;
    inst->configured = 1;

    if (inst->daemon != NULL)
    {
        Daemon_Config_s daemon_cfg = {
            .reload_count = cfg->reload_count,
            .fault_action = cfg->fault_action,
            .callback = NULL, /* 不把总线级恢复挂在实例级离线证据上 */
            .owner_id = inst,
        };
        DaemonConfig(inst->daemon, &daemon_cfg);
    }

    return 0;
}

BSP_Status_e SuperCapYq2027Send(SuperCapYq2027_s *inst, const SuperCapYq2027Command_s *cmd)
{
    if (inst == NULL || cmd == NULL || inst->can == NULL || inst->configured == 0u)
        return BSP_PARAM_ERR;

    /* 按 D0~D7 显式打包：不依赖 C 位域的编译器布局，
     * 保留位 resv0/resv1/resv2 一律发 0（协议要求超电忽略，但发 0 最稳）。 */
    uint8_t raw[SUPER_CAP_YQ2027_FRAME_SIZE] = {0};

    if (cmd->enable_dcdc)
        raw[0] |= SUPER_CAP_YQ2027_D0_ENABLE_DCDC;
    if (cmd->system_restart)
        raw[0] |= SUPER_CAP_YQ2027_D0_SYSTEM_RESTART;
    if (cmd->clear_error)
        raw[0] |= SUPER_CAP_YQ2027_D0_CLEAR_ERROR;
    if (cmd->enable_active_charging_limit)
        raw[0] |= SUPER_CAP_YQ2027_D0_ENABLE_ACTIVE_LIMIT;

    raw[1] = (uint8_t)(cmd->referee_power_limit & 0xFFu);
    raw[2] = (uint8_t)(cmd->referee_power_limit >> 8);
    raw[3] = (uint8_t)(cmd->referee_energy_buffer & 0xFFu);
    raw[4] = (uint8_t)(cmd->referee_energy_buffer >> 8);
    raw[5] = cmd->active_charging_limit_ratio;
    raw[6] = 0;
    raw[7] = 0;

    CAN_Pack_s pack;
    memset(&pack, 0, sizeof(pack));
    pack.id = inst->tx_id;
    pack.frame_type = CAN_STANDARD_DATA_FRAME;
    pack.len = SUPER_CAP_YQ2027_FRAME_SIZE;
    memcpy(pack.data, raw, SUPER_CAP_YQ2027_FRAME_SIZE);

    inst->tx_cnt++;

    BSP_Status_e ret = CANTransmit(inst->can, &pack, inst->timeout_ms, NULL, NULL);

    if (ret != BSP_OK)
    {
        /* 入队失败；入队成功后的逐帧失败由 TxHook 计入同一个计数 */
        inst->tx_fail++;
    }

    return ret;
}

SuperCapYq2027Data_s SuperCapYq2027GetData(const SuperCapYq2027_s *inst)
{
    SuperCapYq2027Data_s zero;
    memset(&zero, 0, sizeof(zero));

    if (inst == NULL)
        return zero;

    /* ISR 写 data[data_idx]，这里读另一份 */
    return inst->data[!inst->data_idx];
}

#endif /* HAL_CAN_MODULE_ENABLED || HAL_FDCAN_MODULE_ENABLED */
#endif /* DRV_SUPER_CAP_YQ2027_USED */

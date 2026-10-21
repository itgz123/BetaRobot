/**
 * @file test_drv_super_cap_yq2027.c
 * @brief drv_super_cap_yq2027 离线测试（主机 gcc，直接编译真实驱动实现）
 *
 * 协议：drv/drv_super_cap_yq2027/supercap-can-interface.md（2026-10-21 版）
 *   主控 → 超电 0x061；超电 → 主控 0x052（仅新格式）
 *
 * 覆盖：
 *   1. 文档给出的命令 / 反馈打包示例逐字节校验
 *   2. D0~D7 显式打包、保留位强制 0
 *   3. 功率 raw/64-256（含负值与饱和边界）
 *   4. capEnergy 满量程 250
 *   5. statusCode 各位解码
 *   6. 长度 / ID / 帧类型异常（含拒绝旧 0x051）
 *   7. daemon 配置、喂狗与在线查询
 *   8. 发送失败与总线错误计数
 *   9. 非法调用、重复配置
 */

#include "app_cfg.h"
#include "bsp_can.h"
#include "bsp_dwt.h"
#include "drv_daemon.h"
#include "drv_super_cap_yq2027.h"

#include <stdio.h>
#include <string.h>

/*============================================
 *              测试桩
 *============================================*/

TestCanState g_can;
TestDaemonState g_daemon;

static uint64_t s_fake_us = 1000000u;

uint64_t DWT_GetTimeUs(void)
{
    s_fake_us += 100u;
    return s_fake_us;
}

BSP_Status_e CANRegister(CANInstance *instance)
{
    if (instance == NULL)
        return BSP_PARAM_ERR;
    g_can.register_calls++;
    return BSP_OK;
}

BSP_Status_e CANConfig(CANInstance *instance, const CAN_Config_s *config)
{
    if (instance == NULL || config == NULL)
        return BSP_PARAM_ERR;

    instance->can_e = config->can_e;
    instance->mode = config->mode;
    instance->parent = config->parent;
    instance->filters = config->filters;
    instance->filter_num = config->filter_num;
    instance->tx_complete_callback = config->tx_complete_callback;
    instance->err_callback = config->err_callback;

    g_can.config_calls++;
    g_can.last_config = *config;
    return BSP_OK;
}

BSP_Status_e CANTransmit(CANInstance *instance, const CAN_Pack_s *pack, uint32_t timeout_ms, uint32_t *tx_mailbox, uint8_t *tx_free_level)
{
    (void)instance;
    (void)timeout_ms;
    (void)tx_mailbox;
    (void)tx_free_level;

    if (pack == NULL)
        return BSP_PARAM_ERR;

    g_can.transmit_calls++;
    g_can.last_pack = *pack;
    return g_can.transmit_result;
}

void DaemonConfig(DaemonInstance *inst, const Daemon_Config_s *config)
{
    if (inst == NULL || config == NULL)
        return;

    inst->reload_count = config->reload_count;
    inst->fault_action = config->fault_action;
    inst->callback = config->callback;
    inst->owner_id = config->owner_id;

    g_daemon.config_calls++;
    g_daemon.last_config = *config;
}

void DaemonRegister(DaemonInstance *inst)
{
    if (inst == NULL)
        return;
    g_daemon.register_calls++;
}

void DaemonReload(DaemonInstance *instance)
{
    if (instance == NULL)
        return;
    g_daemon.reload_calls++;
}

uint8_t DaemonIsOnline(DaemonInstance *instance)
{
    (void)instance;
    return g_daemon.online_ret;
}

/*============================================
 *              断言
 *============================================*/

static int s_checks;
static int s_failures;

#define CHECK(cond, ...)                                                                                                                                       \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        s_checks++;                                                                                                                                            \
        if (!(cond))                                                                                                                                           \
        {                                                                                                                                                      \
            s_failures++;                                                                                                                                      \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                                                                        \
            printf(__VA_ARGS__);                                                                                                                               \
            printf("\n");                                                                                                                                      \
        }                                                                                                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, eps, ...) CHECK(((a) - (b) < (eps)) && ((b) - (a) < (eps)), __VA_ARGS__)

/*============================================
 *              测试数据构造
 *============================================*/

static void put_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

/* 按协议编码：raw = clamp(P*64+16384, 0, 65535)，这里只用于构造测试帧 */
static uint16_t encode_power(float watts)
{
    int32_t raw = (int32_t)(watts * 64.0f) + 16384;
    if (raw < 0)
        raw = 0;
    if (raw > 65535)
        raw = 65535;
    return (uint16_t)raw;
}

static void build_feedback(uint8_t *d, uint8_t status, uint16_t chassis_raw, uint16_t referee_raw, uint16_t limit, uint8_t cap)
{
    memset(d, 0, SUPER_CAP_YQ2027_FRAME_SIZE);
    d[0] = status;
    put_u16_le(&d[1], chassis_raw);
    put_u16_le(&d[3], referee_raw);
    put_u16_le(&d[5], limit);
    d[7] = cap;
}

/* 模拟 CAN 软件过滤器命中后调用驱动回调 */
static void feed(CANInstance *can, uint32_t id, CAN_Frame_Type_e frame_type, const uint8_t *d, uint8_t len)
{
    CAN_Pack_s pack;

    memset(&pack, 0, sizeof(pack));
    pack.id = id;
    pack.frame_type = frame_type;
    pack.len = len;
    if (d != NULL && len > 0u)
        memcpy(pack.data, d, len);

    if (can->filters != NULL && can->filters[0].callback != NULL)
        can->filters[0].callback(can, &pack);
}

/*============================================
 *              实例
 *============================================*/

SUPER_CAP_YQ2027_INSTANCE_DEF(cap);
SUPER_CAP_YQ2027_INSTANCE_DEF(cap_alt);
SUPER_CAP_YQ2027_INSTANCE_DEF(cap_bad);
SUPER_CAP_YQ2027_INSTANCE_DEF(cap_unreg);

/*============================================
 *              主测试
 *============================================*/

int main(void)
{
    SuperCapYq2027Config_s cfg = {
        .can_e = CAN_1,
        .tx_id = SUPER_CAP_YQ2027_TX_ID_DEFAULT,
        .rx_id = SUPER_CAP_YQ2027_RX_ID_DEFAULT,
        .timeout_ms = 1,
        .reload_count = 100,
        .fault_action = DAEMON_FAULT_NONE,
    };

    CHECK(SUPER_CAP_YQ2027_TX_ID_DEFAULT == 0x061u, "default tx id must be 0x061");
    CHECK(SUPER_CAP_YQ2027_RX_ID_DEFAULT == 0x052u, "default rx id must be 0x052");
    CHECK(sizeof(SuperCapYq2027Command_s) == 8u, "command struct must be 8 bytes");
    CHECK(sizeof(SuperCapYq2027FeedbackFrame_s) == 8u, "feedback struct must be 8 bytes");

    /*================ 1. 注册 / 配置 ================*/
    CHECK(SuperCapYq2027Register(&cap) == 0, "register should succeed");
    CHECK(SuperCapYq2027Register(&cap) == -1, "duplicate register should fail");
    CHECK(g_can.register_calls == 1, "CANRegister called %d times", g_can.register_calls);
    CHECK(g_daemon.register_calls == 1, "DaemonRegister called %d times", g_daemon.register_calls);
    CHECK(g_can.transmit_calls == 0, "register must not transmit");

    CHECK(SuperCapYq2027Config(&cap, &cfg) == 0, "config should succeed");
    CHECK(g_can.config_calls == 1, "CANConfig called %d times", g_can.config_calls);
    CHECK(g_can.transmit_calls == 0, "config must not transmit");
    CHECK(g_can.last_config.parent == &cap, "CANConfig parent not bound");
    CHECK(g_can.last_config.can_e == CAN_1, "CAN bus mismatch");
    CHECK(g_can.last_config.mode == CAN_FRAME_FORMAT_CLASSIC, "CAN mode must be classic");
    CHECK(g_can.last_config.filter_num == 1u, "expected 1 filter, got %u", (unsigned)g_can.last_config.filter_num);
    CHECK(g_can.last_config.filters != NULL, "filter array missing");
    if (g_can.last_config.filters != NULL)
    {
        CHECK(g_can.last_config.filters[0].mode == CAN_FILTER_MODE_LIST, "filter mode");
        CHECK(g_can.last_config.filters[0].id0 == 0x052u, "rx filter id = 0x%03X", (unsigned)g_can.last_config.filters[0].id0);
        CHECK(g_can.last_config.filters[0].id1 == CAN_ID_UNUSED, "filter id1");
        CHECK(g_can.last_config.filters[0].frame_type == CAN_STANDARD_DATA_FRAME, "filter frame type");
        CHECK(g_can.last_config.filters[0].callback != NULL, "filter callback missing");
    }
    CHECK(cap.can->parent == &cap, "instance parent not bound");
    CHECK(cap.tx_id == 0x061u, "tx_id stored");
    CHECK(cap.rx_id == 0x052u, "rx_id stored");

    CHECK(g_daemon.config_calls == 1, "DaemonConfig called %d times", g_daemon.config_calls);
    CHECK(g_daemon.last_config.reload_count == 100, "daemon reload_count");
    CHECK(g_daemon.last_config.fault_action == DAEMON_FAULT_NONE, "daemon fault action");
    CHECK(g_daemon.last_config.callback == NULL, "daemon callback must be NULL");
    CHECK(g_daemon.last_config.owner_id == &cap, "daemon owner_id");

    /*================ 2. 文档示例：运行命令 ================*/
    g_can.transmit_result = BSP_OK;

    SuperCapYq2027Command_s cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.enable_dcdc = 1;
    cmd.referee_power_limit = 80;
    cmd.referee_energy_buffer = 57;
    cmd.active_charging_limit_ratio = 255;

    int tx0 = g_can.transmit_calls;
    uint32_t tx_cnt0 = cap.tx_cnt;
    BSP_Status_e ret = SuperCapYq2027Send(&cap, &cmd);

    static const uint8_t run_expect[8] = {0x01, 0x50, 0x00, 0x39, 0x00, 0xFF, 0x00, 0x00};
    static const uint8_t stop_expect[8] = {0x00, 0x50, 0x00, 0x39, 0x00, 0xFF, 0x00, 0x00};

    CHECK(ret == BSP_OK, "send should succeed, ret=%d", (int)ret);
    CHECK(g_can.transmit_calls == tx0 + 1, "transmit count");
    CHECK(cap.tx_cnt == tx_cnt0 + 1, "tx_cnt");
    CHECK(cap.tx_fail == 0, "unexpected tx_fail=%u", (unsigned)cap.tx_fail);
    CHECK(g_can.last_pack.id == 0x061u, "tx id = 0x%03X", (unsigned)g_can.last_pack.id);
    CHECK(g_can.last_pack.len == 8u, "tx len = %u", (unsigned)g_can.last_pack.len);
    CHECK(g_can.last_pack.frame_type == CAN_STANDARD_DATA_FRAME, "tx frame type");
    CHECK(memcmp(g_can.last_pack.data, run_expect, 8) == 0, "run command bytes != doc example");

    /*================ 3. 文档示例：停机命令 ================*/
    cmd.enable_dcdc = 0;
    CHECK(SuperCapYq2027Send(&cap, &cmd) == BSP_OK, "send stop command");
    CHECK(memcmp(g_can.last_pack.data, stop_expect, 8) == 0, "stop command bytes != doc example");

    /*================ 4. 全部标志位与保留位 ================*/
    memset(&cmd, 0, sizeof(cmd));
    cmd.enable_dcdc = 1;
    cmd.system_restart = 1;
    cmd.clear_error = 1;
    cmd.enable_active_charging_limit = 1;
    cmd.resv0 = 7;      /* 应被忽略 */
    cmd.resv1 = 1;      /* 应被忽略 */
    cmd.resv2 = 0x1234; /* 应被忽略 */
    cmd.referee_power_limit = 0x1234u;
    cmd.referee_energy_buffer = 0xABCDu;
    cmd.active_charging_limit_ratio = 0x5Au;

    CHECK(SuperCapYq2027Send(&cap, &cmd) == BSP_OK, "send all flags");
    /* bit0|bit1|bit5|bit6 = 0x63；bit7 与其他保留位强制 0 */
    CHECK(g_can.last_pack.data[0] == 0x63u, "flags byte = 0x%02X, expected 0x63", g_can.last_pack.data[0]);
    CHECK(g_can.last_pack.data[1] == 0x34u && g_can.last_pack.data[2] == 0x12u, "power limit LE");
    CHECK(g_can.last_pack.data[3] == 0xCDu && g_can.last_pack.data[4] == 0xABu, "energy buffer LE");
    CHECK(g_can.last_pack.data[5] == 0x5Au, "charging ratio");
    CHECK(g_can.last_pack.data[6] == 0u && g_can.last_pack.data[7] == 0u, "reserved bytes must be zero");

    /*================ 5. 文档示例：状态反馈 ================*/
    uint8_t frame[8];
    static const uint8_t fb_expect[8] = {0xD0, 0x00, 0x59, 0x00, 0x54, 0xF4, 0x01, 0x7D};
    memcpy(frame, fb_expect, 8);

    uint32_t rx0 = cap.rx_cnt;
    int reload0 = g_daemon.reload_calls;
    feed(cap.can, 0x052u, CAN_STANDARD_DATA_FRAME, frame, 8u);

    SuperCapYq2027Data_s fb = SuperCapYq2027GetData(&cap);
    CHECK(cap.rx_cnt == rx0 + 1u, "valid frame should count");
    CHECK(g_daemon.reload_calls == reload0 + 1, "valid frame should reload daemon");
    CHECK(fb.status_code == 0xD0u, "status=0x%02X", fb.status_code);
    CHECK(fb.dcdc_enabled == 1u, "DCDC output bit should be 1");
    CHECK(fb.protocol_valid == 1u, "protocol bit should be 1");
    CHECK(fb.wireless_state == SUPER_CAP_YQ2027_WIRELESS_OFF, "wireless state = %u", (unsigned)fb.wireless_state);
    CHECK(fb.limit_reason == SUPER_CAP_YQ2027_LIMIT_UPSTREAM_POWER, "limit reason = %u", (unsigned)fb.limit_reason);
    CHECK(fb.fault_level == SUPER_CAP_YQ2027_FAULT_NONE, "fault level = %u", (unsigned)fb.fault_level);
    CHECK_NEAR(fb.chassis_power_w, 100.0f, 0.001f, "chassis power = %f", (double)fb.chassis_power_w);
    CHECK_NEAR(fb.referee_power_w, 80.0f, 0.001f, "referee power = %f", (double)fb.referee_power_w);
    CHECK(fb.chassis_power_limit_w == 500u, "limit = %u", (unsigned)fb.chassis_power_limit_w);
    CHECK(fb.cap_energy_raw == 125u, "cap raw = %u", (unsigned)fb.cap_energy_raw);
    CHECK_NEAR(fb.cap_energy_percent, 50.0f, 0.01f, "cap percent = %f", (double)fb.cap_energy_percent);
    CHECK(fb.timestamp_us != 0u, "timestamp must be non-zero");

    /*================ 6. statusCode 全字段解码 ================*/
    /* bit7=1, bit6=1, bit5:4=2(充电), bit3:2=3(电容限流), bit1:0=2(手动清除) */
    build_feedback(frame, 0xEEu, encode_power(0.0f), encode_power(0.0f), 0u, 0u);
    feed(cap.can, 0x052u, CAN_STANDARD_DATA_FRAME, frame, 8u);
    fb = SuperCapYq2027GetData(&cap);
    CHECK(fb.dcdc_enabled == 1u, "all-field dcdc");
    CHECK(fb.protocol_valid == 1u, "all-field protocol");
    CHECK(fb.wireless_state == SUPER_CAP_YQ2027_WIRELESS_CHARGING, "all-field wireless = %u", (unsigned)fb.wireless_state);
    CHECK(fb.limit_reason == SUPER_CAP_YQ2027_LIMIT_CAP_CURRENT, "all-field limit = %u", (unsigned)fb.limit_reason);
    CHECK(fb.fault_level == SUPER_CAP_YQ2027_FAULT_MANUAL_CLEAR, "all-field fault = %u", (unsigned)fb.fault_level);

    /* bit6=0：仍未收到本协议反馈 */
    build_feedback(frame, 0x01u, encode_power(0.0f), encode_power(0.0f), 0u, 0u);
    feed(cap.can, 0x052u, CAN_STANDARD_DATA_FRAME, frame, 8u);
    fb = SuperCapYq2027GetData(&cap);
    CHECK(fb.protocol_valid == 0u, "protocol invalid when bit6=0");
    CHECK(fb.fault_level == SUPER_CAP_YQ2027_FAULT_AUTO_RECOVER, "fault auto recover");
    CHECK(fb.wireless_state == SUPER_CAP_YQ2027_WIRELESS_ABNORMAL, "wireless abnormal");

    /*================ 7. 功率 / 电量边界 ================*/
    build_feedback(frame, 0xD0u, 0x0000u, 0xFFFFu, 0xFFFFu, 250u);
    feed(cap.can, 0x052u, CAN_STANDARD_DATA_FRAME, frame, 8u);
    fb = SuperCapYq2027GetData(&cap);
    CHECK_NEAR(fb.chassis_power_w, -256.0f, 0.001f, "min power = %f", (double)fb.chassis_power_w);
    CHECK_NEAR(fb.referee_power_w, 767.984375f, 0.001f, "max power = %f", (double)fb.referee_power_w);
    CHECK(fb.chassis_power_limit_w == 65535u, "max limit");
    CHECK_NEAR(fb.cap_energy_percent, 100.0f, 0.01f, "250 raw must be 100%%");

    build_feedback(frame, 0xD0u, encode_power(-12.5f), encode_power(12.5f), 0u, 255u);
    feed(cap.can, 0x052u, CAN_STANDARD_DATA_FRAME, frame, 8u);
    fb = SuperCapYq2027GetData(&cap);
    CHECK_NEAR(fb.chassis_power_w, -12.5f, 0.001f, "negative power = %f", (double)fb.chassis_power_w);
    CHECK_NEAR(fb.referee_power_w, 12.5f, 0.001f, "positive power = %f", (double)fb.referee_power_w);
    CHECK_NEAR(fb.cap_energy_percent, 102.0f, 0.01f, "raw 255 -> 102%%");

    /*================ 8. 长度 / ID / 帧类型异常 ================*/
    uint32_t len_err0 = cap.rx_len_err;
    uint32_t id_err0 = cap.rx_id_err;
    uint32_t rx_before = cap.rx_cnt;
    int reload_before = g_daemon.reload_calls;
    fb = SuperCapYq2027GetData(&cap);

    build_feedback(frame, 0x55u, encode_power(1.0f), encode_power(1.0f), 1u, 1u);
    feed(cap.can, 0x052u, CAN_STANDARD_DATA_FRAME, frame, 7u);   /* 长度错 */
    feed(cap.can, 0x052u, CAN_STANDARD_DATA_FRAME, frame, 9u);   /* 长度错 */
    feed(cap.can, 0x051u, CAN_STANDARD_DATA_FRAME, frame, 8u);   /* 旧 ID 已废弃 */
    feed(cap.can, 0x053u, CAN_STANDARD_DATA_FRAME, frame, 8u);   /* 未知 ID */
    feed(cap.can, 0x052u, CAN_EXTENDED_DATA_FRAME, frame, 8u);   /* 扩展帧 */
    feed(cap.can, 0x052u, CAN_STANDARD_REMOTE_FRAME, frame, 8u); /* 远程帧 */

    CHECK(cap.rx_len_err == len_err0 + 2u, "rx_len_err=%u", (unsigned)cap.rx_len_err);
    CHECK(cap.rx_id_err == id_err0 + 4u, "rx_id_err=%u", (unsigned)cap.rx_id_err);
    CHECK(cap.rx_cnt == rx_before, "bad frames must not count as valid");
    CHECK(g_daemon.reload_calls == reload_before, "bad frames must not reload daemon");
    SuperCapYq2027Data_s fb_after = SuperCapYq2027GetData(&cap);
    CHECK(memcmp(&fb, &fb_after, sizeof(fb)) == 0, "bad frames must not overwrite snapshot");

    /*================ 9. 自定义 ID / 重复 Config ================*/
    CHECK(SuperCapYq2027Register(&cap_alt) == 0, "alt register");

    SuperCapYq2027Config_s alt_cfg = cfg;
    alt_cfg.tx_id = 0x123u;
    alt_cfg.rx_id = 0x456u;
    alt_cfg.reload_count = 0;
    CHECK(SuperCapYq2027Config(&cap_alt, &alt_cfg) == 0, "alt config");
    CHECK(g_can.last_config.filters[0].id0 == 0x456u, "alt filter id = 0x%03X", (unsigned)g_can.last_config.filters[0].id0);
    CHECK(cap_alt.tx_id == 0x123u, "alt tx id");
    CHECK(cap_alt.rx_id == 0x456u, "alt rx id");
    CHECK(g_daemon.last_config.reload_count == 0u, "alt daemon disabled");

    memset(&cmd, 0, sizeof(cmd));
    CHECK(SuperCapYq2027Send(&cap_alt, &cmd) == BSP_OK, "alt send");
    CHECK(g_can.last_pack.id == 0x123u, "alt tx id on wire = 0x%03X", (unsigned)g_can.last_pack.id);

    build_feedback(frame, 0xD0u, encode_power(7.0f), encode_power(8.0f), 9u, 10u);
    feed(cap_alt.can, 0x456u, CAN_STANDARD_DATA_FRAME, frame, 8u);
    fb = SuperCapYq2027GetData(&cap_alt);
    CHECK_NEAR(fb.chassis_power_w, 7.0f, 0.001f, "alt chassis = %f", (double)fb.chassis_power_w);
    CHECK_NEAR(fb.referee_power_w, 8.0f, 0.001f, "alt referee = %f", (double)fb.referee_power_w);

    uint32_t alt_err0 = cap_alt.rx_id_err;
    feed(cap_alt.can, 0x052u, CAN_STANDARD_DATA_FRAME, frame, 8u);
    CHECK(cap_alt.rx_id_err == alt_err0 + 1u, "alt must reject default 0x052");

    /* 重复 Config：改 rx_id 后过滤器随之更新，反馈快照清空 */
    alt_cfg.rx_id = 0x457u;
    CHECK(SuperCapYq2027Config(&cap_alt, &alt_cfg) == 0, "alt reconfig");
    CHECK(g_can.last_config.filters[0].id0 == 0x457u, "reconfig filter id");
    fb = SuperCapYq2027GetData(&cap_alt);
    CHECK(fb.timestamp_us == 0u, "reconfig must clear snapshot");

    /*================ 10. 发送失败 / 总线错误 ================*/
    uint32_t fail0 = cap.tx_fail;
    int tx_before_fail = g_can.transmit_calls;

    g_can.transmit_result = BSP_BUSY;
    memset(&cmd, 0, sizeof(cmd));
    ret = SuperCapYq2027Send(&cap, &cmd);
    CHECK(ret == BSP_BUSY, "send should return BSP_BUSY, ret=%d", (int)ret);
    CHECK(g_can.transmit_calls == tx_before_fail + 1, "transmit attempted");
    CHECK(cap.tx_fail == fail0 + 1u, "enqueue failure not counted");

    g_can.transmit_result = BSP_OK;
    cap.can->tx_complete_callback(cap.can, 0u, BSP_HW_ERR);
    CHECK(cap.tx_fail == fail0 + 2u, "per-frame failure not counted");

    uint32_t bus0 = cap.bus_err_cnt;
    uint32_t off0 = cap.bus_off_cnt;
    cap.can->err_callback(cap.can, CAN_ERR_BUS_OFF);
    cap.can->err_callback(cap.can, CAN_ERR_ERROR_PASSIVE);
    CHECK(cap.bus_err_cnt == bus0 + 2u, "bus errors not counted");
    CHECK(cap.bus_off_cnt == off0 + 1u, "bus-off not counted");

    /*================ 11. 非法调用 ================*/
    CHECK(SuperCapYq2027Register(NULL) == -1, "register NULL");
    CHECK(SuperCapYq2027Config(NULL, &cfg) == -1, "config NULL instance");
    CHECK(SuperCapYq2027Config(&cap, NULL) == -1, "config NULL cfg");
    CHECK(SuperCapYq2027Config(&cap_unreg, &cfg) == -1, "config before register");

    memset(&cmd, 0, sizeof(cmd));
    CHECK(SuperCapYq2027Send(NULL, &cmd) == BSP_PARAM_ERR, "send NULL instance");
    CHECK(SuperCapYq2027Send(&cap, NULL) == BSP_PARAM_ERR, "send NULL cmd");
    CHECK(SuperCapYq2027Send(&cap_unreg, &cmd) == BSP_PARAM_ERR, "send before config");

    SuperCapYq2027Config_s bad_cfg;
    CHECK(SuperCapYq2027Register(&cap_bad) == 0, "bad instance register");

    bad_cfg = cfg;
    bad_cfg.tx_id = CAN_ID_UNUSED;
    CHECK(SuperCapYq2027Config(&cap_bad, &bad_cfg) == -1, "unused tx id must fail");

    bad_cfg = cfg;
    bad_cfg.tx_id = 0x800u;
    CHECK(SuperCapYq2027Config(&cap_bad, &bad_cfg) == -1, "tx id out of range must fail");

    bad_cfg = cfg;
    bad_cfg.rx_id = CAN_ID_UNUSED;
    CHECK(SuperCapYq2027Config(&cap_bad, &bad_cfg) == -1, "unused rx id must fail");

    bad_cfg = cfg;
    bad_cfg.rx_id = 0x800u;
    CHECK(SuperCapYq2027Config(&cap_bad, &bad_cfg) == -1, "rx id out of range must fail");

    CHECK(SuperCapYq2027Send(&cap_bad, &cmd) == BSP_PARAM_ERR, "send after failed config");

    /*================ 12. 在线查询 ================*/
    g_daemon.online_ret = 1;
    CHECK(SuperCapYq2027IsOnline(&cap) == 1u, "online should be 1");
    g_daemon.online_ret = 0;
    CHECK(SuperCapYq2027IsOnline(&cap) == 0u, "online should be 0");
    CHECK(SuperCapYq2027IsOnline(NULL) == 0u, "online NULL");

    /* GetData(NULL) 必须全 0 */
    SuperCapYq2027Data_s zero = SuperCapYq2027GetData(NULL);
    const uint8_t *zp = (const uint8_t *)&zero;
    int all_zero = 1;
    for (size_t i = 0; i < sizeof(zero); i++)
    {
        if (zp[i] != 0u)
        {
            all_zero = 0;
            break;
        }
    }
    CHECK(all_zero, "GetData(NULL) must be all zero");

    /*================ 汇总 ================*/
    printf("checks=%d failures=%d\n", s_checks, s_failures);
    if (s_failures == 0)
        printf("PASS\n");
    else
        printf("FAIL\n");

    return (s_failures == 0) ? 0 : 1;
}

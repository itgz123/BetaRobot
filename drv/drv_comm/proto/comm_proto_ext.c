/**
 * @file comm_proto_ext.c
 * @brief 通信框架-协议层（Protocol）扩展缩短汉明码 + CRC8 帧协议后端实现
 *
 * 帧格式（N = payload_size，E = 编码区字节数，均编译期确定）：
 *   [帧头 0xA5] [seq 1B] [编码区 E B] [CRC8 1B] [帧尾 0x5A]
 *   CRC8 校验范围 = seq + 编码区（帧头后到 CRC 前），poly 0x07、init 0x00，
 *   由 lib_crc 提供（Flash 表 LIB_CRC_TBL_CRC8，每字节 1 次查表）。
 *
 * 层次（"先过汉明码、再算 CRC"）：
 *   发：payload --lib_hamming_ext--> 编码区 --lib_crc--> CRC8
 *   收：CRC8 相符 ⇒ 无错，汉明解码即得 payload（不触发纠错）
 *       CRC8 不符 ⇒ 汉明纠错 → 用纠错后的 payload 重编码 → 与原 CRC 复核，过则采信。
 *       复核这一步是必须的：SECDED 在 ≥3 位错时可能误纠，只有"纠错后的码字能重现
 *       发送端 CRC"才说明真的纠对了；对不上（或错在 seq/CRC 字节本身）整帧丢弃。
 *
 * 分块：块数与每块长度只由 payload 长度决定。编译期用 PROTO_EXT_BLKS / _BLK_BYTES / _BYTES
 *       定死缓冲尺寸并做 _Static_assert（见 comm_proto_ext.h 的 DEF 宏）；CommProtoExtInit
 *       在 config 阶段把 cfg.m / cfg.k_s / enc_bytes 算一次存进实例；运行期 pack/unpack
 *       只按存好的值逐块转调 lib_hamming —— 不重算块数、不做除法、无动态分配。
 */

#include "comm_proto_ext.h"

#ifdef DRV_COMM_USED

#ifndef LIB_HAMMING_USED
#error "comm_proto_ext 的汉明纠错依赖 lib_hamming（LIB_HAMMING_USED），请启用 LIB_HAMMING_USED"
#endif

/* CRC8（poly 0x07、init 0x00、MSB-first、无反射无异或）两档实现，均来自 lib_crc：
 *   1) lib_crc 表已编译（LIB_CRC_USED + LIB_CRC_TABLES_USED）→ 查表 LIB_CRC_TableCalc
 *   2) 表未编译 → 逐位 LIB_CRC_Direct
 * 协议 CRC 强制依赖 lib_crc（不内置手写实现）；未启用 LIB_CRC_USED 则编译报错。
 * 校验范围统一为 seq 起（&data[1]）到编码区末，长度 E + 1 */
#if defined(LIB_CRC_USED) && defined(LIB_CRC_TABLES_USED)
#include "lib_crc.h"
#include "lib_crc_tables.h"
#define PROTO_EXT_CRC8(data, len) ((uint8_t)LIB_CRC_TableCalc(&LIB_CRC_TBL_CRC8, (data), (len)))
#elif defined(LIB_CRC_USED)
#include "lib_crc.h"
static const LIB_CRC_Algo_t s_ext_crc8_algo = {0x00, 8, 0x07, 0x00, 0, 0}; /* CRC-8: poly 0x07/init 0x00 非反射 */
#define PROTO_EXT_CRC8(data, len) ((uint8_t)LIB_CRC_Direct(&s_ext_crc8_algo, (data), (len)))
#else
#error "comm_proto_ext 的 CRC8 依赖 lib_crc（LIB_CRC_USED），请启用 LIB_CRC_USED"
#endif /* LIB_CRC_USED / LIB_CRC_TABLES_USED */

static int8_t ExtPack(CommProto *self, const uint8_t *payload, uint8_t *out_buff);
static const uint8_t *ExtUnpack(CommProto *self, const uint8_t *data);
static void ExtReset(CommProto *self);

static const CommProtoVTable_s s_ext_vtable = {
    .pack = ExtPack,
    .unpack = ExtUnpack,
    .reset = ExtReset,
};

/* 帧序列检测（与 PROTO_CUSTOM 同语义，见 comm_proto_custom.h）
 * @return 0 接受（并更新 rx_last_seq）；-1 重帧，丢弃（不更新） */
static int8_t ExtSeqCheck(CommProtoExt *p, uint8_t seq)
{
    uint8_t gap;

    if (seq == p->rx_last_seq)
        return -1; /* 重帧，丢弃 */

    /* 正向跳号（gap 1..127）= 真丢帧，累计；
     * 反向跳变（gap >= 128，对端重启 / 长时间中断后 seq 回绕）按重同步处理，
     * 不把 (256-gap) 巨量帧数记成丢帧（否则计数器被一次重启刷爆） */
    gap = (uint8_t)(seq - (uint8_t)(p->rx_last_seq + 1u));
    if (gap != 0u)
    {
        if (gap < 128u)
            p->lost_frames += gap;
        else
            p->rx_resync++;
    }
    p->rx_last_seq = seq;
    return 0;
}

/* 打包：帧头 + seq(自增) + 汉明编码区 + CRC8 + 帧尾
 * @note 编码直接写进 out_buff 的编码区（无中间拷贝）；汉明编码会把缓冲清零，
 *       故尾部非整字节残留位不会污染 CRC */
static int8_t ExtPack(CommProto *self, const uint8_t *payload, uint8_t *out_buff)
{
    CommProtoExt *p = (CommProtoExt *)self;
    uint32_t enc_bits;
    uint32_t code_bits = 0;
    uint16_t E;

    if (self == NULL || payload == NULL || out_buff == NULL)
        return -1;

    E = p->enc_bytes;
    enc_bits = (uint32_t)E * 8u;

    out_buff[0] = PROTO_EXT_FRAME_HEADER;
    out_buff[1] = p->tx_seq++; /* 发送 seq 自增（255→0 自然回卷） */

    /* 先过汉明码：payload(N B) → 编码区(E B) */
    if (LIB_Hamming_ExtEncode(payload, (uint32_t)self->payload_size * 8u, &p->cfg, &out_buff[2],
                              enc_bits, &code_bits) != 0)
        return -1;
    if (code_bits != enc_bits)
        return -1; /* 编译期算出的 E 与 lib 的分块结果不符（不该发生） */

    /* 再算 CRC8：范围 = seq + 编码区 */
    out_buff[2 + E] = PROTO_EXT_CRC8(&out_buff[1], (uint32_t)E + 1u);
    out_buff[3 + E] = PROTO_EXT_FRAME_TAIL;
    return 0;
}

/* 只解包：校验/纠错后返回解出的 payload 指针（NULL = 丢弃）；on_frame 由 comm 层统一调
 * @note 返回的是本实例的 rx_buff（非接收缓冲），on_frame 同步消费即可，无需立即拷走 */
static const uint8_t *ExtUnpack(CommProto *self, const uint8_t *data)
{
    CommProtoExt *p = (CommProtoExt *)self;
    LIB_Hamming_Stat_t stat;
    uint32_t enc_bits;
    uint32_t pay_bits;
    uint32_t code_bits = 0;
    uint16_t E;
    uint8_t crc_rx;

    if (self == NULL || data == NULL)
        return NULL;

    E = p->enc_bytes;
    enc_bits = (uint32_t)E * 8u;
    pay_bits = (uint32_t)self->payload_size * 8u;

    /* 1. 帧头/帧尾定界（固定长度帧，整帧长 = E + 4） */
    if (data[0] != PROTO_EXT_FRAME_HEADER)
    {
        p->rx_err++;
        return NULL;
    }
    if (data[E + 3] != PROTO_EXT_FRAME_TAIL)
    {
        p->rx_err++;
        return NULL;
    }
    crc_rx = data[E + 2];

    /* 2. 汉明解码（CRC 相符时链路无错，不会触发纠错；不符时按每块 SECDED 纠错） */
    if (LIB_Hamming_ExtDecode(&data[2], enc_bits, pay_bits, &p->cfg, p->rx_buff, &stat) != 0)
    {
        p->rx_err++;
        return NULL;
    }
    if (stat.detected != 0u)
    {
        p->rx_err++; /* 有块检出不可纠（≥2 位错），整帧丢弃 */
        return NULL;
    }

    /* 3. 再 CRC 校验 */
    if (PROTO_EXT_CRC8(&data[1], (uint32_t)E + 1u) != crc_rx)
    {
        /* 不符 ⇒ 采信汉明纠错前先复核：把纠错后的 payload 重编码，能重现原 CRC 才算纠对。
         * 这一步同时挡掉 SECDED 的 ≥3 位错误纠，以及错在 seq / CRC 字节自身的情况。 */
        p->reenc_buff[0] = data[1]; /* seq 原样带入，保持 CRC 范围一致 */
        if (LIB_Hamming_ExtEncode(p->rx_buff, pay_bits, &p->cfg, &p->reenc_buff[1], enc_bits,
                                  &code_bits) != 0 ||
            code_bits != enc_bits)
        {
            p->rx_err++;
            return NULL;
        }
        if (PROTO_EXT_CRC8(p->reenc_buff, (uint32_t)E + 1u) != crc_rx)
        {
            p->rx_err++;
            return NULL;
        }
        p->rx_fixed++;
    }

    /* 4. 整帧确认合法后才做帧序列检测（坏帧不更新 seq） */
    if (ExtSeqCheck(p, data[1]) != 0)
        return NULL; /* 重帧，丢弃 */

    return p->rx_buff;
}

/* 重置解包/打包状态（通讯重连/协议切换时调用） */
static void ExtReset(CommProto *self)
{
    CommProtoExt *p = (CommProtoExt *)self;

    if (self == NULL)
        return;
    p->tx_seq = 0;
    p->rx_last_seq = 0xFF; /* 0xFF+1 回卷=0：重置后首帧 seq=0 视为正常新帧而非重帧 */
    p->lost_frames = 0;
    p->rx_err = 0;
    p->rx_fixed = 0;
    p->rx_resync = 0;
}

int8_t CommProtoExtInit(CommProtoExt *proto)
{
    uint32_t E;

    if (proto == NULL)
        return -1;
    if (proto->base.payload_size == 0u || proto->rx_buff == NULL || proto->reenc_buff == NULL)
        return -1;

    /* config 阶段算一次分块参数（全部由 payload_size 派生，见 comm_proto_ext.h）：
     * 运行期的 pack/unpack 只用已算好的 cfg / enc_bytes，不再做任何分块推导 */
    E = PROTO_EXT_BYTES(proto->base.payload_size);
    if (E == 0u || E > 0xFFFFu)
        return -1; /* 编码区长度越界（超出 DEF 宏定的缓冲尺寸，防运行期越界读写） */

    proto->cfg.m = (uint8_t)PROTO_EXT_M;
    proto->cfg.k_s = (uint16_t)(PROTO_EXT_BLK_BYTES(proto->base.payload_size) * 8u);
    /* 每块数据位数须落在合法缩短码范围 1..2^m-1-m（不合法则编解码会拒绝，提前挡掉） */
    if (proto->cfg.k_s < 1u ||
        proto->cfg.k_s > (uint16_t)((1u << proto->cfg.m) - 1u - proto->cfg.m))
        return -1;
    proto->enc_bytes = (uint16_t)E;

    proto->base.vtable = &s_ext_vtable;
    proto->tx_seq = 0;
    proto->rx_last_seq = 0xFF; /* 0xFF+1 回卷=0：首帧 seq=0 视为正常新帧而非重帧 */
    proto->lost_frames = 0;
    proto->rx_err = 0;
    proto->rx_fixed = 0;
    proto->rx_resync = 0;
    return 0;
}

#endif /* DRV_COMM_USED */

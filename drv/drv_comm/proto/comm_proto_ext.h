/**
 * @file comm_proto_ext.h
 * @brief 通信框架-协议层（Protocol）扩展缩短汉明码 + CRC8 帧协议后端（PROTO_EXT）
 *
 * 在 comm_proto_custom.h 的固定长度帧协议基础上加一层纠错：**先过汉明码、再算 CRC**。
 * 帧格式（N = payload_size，E = 编码区字节数，均编译期确定）：
 *   [帧头 0xA5] [seq 1B] [编码区 E B] [CRC8 1B] [帧尾 0x5A]
 *   整帧长 = E + 4，即 payload_size + PROTO_EXT_OVERHEAD(payload_size)。
 *
 * 发送（pack）：payload(N B) --汉明扩展码--> 编码区(E B) --CRC8(seq+编码区)--> 帧尾
 * 接收（unpack）：帧头/帧尾 → CRC8 校验 → 汉明解码
 *   - CRC 相符：无错，直接取汉明解出的 payload（快路径，不触发纠错）
 *   - CRC 不符：走汉明纠错，再用**纠错后的 payload 重编码**并按原 CRC 复核，
 *     复核通过才采信（CRC 在这里的作用是"确认汉明纠对了"，同时兜住 SECDED 的
 *     ≥3 位错的误纠）—— 复核不通过或任一汉明块报不可纠，则整帧丢弃。
 *
 * 汉明参数全部在编译期由 payload 长度定死（见下"编译期分块"），不占用运行期计算：
 *   - m = PROTO_EXT_M = 7（校验位 7 + 总校验位 1 = 8 位 = 1 字节，编码区才能整字节 CRC）
 *   - 满块数据 15 B（k = 2^7-1-7 = 120 bit = 15 B），块数 B = ceil(N/15) 取最少，
 *     每块数据 s = ceil(N/B) B —— 保证码字总长最小（例：N=13 → 1 块 ×14 B；
 *     N=16 → 2 块 ×9 B；N=48 → 4 块 ×13 B）
 *
 * @note 纠错能力：每块"纠 1 位、检 2 位"（SECDED）；整帧可纠错位数 ≈ 块数 B，
 *       且要求错误分散在不同块。跨块的突发多位错不保证可纠。
 * @note 依赖 LIB_HAMMING_USED（lib_hamming）+ LIB_CRC_USED（lib_crc），未启用则编译报错。
 * @note 复用 custom 的帧头/帧尾与 seq 检测语义（丢帧/重帧/重同步）。
 * @note COMM_DEF 通过 token 拼接 COMM_##proto_type_##_DEF 分发到本宏。
 */

#ifndef COMM_PROTO_EXT_H
#define COMM_PROTO_EXT_H

#include "comm_proto.h"

#ifdef DRV_COMM_USED

#include "lib_hamming.h"
#include "lib_hamming_ext.h"

/* 帧头/帧尾定界（与 PROTO_CUSTOM 同值；CRC8 校验范围 = seq + 编码区） */
#define PROTO_EXT_FRAME_HEADER 0xA5
#define PROTO_EXT_FRAME_TAIL 0x5A

/* 校验位数：须使 (m+1) 为 8 的倍数（每块的 m 位校验 + 1 位总校验 = 整数字节），
 * 否则编码区会跨块余位、无法按字节做 CRC。m=7 → 每块校验 1 B。 */
#ifndef PROTO_EXT_M
#define PROTO_EXT_M 7
#endif

/*---- 编译期分块（宏，见 lib_hamming.h / lib_hamming_ext.h 的编译期参数节）----
 * 全部展开为整数常量表达式，可直接做静态数组维度、_Static_assert 条件与帧长。 */

/* 块数 B = ceil(N / 15) */
#define PROTO_EXT_BLKS(payload_bytes) LIB_HAMMING_BYTE_BLKS((payload_bytes), PROTO_EXT_M)

/* 每块数据字节数 s = ceil(N / B)（= cfg.k_s / 8） */
#define PROTO_EXT_BLK_BYTES(payload_bytes) LIB_HAMMING_BYTE_BLK_DATA((payload_bytes), PROTO_EXT_M)

/* 编码区字节数 E = B * (s + 1) */
#define PROTO_EXT_BYTES(payload_bytes) LIB_HAMMING_EXT_BYTES((payload_bytes), PROTO_EXT_M)

/* 协议开销（字节）= 帧头(1) + seq(1) + CRC8(1) + 帧尾(1) + 编码区膨胀(E - N) */
#define PROTO_EXT_OVERHEAD(payload_size) \
    (4u + PROTO_EXT_BYTES(payload_size) - (uint32_t)(payload_size))

/* 扩展汉明码帧协议派生结构体（首成员必须为 CommProto 基类） */
typedef struct
{
    CommProto base;        /* 基类（首成员） */
    LIB_Hamming_Cfg_t cfg; /* 汉明参数（Init 阶段按 payload_size 算一次：m、k_s = 每块数据位数） */
    uint16_t enc_bytes;    /* 编码区字节数 E（Init 阶段算一次） */
    uint8_t tx_seq;        /* 发送帧序列（pack 时自增写入帧） */
    uint8_t rx_last_seq;   /* 上次收到的帧序列（unpack 检测丢帧/重帧） */
    uint32_t lost_frames;  /* 累计丢帧数（正向跳号时 += 跳过帧数；重帧/坏帧不计） */
    uint32_t rx_err;       /* 坏帧计数（帧头/帧尾/CRC 复核不过/汉明不可纠；只增不清） */
    uint32_t rx_fixed;     /* 经汉明纠错 + CRC 复核救回的帧数（只增不清，调试用） */
    uint32_t rx_resync;    /* 对端 seq 反向跳变（重启/长中断）重同步次数；只增不清 */
    uint8_t *rx_buff;      /* 解码输出缓冲（payload 字节数；DEF 宏静态提供，作 unpack 返回值） */
    uint8_t *reenc_buff;   /* 重编码复核缓冲（[0]=seq，[1..E]=重编码码字；DEF 宏静态提供） */
} CommProtoExt;

/**
 * @brief 静态定义扩展汉明码帧协议实例
 * @param name        实例名称
 * @param media_      media 实例（发送用，指向 CommMedia 派生实例）
 * @param payload_sz  payload 长度（编译期确定；整帧长 = payload_sz + PROTO_EXT_OVERHEAD(payload_sz)）
 *
 * @note 分工（对齐 lib_crc 的 LIB_CRC_CUSTOM_DEF + LIB_CRC_GenTable）：本宏**只定缓冲大小**
 *       —— 静态数组维度只能是编译期常量，故分块相关的 _Static_assert 在此做；而 cfg（m/k_s）
 *       与 enc_bytes 这类"由 payload_size 派生"的值不写进初始化器，统一由
 *       CommProtoExtInit 在 config 阶段算一次，运行期收发只按已算好的值逐块编解码。
 * @note media_ 以指针绑定，运行时无需另传；发送缓冲由 comm 层提供（inst->tx_buff，
 *       大小由 COMM_DEF 按开销推算）。vtable 与收发序列状态由 CommProtoExtInit 初始化。
 * @note 另静态定义两个 scratch：name##_rx_buff[payload_sz]（解码输出）、
 *       name##_reenc_buff[E+1]（CRC 复核用的重编码缓冲）。
 *
 * @example
 *   COMM_PROTO_EXT_DEF(proto_cmd, uart_comm, 13);
 */
#define COMM_PROTO_EXT_DEF(name, media_, payload_sz)                                            \
    _Static_assert((payload_sz) >= 1u && (uint32_t)(payload_sz) <= 0xFFFFu,                     \
                   "PROTO_EXT: payload 长度须在 1..65535 字节");                                 \
    LIB_HAMMING_EXT_CHECK_M(PROTO_EXT_M);                                                       \
    LIB_HAMMING_CHECK_CFG(PROTO_EXT_M, PROTO_EXT_BLK_BYTES(payload_sz) * 8u);                   \
    static uint8_t name##_rx_buff[(payload_sz)];                                                \
    static uint8_t name##_reenc_buff[(PROTO_EXT_BYTES(payload_sz)) + 1u];                       \
    static CommProtoExt name = {                                                                \
        .base.payload_size = (payload_sz),                                                      \
        .base.media = (void *)&media_,                                                          \
        .rx_buff = name##_rx_buff,                                                              \
        .reenc_buff = name##_reenc_buff} /* 尾部无分号，调用处加 */

/**
 * @brief 初始化扩展汉明码帧协议后端（挂 vtable + 算分块参数 + 清零收发序列状态）
 * @param proto CommProtoExt 实例指针（COMM_PROTO_EXT_DEF 定义）
 * @retval 0 成功；-1 参数非法（含分块参数越界）
 *
 * @note config 阶段一次性算好由 payload_size 派生的量（cfg.m / cfg.k_s / enc_bytes），
 *       运行期收发不再做任何分块推导；算出的结果须与编译期 DEF 宏定下的缓冲尺寸自洽，
 *       不自洽（越界）直接返回 -1，避免运行期越界读写。
 */
int8_t CommProtoExtInit(CommProtoExt *proto);

#endif /* DRV_COMM_USED */
#endif /* COMM_PROTO_EXT_H */

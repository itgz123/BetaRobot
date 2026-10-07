/**
 * @file referee2026_proto.c
 * @brief 对接层定义处：串口外设配置表 + 本模块自有的 CRC 表/算法/查表描述符
 *
 * 本文件只放**编译期常量**，没有函数，没有运行期状态：
 *   ① `referee2026_uart_init[]` —— 各链路的串口外设参数（协议 §1.1 表 1-1）
 *   ② 三张 CRC 表 —— 本模块自备，**不依赖 lib_crc 的 Flash 表开关**
 *   ③ `referee2026_crc8_algo` / `referee2026_crc16_algo` —— 两个帧校验的算法六元组
 *   ④ 三个查表描述符（算法 + 表）
 *
 * CRC 表为什么在模块内自备（而不是引 lib 的 LIB_CRC_TABLE_*）：
 *   lib 的表受 `LIB_CRC_TABLES_USED` 管，而该开关是给"要不要带 lib 那 6 张通用表"用的；
 *   裁判系统的帧校验是本模块的硬需求，不该因为别处省 Flash 而被连带关掉。
 *   故本模块的表与 lib 的表**解耦**：`LIB_CRC_TABLES_USED` 只管 lib，本模块的表照常定义。
 *   本模块对 lib 的依赖只剩**函数**（`LIB_CRC_TableCalc` / `LIB_CRC_Direct`），
 *   即 `LIB_CRC_USED` —— 那是"要不要 CRC 功能"，与"带哪几张表"是两回事。
 *   代价是这三张表（768 项 uint32 = 3KB Flash）与 lib 的同名表可能各存一份；
 *   以 3KB 换掉一处跨模块的隐式耦合，值。
 *
 * 变量一律 `const`，落 .rodata：零 RAM、零初始化代码。
 *
 * 生成区（文件末尾）由 tools/referee2026_crc_tables_gen.py 生成，**勿手改**；
 * 该脚本自校验（标准 check 向量 + 与 lib 表逐项比对）后才写回。
 *
 * 生效条件：`DRV_REFEREE2026_USED`（app_cfg.h 里开）。未开时本文件编译为空。
 */

#include "referee2026_proto.h"
#include "app_cfg.h"

#ifdef DRV_REFEREE2026_USED

/*============================================
 *      一、串口外设配置（协议 §1.1 表 1-1）
 *============================================*/

#ifdef HAL_UART_MODULE_ENABLED

/**
 * 各链路一条。三条链路的公共参数一致，差异只在波特率（雷达链路不走串口，全 0 占位）。
 *
 * **必须用指定初始化器**：本表在 F4 与 H7 上共用，H7 的 UART_InitTypeDef 多出
 * `OneBitSampling` / `ClockPrescaler` 两个字段，按位置填会被静默错配；指定初始化器下
 * 这两项被隐式清零，恰好就是 HAL 的期望默认（DISABLE / DIV1，宏值都是 0）。
 *
 * 与 CubeMX 的关系：这张表是"协议要求的硬件参数"在固件里的权威记录，各串口的 CubeMX
 * 配置就照它来（bsp_usart 只管实例、不管硬件，见 bsp_usart.h）。运行期若要改波特率，
 * 把对应行喂给 HAL_UART_Init 重初始化即可。
 */
const UART_InitTypeDef referee2026_uart_init[REFEREE2026_LINK_TYPE_COUNT] = {
    /* 常规链路：电源管理模块的 User 串口，115200 */
    [REFEREE2026_LINK_TYPE_COMMON] =
        {
            .BaudRate = 115200,
            .WordLength = UART_WORDLENGTH_8B,
            .StopBits = UART_STOPBITS_1,
            .Parity = UART_PARITY_NONE,
            .Mode = UART_MODE_TX_RX,
            .HwFlowCtl = UART_HWCONTROL_NONE,
            .OverSampling = UART_OVERSAMPLING_16,
        },
    /* 图传链路：图传发送端串口，921600 */
    [REFEREE2026_LINK_TYPE_VIDEO] =
        {
            .BaudRate = 921600,
            .WordLength = UART_WORDLENGTH_8B,
            .StopBits = UART_STOPBITS_1,
            .Parity = UART_PARITY_NONE,
            .Mode = UART_MODE_TX_RX,
            .HwFlowCtl = UART_HWCONTROL_NONE,
            .OverSampling = UART_OVERSAMPLING_16,
        },
    /* 雷达无线链路：电磁波，不占串口。全 0 占位仅为保持下标对齐，
     * 取用前先过 Referee2026LinkHasUart()，别把它当合法配置。 */
    [REFEREE2026_LINK_TYPE_RADAR] = {0},
    /* 非链路（0x0306）：自定义控制器与选手端图传模块直连串口，115200 */
    [REFEREE2026_LINK_TYPE_NONE] =
        {
            .BaudRate = 115200,
            .WordLength = UART_WORDLENGTH_8B,
            .StopBits = UART_STOPBITS_1,
            .Parity = UART_PARITY_NONE,
            .Mode = UART_MODE_TX_RX,
            .HwFlowCtl = UART_HWCONTROL_NONE,
            .OverSampling = UART_OVERSAMPLING_16,
        },
};

#endif /* HAL_UART_MODULE_ENABLED */

/*============================================
 *   二、CRC 算法描述（协议附录一）
 *============================================*/

/**
 * 帧头 CRC8。参数与官方附录一的 C 代码逐项对应：
 *   `CRC8_INIT = 0xff`、`G(x)=x8+x5+x4+1`（→ poly 0x31，反射形式 0x8C）、
 *   表驱动的 `ucCRC8 = CRC8_TAB[ucCRC8 ^ byte]` 即 RefIn/RefOut = 1，结果不再异或。
 *
 * 用的表是文件末尾的 `s_referee2026_crc8_tab`（poly 0x31、反射），与官方 `CRC8_TAB[256]`
 * 逐项一致 —— 差别只在初始值：官方表本身与 init 无关，本模块需要 init 0xFF。
 */
const Referee2026CrcAlgo_t referee2026_crc8_algo = {
    .init_value = 0xFFu,
    .poly_size = 8,
    .poly = 0x31u,
    .xor_out = 0x00u,
    .reverse_in = 1,
    .reverse_out = 1,
};

/**
 * 整包 CRC16。对应官方附录一的 `Get_CRC16_Check_Sum` + `CRC_INIT = 0xffff`：
 *   poly 0x1021（反射形式 0x8408，官方表里能看到这个值）、Init 0xFFFF、无 XorOut、
 *   `wCRC = (wCRC >> 8) ^ wCRC_Table[(wCRC ^ byte) & 0xFF]` 即反射算法。
 * 标准名 CRC-16/MCRF4XX。
 */
const Referee2026CrcAlgo_t referee2026_crc16_algo = {
    .init_value = 0xFFFFu,
    .poly_size = 16,
    .poly = 0x1021u,
    .xor_out = 0x0000u,
    .reverse_in = 1,
    .reverse_out = 1,
};

/**
 * 0xA9 图传遥控帧的 CRC16（自记出处：VT03·VT13 图传说明书）。
 * 与上面那条**同 poly 但方向相反**：非反射（RefIn/RefOut = 0），Init 0xFFFF。
 * 标准名 CRC-16/CCITT-FALSE。
 *
 * @warning **本条的 CRC 方向存疑，且手头没有权威出处可判**：
 *          - 本模块按**非反射**（CCITT-FALSE）建模，出处是上面那句"VT03·VT13 图传说明书"
 *            —— 那份文档**不在本仓库**，无法核对；
 *          - 参考工程 `rm_referee` 的 `imageRoad.c` 校验 `0xA9 0x53` 帧用的是
 *            `Verify_CRC16_Check_Sum`，即**反射**表（与 0xA5 帧同一张），方向与本模块相反；
 *          - 官方 2026 协议与串口协议附录里**没有 0xA9 帧**（附录里的图传遥控走 0xA5 的
 *            0x0304），故官方口径也无法裁决。
 *          二者必有一错。本模块**不解析 0xA9**，这张表目前零运行期消费者；将来要实现 0xA9
 *          解析前，必须先拿到那份图传说明书把方向钉死（详见 tools/referee2026_crc_tables_gen.py
 *          文件头的警告）。
 * @note 方向不同 ⇒ 表也不同（0x1021 的反射表与非反射表是两套数），故本模块备两张 16 位表。
 *       绝不能把两条 16 位算法的表对调 —— 查表方向由 algo.reverse_in 决定，
 *       挂错表不会报错，只会**静默算出错值**。
 */
const Referee2026CrcAlgo_t referee2026_crc16_remote_algo = {
    .init_value = 0xFFFFu,
    .poly_size = 16,
    .poly = 0x1021u,
    .xor_out = 0x0000u,
    .reverse_in = 0,
    .reverse_out = 0,
};

/*============================================
 *  三、CRC 查表数据（生成区，勿手改）
 *============================================*/

/* 以下为生成器排版的定宽内容（每行 8 个数），重排会与下次生成冲突，故关闭格式化。
 * 重新生成：python tools/referee2026_crc_tables_gen.py */
// clang-format off

/* ==== 生成区开始（referee2026_crc_tables_gen.py） ==== */

/* 查表 (poly=0x00000031, width=8, refin=1) */
static const uint32_t s_referee2026_crc8_tab[256] = {
    0x00000000u, 0x0000005Eu, 0x000000BCu, 0x000000E2u, 0x00000061u, 0x0000003Fu, 0x000000DDu, 0x00000083u,
    0x000000C2u, 0x0000009Cu, 0x0000007Eu, 0x00000020u, 0x000000A3u, 0x000000FDu, 0x0000001Fu, 0x00000041u,
    0x0000009Du, 0x000000C3u, 0x00000021u, 0x0000007Fu, 0x000000FCu, 0x000000A2u, 0x00000040u, 0x0000001Eu,
    0x0000005Fu, 0x00000001u, 0x000000E3u, 0x000000BDu, 0x0000003Eu, 0x00000060u, 0x00000082u, 0x000000DCu,
    0x00000023u, 0x0000007Du, 0x0000009Fu, 0x000000C1u, 0x00000042u, 0x0000001Cu, 0x000000FEu, 0x000000A0u,
    0x000000E1u, 0x000000BFu, 0x0000005Du, 0x00000003u, 0x00000080u, 0x000000DEu, 0x0000003Cu, 0x00000062u,
    0x000000BEu, 0x000000E0u, 0x00000002u, 0x0000005Cu, 0x000000DFu, 0x00000081u, 0x00000063u, 0x0000003Du,
    0x0000007Cu, 0x00000022u, 0x000000C0u, 0x0000009Eu, 0x0000001Du, 0x00000043u, 0x000000A1u, 0x000000FFu,
    0x00000046u, 0x00000018u, 0x000000FAu, 0x000000A4u, 0x00000027u, 0x00000079u, 0x0000009Bu, 0x000000C5u,
    0x00000084u, 0x000000DAu, 0x00000038u, 0x00000066u, 0x000000E5u, 0x000000BBu, 0x00000059u, 0x00000007u,
    0x000000DBu, 0x00000085u, 0x00000067u, 0x00000039u, 0x000000BAu, 0x000000E4u, 0x00000006u, 0x00000058u,
    0x00000019u, 0x00000047u, 0x000000A5u, 0x000000FBu, 0x00000078u, 0x00000026u, 0x000000C4u, 0x0000009Au,
    0x00000065u, 0x0000003Bu, 0x000000D9u, 0x00000087u, 0x00000004u, 0x0000005Au, 0x000000B8u, 0x000000E6u,
    0x000000A7u, 0x000000F9u, 0x0000001Bu, 0x00000045u, 0x000000C6u, 0x00000098u, 0x0000007Au, 0x00000024u,
    0x000000F8u, 0x000000A6u, 0x00000044u, 0x0000001Au, 0x00000099u, 0x000000C7u, 0x00000025u, 0x0000007Bu,
    0x0000003Au, 0x00000064u, 0x00000086u, 0x000000D8u, 0x0000005Bu, 0x00000005u, 0x000000E7u, 0x000000B9u,
    0x0000008Cu, 0x000000D2u, 0x00000030u, 0x0000006Eu, 0x000000EDu, 0x000000B3u, 0x00000051u, 0x0000000Fu,
    0x0000004Eu, 0x00000010u, 0x000000F2u, 0x000000ACu, 0x0000002Fu, 0x00000071u, 0x00000093u, 0x000000CDu,
    0x00000011u, 0x0000004Fu, 0x000000ADu, 0x000000F3u, 0x00000070u, 0x0000002Eu, 0x000000CCu, 0x00000092u,
    0x000000D3u, 0x0000008Du, 0x0000006Fu, 0x00000031u, 0x000000B2u, 0x000000ECu, 0x0000000Eu, 0x00000050u,
    0x000000AFu, 0x000000F1u, 0x00000013u, 0x0000004Du, 0x000000CEu, 0x00000090u, 0x00000072u, 0x0000002Cu,
    0x0000006Du, 0x00000033u, 0x000000D1u, 0x0000008Fu, 0x0000000Cu, 0x00000052u, 0x000000B0u, 0x000000EEu,
    0x00000032u, 0x0000006Cu, 0x0000008Eu, 0x000000D0u, 0x00000053u, 0x0000000Du, 0x000000EFu, 0x000000B1u,
    0x000000F0u, 0x000000AEu, 0x0000004Cu, 0x00000012u, 0x00000091u, 0x000000CFu, 0x0000002Du, 0x00000073u,
    0x000000CAu, 0x00000094u, 0x00000076u, 0x00000028u, 0x000000ABu, 0x000000F5u, 0x00000017u, 0x00000049u,
    0x00000008u, 0x00000056u, 0x000000B4u, 0x000000EAu, 0x00000069u, 0x00000037u, 0x000000D5u, 0x0000008Bu,
    0x00000057u, 0x00000009u, 0x000000EBu, 0x000000B5u, 0x00000036u, 0x00000068u, 0x0000008Au, 0x000000D4u,
    0x00000095u, 0x000000CBu, 0x00000029u, 0x00000077u, 0x000000F4u, 0x000000AAu, 0x00000048u, 0x00000016u,
    0x000000E9u, 0x000000B7u, 0x00000055u, 0x0000000Bu, 0x00000088u, 0x000000D6u, 0x00000034u, 0x0000006Au,
    0x0000002Bu, 0x00000075u, 0x00000097u, 0x000000C9u, 0x0000004Au, 0x00000014u, 0x000000F6u, 0x000000A8u,
    0x00000074u, 0x0000002Au, 0x000000C8u, 0x00000096u, 0x00000015u, 0x0000004Bu, 0x000000A9u, 0x000000F7u,
    0x000000B6u, 0x000000E8u, 0x0000000Au, 0x00000054u, 0x000000D7u, 0x00000089u, 0x0000006Bu, 0x00000035u,
};

/* 查表 (poly=0x00001021, width=16, refin=1) */
static const uint32_t s_referee2026_crc16_tab[256] = {
    0x00000000u, 0x00001189u, 0x00002312u, 0x0000329Bu, 0x00004624u, 0x000057ADu, 0x00006536u, 0x000074BFu,
    0x00008C48u, 0x00009DC1u, 0x0000AF5Au, 0x0000BED3u, 0x0000CA6Cu, 0x0000DBE5u, 0x0000E97Eu, 0x0000F8F7u,
    0x00001081u, 0x00000108u, 0x00003393u, 0x0000221Au, 0x000056A5u, 0x0000472Cu, 0x000075B7u, 0x0000643Eu,
    0x00009CC9u, 0x00008D40u, 0x0000BFDBu, 0x0000AE52u, 0x0000DAEDu, 0x0000CB64u, 0x0000F9FFu, 0x0000E876u,
    0x00002102u, 0x0000308Bu, 0x00000210u, 0x00001399u, 0x00006726u, 0x000076AFu, 0x00004434u, 0x000055BDu,
    0x0000AD4Au, 0x0000BCC3u, 0x00008E58u, 0x00009FD1u, 0x0000EB6Eu, 0x0000FAE7u, 0x0000C87Cu, 0x0000D9F5u,
    0x00003183u, 0x0000200Au, 0x00001291u, 0x00000318u, 0x000077A7u, 0x0000662Eu, 0x000054B5u, 0x0000453Cu,
    0x0000BDCBu, 0x0000AC42u, 0x00009ED9u, 0x00008F50u, 0x0000FBEFu, 0x0000EA66u, 0x0000D8FDu, 0x0000C974u,
    0x00004204u, 0x0000538Du, 0x00006116u, 0x0000709Fu, 0x00000420u, 0x000015A9u, 0x00002732u, 0x000036BBu,
    0x0000CE4Cu, 0x0000DFC5u, 0x0000ED5Eu, 0x0000FCD7u, 0x00008868u, 0x000099E1u, 0x0000AB7Au, 0x0000BAF3u,
    0x00005285u, 0x0000430Cu, 0x00007197u, 0x0000601Eu, 0x000014A1u, 0x00000528u, 0x000037B3u, 0x0000263Au,
    0x0000DECDu, 0x0000CF44u, 0x0000FDDFu, 0x0000EC56u, 0x000098E9u, 0x00008960u, 0x0000BBFBu, 0x0000AA72u,
    0x00006306u, 0x0000728Fu, 0x00004014u, 0x0000519Du, 0x00002522u, 0x000034ABu, 0x00000630u, 0x000017B9u,
    0x0000EF4Eu, 0x0000FEC7u, 0x0000CC5Cu, 0x0000DDD5u, 0x0000A96Au, 0x0000B8E3u, 0x00008A78u, 0x00009BF1u,
    0x00007387u, 0x0000620Eu, 0x00005095u, 0x0000411Cu, 0x000035A3u, 0x0000242Au, 0x000016B1u, 0x00000738u,
    0x0000FFCFu, 0x0000EE46u, 0x0000DCDDu, 0x0000CD54u, 0x0000B9EBu, 0x0000A862u, 0x00009AF9u, 0x00008B70u,
    0x00008408u, 0x00009581u, 0x0000A71Au, 0x0000B693u, 0x0000C22Cu, 0x0000D3A5u, 0x0000E13Eu, 0x0000F0B7u,
    0x00000840u, 0x000019C9u, 0x00002B52u, 0x00003ADBu, 0x00004E64u, 0x00005FEDu, 0x00006D76u, 0x00007CFFu,
    0x00009489u, 0x00008500u, 0x0000B79Bu, 0x0000A612u, 0x0000D2ADu, 0x0000C324u, 0x0000F1BFu, 0x0000E036u,
    0x000018C1u, 0x00000948u, 0x00003BD3u, 0x00002A5Au, 0x00005EE5u, 0x00004F6Cu, 0x00007DF7u, 0x00006C7Eu,
    0x0000A50Au, 0x0000B483u, 0x00008618u, 0x00009791u, 0x0000E32Eu, 0x0000F2A7u, 0x0000C03Cu, 0x0000D1B5u,
    0x00002942u, 0x000038CBu, 0x00000A50u, 0x00001BD9u, 0x00006F66u, 0x00007EEFu, 0x00004C74u, 0x00005DFDu,
    0x0000B58Bu, 0x0000A402u, 0x00009699u, 0x00008710u, 0x0000F3AFu, 0x0000E226u, 0x0000D0BDu, 0x0000C134u,
    0x000039C3u, 0x0000284Au, 0x00001AD1u, 0x00000B58u, 0x00007FE7u, 0x00006E6Eu, 0x00005CF5u, 0x00004D7Cu,
    0x0000C60Cu, 0x0000D785u, 0x0000E51Eu, 0x0000F497u, 0x00008028u, 0x000091A1u, 0x0000A33Au, 0x0000B2B3u,
    0x00004A44u, 0x00005BCDu, 0x00006956u, 0x000078DFu, 0x00000C60u, 0x00001DE9u, 0x00002F72u, 0x00003EFBu,
    0x0000D68Du, 0x0000C704u, 0x0000F59Fu, 0x0000E416u, 0x000090A9u, 0x00008120u, 0x0000B3BBu, 0x0000A232u,
    0x00005AC5u, 0x00004B4Cu, 0x000079D7u, 0x0000685Eu, 0x00001CE1u, 0x00000D68u, 0x00003FF3u, 0x00002E7Au,
    0x0000E70Eu, 0x0000F687u, 0x0000C41Cu, 0x0000D595u, 0x0000A12Au, 0x0000B0A3u, 0x00008238u, 0x000093B1u,
    0x00006B46u, 0x00007ACFu, 0x00004854u, 0x000059DDu, 0x00002D62u, 0x00003CEBu, 0x00000E70u, 0x00001FF9u,
    0x0000F78Fu, 0x0000E606u, 0x0000D49Du, 0x0000C514u, 0x0000B1ABu, 0x0000A022u, 0x000092B9u, 0x00008330u,
    0x00007BC7u, 0x00006A4Eu, 0x000058D5u, 0x0000495Cu, 0x00003DE3u, 0x00002C6Au, 0x00001EF1u, 0x00000F78u,
};

/* 查表 (poly=0x00001021, width=16, refin=0) */
static const uint32_t s_referee2026_crc16_remote_tab[256] = {
    0x00000000u, 0x00001021u, 0x00002042u, 0x00003063u, 0x00004084u, 0x000050A5u, 0x000060C6u, 0x000070E7u,
    0x00008108u, 0x00009129u, 0x0000A14Au, 0x0000B16Bu, 0x0000C18Cu, 0x0000D1ADu, 0x0000E1CEu, 0x0000F1EFu,
    0x00001231u, 0x00000210u, 0x00003273u, 0x00002252u, 0x000052B5u, 0x00004294u, 0x000072F7u, 0x000062D6u,
    0x00009339u, 0x00008318u, 0x0000B37Bu, 0x0000A35Au, 0x0000D3BDu, 0x0000C39Cu, 0x0000F3FFu, 0x0000E3DEu,
    0x00002462u, 0x00003443u, 0x00000420u, 0x00001401u, 0x000064E6u, 0x000074C7u, 0x000044A4u, 0x00005485u,
    0x0000A56Au, 0x0000B54Bu, 0x00008528u, 0x00009509u, 0x0000E5EEu, 0x0000F5CFu, 0x0000C5ACu, 0x0000D58Du,
    0x00003653u, 0x00002672u, 0x00001611u, 0x00000630u, 0x000076D7u, 0x000066F6u, 0x00005695u, 0x000046B4u,
    0x0000B75Bu, 0x0000A77Au, 0x00009719u, 0x00008738u, 0x0000F7DFu, 0x0000E7FEu, 0x0000D79Du, 0x0000C7BCu,
    0x000048C4u, 0x000058E5u, 0x00006886u, 0x000078A7u, 0x00000840u, 0x00001861u, 0x00002802u, 0x00003823u,
    0x0000C9CCu, 0x0000D9EDu, 0x0000E98Eu, 0x0000F9AFu, 0x00008948u, 0x00009969u, 0x0000A90Au, 0x0000B92Bu,
    0x00005AF5u, 0x00004AD4u, 0x00007AB7u, 0x00006A96u, 0x00001A71u, 0x00000A50u, 0x00003A33u, 0x00002A12u,
    0x0000DBFDu, 0x0000CBDCu, 0x0000FBBFu, 0x0000EB9Eu, 0x00009B79u, 0x00008B58u, 0x0000BB3Bu, 0x0000AB1Au,
    0x00006CA6u, 0x00007C87u, 0x00004CE4u, 0x00005CC5u, 0x00002C22u, 0x00003C03u, 0x00000C60u, 0x00001C41u,
    0x0000EDAEu, 0x0000FD8Fu, 0x0000CDECu, 0x0000DDCDu, 0x0000AD2Au, 0x0000BD0Bu, 0x00008D68u, 0x00009D49u,
    0x00007E97u, 0x00006EB6u, 0x00005ED5u, 0x00004EF4u, 0x00003E13u, 0x00002E32u, 0x00001E51u, 0x00000E70u,
    0x0000FF9Fu, 0x0000EFBEu, 0x0000DFDDu, 0x0000CFFCu, 0x0000BF1Bu, 0x0000AF3Au, 0x00009F59u, 0x00008F78u,
    0x00009188u, 0x000081A9u, 0x0000B1CAu, 0x0000A1EBu, 0x0000D10Cu, 0x0000C12Du, 0x0000F14Eu, 0x0000E16Fu,
    0x00001080u, 0x000000A1u, 0x000030C2u, 0x000020E3u, 0x00005004u, 0x00004025u, 0x00007046u, 0x00006067u,
    0x000083B9u, 0x00009398u, 0x0000A3FBu, 0x0000B3DAu, 0x0000C33Du, 0x0000D31Cu, 0x0000E37Fu, 0x0000F35Eu,
    0x000002B1u, 0x00001290u, 0x000022F3u, 0x000032D2u, 0x00004235u, 0x00005214u, 0x00006277u, 0x00007256u,
    0x0000B5EAu, 0x0000A5CBu, 0x000095A8u, 0x00008589u, 0x0000F56Eu, 0x0000E54Fu, 0x0000D52Cu, 0x0000C50Du,
    0x000034E2u, 0x000024C3u, 0x000014A0u, 0x00000481u, 0x00007466u, 0x00006447u, 0x00005424u, 0x00004405u,
    0x0000A7DBu, 0x0000B7FAu, 0x00008799u, 0x000097B8u, 0x0000E75Fu, 0x0000F77Eu, 0x0000C71Du, 0x0000D73Cu,
    0x000026D3u, 0x000036F2u, 0x00000691u, 0x000016B0u, 0x00006657u, 0x00007676u, 0x00004615u, 0x00005634u,
    0x0000D94Cu, 0x0000C96Du, 0x0000F90Eu, 0x0000E92Fu, 0x000099C8u, 0x000089E9u, 0x0000B98Au, 0x0000A9ABu,
    0x00005844u, 0x00004865u, 0x00007806u, 0x00006827u, 0x000018C0u, 0x000008E1u, 0x00003882u, 0x000028A3u,
    0x0000CB7Du, 0x0000DB5Cu, 0x0000EB3Fu, 0x0000FB1Eu, 0x00008BF9u, 0x00009BD8u, 0x0000ABBBu, 0x0000BB9Au,
    0x00004A75u, 0x00005A54u, 0x00006A37u, 0x00007A16u, 0x00000AF1u, 0x00001AD0u, 0x00002AB3u, 0x00003A92u,
    0x0000FD2Eu, 0x0000ED0Fu, 0x0000DD6Cu, 0x0000CD4Du, 0x0000BDAAu, 0x0000AD8Bu, 0x00009DE8u, 0x00008DC9u,
    0x00007C26u, 0x00006C07u, 0x00005C64u, 0x00004C45u, 0x00003CA2u, 0x00002C83u, 0x00001CE0u, 0x00000CC1u,
    0x0000EF1Fu, 0x0000FF3Eu, 0x0000CF5Du, 0x0000DF7Cu, 0x0000AF9Bu, 0x0000BFBAu, 0x00008FD9u, 0x00009FF8u,
    0x00006E17u, 0x00007E36u, 0x00004E55u, 0x00005E74u, 0x00002E93u, 0x00003EB2u, 0x00000ED1u, 0x00001EF0u,
};

/* ==== 生成区结束 ==== */
// clang-format on

/*============================================
 *  四、查表描述符（算法 + 表）
 *============================================*/

/** 帧头 CRC8：覆盖帧头前 4B（SOF、data_length 小端、seq），结果写回偏移 4。 */
const Referee2026CrcTable_t referee2026_crc8_table = {
    .algo = &referee2026_crc8_algo,
    .table = s_referee2026_crc8_tab,
};

/** 整包 CRC16：覆盖「帧头 5B + cmd_id 2B + 数据段」，结果小端写帧尾 2B。 */
const Referee2026CrcTable_t referee2026_crc16_table = {
    .algo = &referee2026_crc16_algo,
    .table = s_referee2026_crc16_tab,
};

/** 0xA9 图传遥控帧 CRC16（非反射，21B 短帧）。⚠ 方向存疑，见上方 algo 的 @warning。 */
const Referee2026CrcTable_t referee2026_crc16_remote_table = {
    .algo = &referee2026_crc16_remote_algo,
    .table = s_referee2026_crc16_remote_tab,
};

#endif /* DRV_REFEREE2026_USED */

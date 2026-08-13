/**
 * @file bsp_crc.h
 * @brief BSP层CRC计算封装（直接计算 / Flash 表 / 自定义表 / 通用查表）
 *
 * 设计（分层降级，任一层都可用）：
 *   1. BSP_CRC_Direct    逐位直接计算，可算任意算法（Poly/Init/XorOut/RefIn/RefOut
 *                        五个自由度全覆盖），支持 CRC-7/MMC、CRC-8、CRC-16、CRC-32
 *                        等全部常用标准算法 —— 兜底层，无需任何表。
 *   2. BSP_CRC_TableCalc + Flash 内置表（bsp_crc_tables.h）：常用算法预生成 const 表
 *                        （零 RAM、零运行时建表），查表计算每字节 1 次查表替代 8 次位循环。
 *   3. BSP_CRC_CUSTOM_DEF 宏定义自定义算法表（定义时只定大小 256 项），运行时调用
 *                        BSP_CRC_GenTable（先校验表大小与算法是否匹配，匹配才生成），
 *                        再用 BSP_CRC_TableCalc 计算。
 *
 * 表统一为 256 项 uint32_t（1 字节输入 → 查表项），Flash const 表与 RAM 生成表格式一致，
 * BSP_CRC_TableCalc 两者通用。
 */

#ifndef __BSP_CRC_H
#define __BSP_CRC_H

#include <stdint.h>

/*============================================
 *                 类型定义
 *============================================*/

/**
 * @brief CRC算法描述（完全参数化，5 个自由度对齐常见 CRC 计算器/crcmod）
 *
 * 参数说明：
 *   - poly_size    多项式宽度：7 / 8 / 16 / 32
 *   - poly         生成多项式（Poly，非反射形式，如 0x07 / 0x31 / 0x9B / 0x1021 / 0x04C11DB7）
 *   - init_value   寄存器初始值（Init，常见 0x00 / 0xFF / 0xFFFF / 0xFFFFFFFF）
 *   - xor_out      结果异或值（XorOut，计算完与某值异或，常见 0x00 / 0xFF / 0xFFFFFFFF）
 *   - reverse_in   输入反转（RefIn）：=1 时按反射算法（LSB-first，内部取反射多项式）计算
 *   - reverse_out  输出反转（RefOut）：=1 时最终结果按位整体反转
 *
 * @example
 *   CRC-8/MAXIM（poly 0x31, init 0x00, refin 1, refout 1, xorout 0x00）
 *   CRC-16/CCITT-FALSE（poly 0x1021, init 0xFFFF, refin 0, refout 0, xorout 0x0000）
 *   CRC-32（poly 0x04C11DB7, init 0xFFFFFFFF, refin 1, refout 1, xorout 0xFFFFFFFF）
 */
typedef struct
{
    uint32_t init_value; // 初始值（Init）
    uint32_t poly_size;  // 多项式宽度: 7, 8, 16, 32
    uint32_t poly;       // 生成多项式（Poly，非反射形式）
    uint32_t xor_out;    // 结果异或值（XorOut）
    uint8_t reverse_in;  // 输入反转（RefIn）
    uint8_t reverse_out; // 输出反转（RefOut）
} BSP_CRC_Algo_t;

/**
 * @brief 查表描述：算法 + 表指针（统一 256 项 uint32_t，Flash const 或 RAM 生成均可）
 */
typedef struct
{
    const BSP_CRC_Algo_t *algo; /* 算法描述 */
    const uint32_t *table;      /* 256 项查表 */
} BSP_CRC_Table_t;

/*============================================
 *              CRC 计算接口
 *============================================*/

/**
 * @brief 逐位直接计算（可算任意算法，兜底层，无需表）
 * @param algo CRC算法描述（见 BSP_CRC_Algo_t）
 * @param data 数据指针（NULL 返回 0；len==0 时返回 init 经 RefOut/XorOut 后的值）
 * @param len 数据长度
 * @return CRC校验值（宽度由 poly_size 决定，高位置 0；非法宽度返回 0）
 * @note 支持 poly_size = 7/8/16/32；CRC-7 因宽度非 8 倍数，只能走本函数。
 */
uint32_t BSP_CRC_Direct(const BSP_CRC_Algo_t *algo, const uint8_t *data, uint32_t len);

/**
 * @brief 生成 256 项查表到 table[256]（RAM），供 BSP_CRC_TableCalc 使用
 * @param algo CRC算法描述
 * @param table 输出表缓冲（必须 256 项 uint32_t）
 * @return 0 成功；-1 失败（algo/table 为空，或 poly_size 非 8/16/32 —— 7 位/非法宽度
 *         不支持逐字节查表，须用 BSP_CRC_Direct）
 * @note 生成表与 Flash 内置表（bsp_crc_tables.h）格式一致，可混用。
 */
int8_t BSP_CRC_GenTable(const BSP_CRC_Algo_t *algo, uint32_t table[256]);

/**
 * @brief 通用查表计算（表可为 Flash const 表或 BSP_CRC_GenTable 生成的 RAM 表）
 * @param tbl 查表描述（算法 + 表指针）
 * @param data 数据指针（NULL 返回 0；len==0 时返回 init 经 RefOut/XorOut 后的值）
 * @param len 数据长度
 * @return CRC校验值（宽度由 poly_size 决定，高位置 0；tbl 无效返回 0）
 * @note poly_size == 7 时自动降级为 BSP_CRC_Direct（7 位不支持查表）。
 */
uint32_t BSP_CRC_TableCalc(const BSP_CRC_Table_t *tbl, const uint8_t *data, uint32_t len);

/*============================================
 *          自定义算法表定义宏（对齐 COMM_DEF 风格）
 *============================================*/

/* 自定义算法表定义宏（对齐 COMM_DEF 风格）：定义查表描述符 BSP_CRC_Table_t name（表名）
 * + 内部 256 项 RAM 表 name##_table（只定大小）。算法参数（Poly/Init/XorOut/RefIn/RefOut）
 * 不写在宏里，由用户定义 BSP_CRC_Algo_t 常量并绑定到 name.algo，调用 BSP_CRC_GenTable
 * 生成表时传入（生成时设置算法）。
 *
 *   BSP_CRC_CUSTOM_DEF(name) →
 *       static uint32_t name##_table[256];
 *       static BSP_CRC_Table_t name = { NULL, name##_table };
 *
 * @note name.algo 初始为 NULL；使用前必须赋值（生成表与查表计算都依赖算法）。
 *
 * @example 自定义算法流程：
 *   BSP_CRC_CUSTOM_DEF(my_crc);                 // 1. 定义描述符 my_crc + 表（只定大小）
 *   static const BSP_CRC_Algo_t my_crc_algo =   // 2. 算法参数
 *       {0x00, 8, 0x07, 0x00, 0, 0};            //    (init,size,poly,xor,refin,refout)
 *   my_crc.algo = &my_crc_algo;                 // 3. 绑定算法
 *   BSP_CRC_GenTable(my_crc.algo, my_crc_table);// 4. 生成表（生成时设置算法）
 *   uint32_t c = BSP_CRC_TableCalc(&my_crc, data, len); // 5. 通用查表计算
 *   // 兜底：uint32_t d = BSP_CRC_Direct(my_crc.algo, data, len); */
#define BSP_CRC_CUSTOM_DEF(name)       \
    static uint32_t name##_table[256]; \
    static BSP_CRC_Table_t name = {NULL, name##_table}

#endif /* __BSP_CRC_H */

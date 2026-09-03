/**
 * @file lib_crc_tables.h
 * @brief 常用 CRC 算法 Flash 表 + 算法/查表描述符（由 lib_crc_tables_gen.py 生成）
 *
 * 表为 const，放 FLASH：零 RAM、零运行时建表。用法：
 *   uint32_t c = LIB_CRC_TableCalc(&LIB_CRC_TBL_CRC8, data, len);
 * 重新生成：python lib_crc_tables_gen.py
 */

#ifndef __LIB_CRC_TABLES_H
#define __LIB_CRC_TABLES_H

#include "lib_crc.h"

/* 6 张查表（256 项 uint32_t，const FLASH） */
extern const uint32_t LIB_CRC_TABLE_CRC8[256];
extern const uint32_t LIB_CRC_TABLE_CRC8_MAXIM[256];
extern const uint32_t LIB_CRC_TABLE_CRC16_CCITT[256];
extern const uint32_t LIB_CRC_TABLE_CRC16_KERMIT[256];
extern const uint32_t LIB_CRC_TABLE_CRC16_MODBUS[256];
extern const uint32_t LIB_CRC_TABLE_CRC32[256];

/* 8 个算法描述符 */
extern const LIB_CRC_Algo_t LIB_CRC_ALGO_CRC8;
extern const LIB_CRC_Algo_t LIB_CRC_ALGO_CRC8_MAXIM;
extern const LIB_CRC_Algo_t LIB_CRC_ALGO_CRC16_CCITT_FALSE;
extern const LIB_CRC_Algo_t LIB_CRC_ALGO_CRC16_XMODEM;
extern const LIB_CRC_Algo_t LIB_CRC_ALGO_CRC16_KERMIT;
extern const LIB_CRC_Algo_t LIB_CRC_ALGO_CRC16_MODBUS;
extern const LIB_CRC_Algo_t LIB_CRC_ALGO_CRC16_MAXIM;
extern const LIB_CRC_Algo_t LIB_CRC_ALGO_CRC32;

/* 8 个查表描述符（算法 + 表） */
extern const LIB_CRC_Table_t LIB_CRC_TBL_CRC8;
extern const LIB_CRC_Table_t LIB_CRC_TBL_CRC8_MAXIM;
extern const LIB_CRC_Table_t LIB_CRC_TBL_CRC16_CCITT_FALSE;
extern const LIB_CRC_Table_t LIB_CRC_TBL_CRC16_XMODEM;
extern const LIB_CRC_Table_t LIB_CRC_TBL_CRC16_KERMIT;
extern const LIB_CRC_Table_t LIB_CRC_TBL_CRC16_MODBUS;
extern const LIB_CRC_Table_t LIB_CRC_TBL_CRC16_MAXIM;
extern const LIB_CRC_Table_t LIB_CRC_TBL_CRC32;

#endif /* __LIB_CRC_TABLES_H */

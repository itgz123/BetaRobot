#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 bsp_crc_tables.c / bsp_crc_tables.h：6 张 Flash 表 + 8 个算法描述符。

表生成逐位逻辑复刻 bsp_crc.c 的 BSP_CRC_GenTable（& MASK32 复现 uint32 回卷语义），
保证 Flash 表与运行时 BSP_CRC_GenTable 生成的值完全一致，可混用。
脚本运行前先自校验每个算法的标准 check 向量（"123456789"），不通过不输出。

用法：
    python bsp_crc_tables_gen.py          # 输出到本文件所在目录
"""

import os

MASK32 = 0xFFFFFFFF
CHECK_DATA = b"123456789"


def reflect(value, width):
    """把 value 的低 width 位整体反转（复刻 C CRC_BitReflect）。"""
    r = 0
    for _ in range(width):
        r = (r << 1) | (value & 1)
        value >>= 1
    return r


def gen_table(poly, width, refin):
    """复刻 C BSP_CRC_GenTable 的逐位逻辑（含 uint32 回卷语义）。"""
    if refin:
        poly = reflect(poly, width)
    table = []
    for i in range(256):
        if refin:
            crc = i
            for _ in range(8):
                if crc & 1:
                    crc = ((crc >> 1) ^ poly) & MASK32
                else:
                    crc = (crc >> 1) & MASK32
        else:
            crc = (i << (width - 8)) & MASK32
            for _ in range(8):
                if crc & (1 << (width - 1)):
                    crc = ((crc << 1) ^ poly) & MASK32
                else:
                    crc = (crc << 1) & MASK32
        table.append(crc)
    return table


def table_calc(table, algo, data):
    """复刻 C BSP_CRC_TableCalc，用于自校验。"""
    width = algo["poly_size"]
    mask = MASK32 if width >= 32 else (1 << width) - 1
    crc = algo["init_value"] & mask
    if algo["reverse_in"]:
        for byte in data:
            crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF]
    else:
        for byte in data:
            crc = (crc << 8) ^ table[((crc >> (width - 8)) ^ byte) & 0xFF]
    crc &= mask
    if algo["reverse_in"] != algo["reverse_out"]:
        crc = reflect(crc, width)
    return (crc ^ algo["xor_out"]) & mask


# (算法名, 表名, init, width, poly, xorout, refin, refout, check)
# 表名相同 = (poly,width,refin) 相同，复用同一张表
ALGOS = [
    ("CRC8",               "CRC8",        0x00,        8,  0x07,         0x00,          0, 0, 0xF4),
    ("CRC8_MAXIM",         "CRC8_MAXIM",  0x00,        8,  0x31,         0x00,          1, 1, 0xA1),
    ("CRC16_CCITT_FALSE",  "CRC16_CCITT", 0xFFFF,      16, 0x1021,       0x0000,        0, 0, 0x29B1),
    ("CRC16_XMODEM",       "CRC16_CCITT", 0x0000,      16, 0x1021,       0x0000,        0, 0, 0x31C3),
    ("CRC16_KERMIT",       "CRC16_KERMIT", 0x0000,     16, 0x1021,       0x0000,        1, 1, 0x2189),
    ("CRC16_MODBUS",       "CRC16_MODBUS", 0xFFFF,     16, 0x8005,       0x0000,        1, 1, 0x4B37),
    ("CRC16_MAXIM",        "CRC16_MODBUS", 0x0000,     16, 0x8005,       0xFFFF,        1, 1, 0x44C2),
    ("CRC32",              "CRC32",       0xFFFFFFFF,  32, 0x04C11DB7,   0xFFFFFFFF,    1, 1, 0xCBF43926),
]


def build():
    out_dir = os.path.dirname(os.path.abspath(__file__))

    # 自校验：逐算法用生成的表算 check 向量
    tables = {}          # 表名 -> [256] 表值
    table_params = {}    # 表名 -> (poly, width, refin)
    for name, tname, init, width, poly, xorout, refin, refout, check in ALGOS:
        algo = {
            "init_value": init, "poly_size": width, "poly": poly,
            "xor_out": xorout, "reverse_in": refin, "reverse_out": refout,
        }
        if tname not in tables:
            tables[tname] = gen_table(poly, width, refin)
            table_params[tname] = (poly, width, refin)
        got = table_calc(tables[tname], algo, CHECK_DATA)
        assert got == check, f"自校验失败: {name} got=0x{got:08X} expect=0x{check:08X}"

    # ---- 生成 .h ----
    lines_h = []
    lines_h.append("/**")
    lines_h.append(" * @file bsp_crc_tables.h")
    lines_h.append(" * @brief 常用 CRC 算法 Flash 表 + 算法/查表描述符（由 bsp_crc_tables_gen.py 生成）")
    lines_h.append(" *")
    lines_h.append(" * 表为 const，放 FLASH：零 RAM、零运行时建表。用法：")
    lines_h.append(" *   uint32_t c = BSP_CRC_TableCalc(&BSP_CRC_TBL_CRC8, data, len);")
    lines_h.append(" * 重新生成：python bsp_crc_tables_gen.py")
    lines_h.append(" */")
    lines_h.append("")
    lines_h.append("#ifndef __BSP_CRC_TABLES_H")
    lines_h.append("#define __BSP_CRC_TABLES_H")
    lines_h.append("")
    lines_h.append('#include "bsp_crc.h"')
    lines_h.append("")
    lines_h.append("/* %d 张查表（256 项 uint32_t，const FLASH） */" % len(tables))
    for tname in tables:
        lines_h.append("extern const uint32_t BSP_CRC_TABLE_%s[256];" % tname)
    lines_h.append("")
    lines_h.append("/* %d 个算法描述符 */" % len(ALGOS))
    for name, *_ in ALGOS:
        lines_h.append("extern const BSP_CRC_Algo_t BSP_CRC_ALGO_%s;" % name)
    lines_h.append("")
    lines_h.append("/* %d 个查表描述符（算法 + 表） */" % len(ALGOS))
    for name, *_ in ALGOS:
        lines_h.append("extern const BSP_CRC_Table_t BSP_CRC_TBL_%s;" % name)
    lines_h.append("")
    lines_h.append("#endif /* __BSP_CRC_TABLES_H */")
    lines_h.append("")

    # ---- 生成 .c ----
    lines_c = []
    lines_c.append("/**")
    lines_c.append(" * @file bsp_crc_tables.c")
    lines_c.append(" * @brief 常用 CRC 算法 Flash 表（由 bsp_crc_tables_gen.py 生成，勿手改）")
    lines_c.append(" */")
    lines_c.append("")
    lines_c.append('#include "bsp_crc.h"')
    lines_c.append('#include "bsp_crc_tables.h"')
    lines_c.append('#include "app_cfg.h"')
    lines_c.append("")
    lines_c.append("#ifdef BSP_CRC_USED")
    lines_c.append("")

    for tname, vals in tables.items():
        tp = table_params[tname]
        lines_c.append("/* 查表 (poly=0x%08X, width=%d, refin=%d) */" % (tp[0], tp[1], tp[2]))
        lines_c.append("const uint32_t BSP_CRC_TABLE_%s[256] = {" % tname)
        for i in range(0, 256, 8):
            row = ", ".join("0x%08Xu" % v for v in vals[i:i + 8])
            lines_c.append("    " + row + ",")
        lines_c.append("};")
        lines_c.append("")

    for name, tname, init, width, poly, xorout, refin, refout, check in ALGOS:
        lines_c.append("const BSP_CRC_Algo_t BSP_CRC_ALGO_%s = {" % name)
        lines_c.append("    .init_value  = 0x%08Xu," % init)
        lines_c.append("    .poly_size   = %d," % width)
        lines_c.append("    .poly        = 0x%08Xu," % poly)
        lines_c.append("    .xor_out     = 0x%08Xu," % xorout)
        lines_c.append("    .reverse_in  = %d," % refin)
        lines_c.append("    .reverse_out = %d," % refout)
        lines_c.append("};")
        lines_c.append("")
        lines_c.append("const BSP_CRC_Table_t BSP_CRC_TBL_%s = {" % name)
        lines_c.append("    .algo  = &BSP_CRC_ALGO_%s," % name)
        lines_c.append("    .table = BSP_CRC_TABLE_%s," % tname)
        lines_c.append("};")
        lines_c.append("")

    lines_c.append("#endif /* BSP_CRC_USED */")
    lines_c.append("")

    h_path = os.path.join(out_dir, "bsp_crc_tables.h")
    c_path = os.path.join(out_dir, "bsp_crc_tables.c")
    with open(h_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines_h))
    with open(c_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines_c))
    print("生成完成：%s / %s（自校验 %d 个算法全 PASS）" % (h_path, c_path, len(ALGOS)))


if __name__ == "__main__":
    build()

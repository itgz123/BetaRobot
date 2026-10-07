#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 referee2026_proto.c 的 CRC 查表数据（三张表）。

表值本身是协议常量，一辈子不会变；留脚本是为了可复现、可审计，而不是为了将来重生成。
生成逻辑与 lib_crc/tools/lib_crc_tables_gen.py 同源（逐位复刻 lib_crc.c 的 LIB_CRC_GenTable），
但**目的是核对而不是抄**：本脚本每次运行都会

  1. 用标准 check 向量（"123456789"）自校验生成逻辑；
  2. 用官方协议附录的示例帧核对本模块三条算法的实际输出；
  3. 与 lib/lib_crc/lib_crc_tables.c 的同参数表**逐项比对**（该文件已核对过官方附录表），
     证明"本模块自备的表"与"lib 的表"是同一套数 —— 若某天不一致，这里会直接报错。

用法：
    python referee2026_crc_tables_gen.py            # 自校验 + 写回 proto.c
    python referee2026_crc_tables_gen.py --check    # 只校验，不写文件
"""

import os
import re
import sys

MASK32 = 0xFFFFFFFF

# ---- 本模块要的三张表：(变量名, poly, width, refin) ----
# 表只由 (poly, width, refin) 决定；init/xorout/refout 在查表时另外施加，不影响表值。
TABLES = [
    ("s_referee2026_crc8_tab", 0x31, 8, 1),    # 帧头 CRC8（附录一 CRC8_TAB）
    ("s_referee2026_crc16_tab", 0x1021, 16, 1),  # 整包 CRC16（附录一 wCRC_Table）
    ("s_referee2026_crc16_remote_tab", 0x1021, 16, 0),  # 0xA9 遥控帧（非反射）
]

# ---- 与 lib_crc_tables.c 的对应关系（用于交叉比对）----
LIB_PEER = {
    "s_referee2026_crc8_tab": "LIB_CRC_TABLE_CRC8_MAXIM",
    "s_referee2026_crc16_tab": "LIB_CRC_TABLE_CRC16_KERMIT",
    "s_referee2026_crc16_remote_tab": "LIB_CRC_TABLE_CRC16_CCITT",
}

# ---- 本模块三条算法（与 referee2026_proto.c 里的算法描述一致）----
ALGOS = {
    "crc8": {"init_value": 0xFF, "poly_size": 8, "poly": 0x31, "xor_out": 0x00, "refin": 1, "refout": 1,
             "tab": "s_referee2026_crc8_tab"},
    "crc16": {"init_value": 0xFFFF, "poly_size": 16, "poly": 0x1021, "xor_out": 0x0000, "refin": 1, "refout": 1,
              "tab": "s_referee2026_crc16_tab"},
    "crc16_remote": {"init_value": 0xFFFF, "poly_size": 16, "poly": 0x1021, "xor_out": 0x0000, "refin": 0, "refout": 0,
                     "tab": "s_referee2026_crc16_remote_tab"},
}

# ---- 官方协议附录的示例帧（A5 帧头 4B / 整包 18B / 0xA9 帧 21B），期望值见下 ----
FRAME_HDR = bytes([0xA5, 0x0B, 0x00, 0x01])
FRAME_PKT = bytes([0xA5, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11])
FRAME_REMOTE = bytes([0xA9, 0x53]) + bytes(19)

# (算法, 数据, 期望值) —— 期望值是用官方附录一给出的表格与过程独立算出来的
CASE_VECTORS = [
    ("crc8", FRAME_HDR, 0x5C),
    ("crc16", FRAME_PKT, 0x25C8),
    ("crc16_remote", FRAME_REMOTE, 0x9EE5),
    # 标准 check 向量（CRC 目录里的 "123456789"），用来校验生成逻辑本身
    ("crc16", b"123456789", 0x6F91),          # CRC-16/MCRF4XX
    ("crc16_remote", b"123456789", 0x29B1),   # CRC-16/CCITT-FALSE
]


def reflect(value, width):
    r = 0
    for _ in range(width):
        r = (r << 1) | (value & 1)
        value >>= 1
    return r


def gen_table(poly, width, refin):
    """复刻 C LIB_CRC_GenTable 的逐位逻辑（含 uint32 回卷语义）。"""
    if refin:
        poly = reflect(poly, width)
    table = []
    for i in range(256):
        if refin:
            crc = i
            for _ in range(8):
                crc = ((crc >> 1) ^ poly) & MASK32 if crc & 1 else (crc >> 1) & MASK32
        else:
            crc = (i << (width - 8)) & MASK32
            for _ in range(8):
                crc = ((crc << 1) ^ poly) & MASK32 if crc & (1 << (width - 1)) else (crc << 1) & MASK32
        table.append(crc)
    return table


def table_calc(table, algo, data):
    """复刻 C LIB_CRC_TableCalc，语义与 lib_crc.c 一致。"""
    width = algo["poly_size"]
    mask = MASK32 if width >= 32 else (1 << width) - 1
    crc = algo["init_value"] & mask
    if algo["refin"]:
        for byte in data:
            crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF]
    else:
        for b in data:
            crc = (crc << 8) ^ table[((crc >> (width - 8)) ^ b) & 0xFF]
    crc &= mask
    if algo["refin"] != algo["refout"]:
        crc = reflect(crc, width)
    return (crc ^ algo["xor_out"]) & mask


def parse_lib_tables(lib_c_path):
    """从 lib_crc_tables.c 抽出各表的值，用于逐项比对。"""
    with open(lib_c_path, encoding="utf-8") as f:
        text = f.read()
    out = {}
    for name in LIB_PEER.values():
        key = "%s[256]" % name
        if key not in text:
            continue
        seg = text[text.index(key):]
        seg = seg[:seg.index("};")]
        vals = [int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]+)", seg)]
        out[name] = vals[:256]
    return out


def build_text(tables):
    """把三张表排成 proto.c 里的定宽文本（每行 8 项）。"""
    lines = []
    for var, poly, width, refin in TABLES:
        lines.append("/* 查表 (poly=0x%08X, width=%d, refin=%d) */" % (poly, width, refin))
        lines.append("static const uint32_t %s[256] = {" % var)
        vals = tables[var]
        for i in range(0, 256, 8):
            lines.append("    " + ", ".join("0x%08Xu" % v for v in vals[i:i + 8]) + ",")
        lines.append("};")
        lines.append("")
    return "\n".join(lines).rstrip("\n")


def main():
    check_only = "--check" in sys.argv
    out_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # drv/drv_referee2026/
    repo = os.path.dirname(os.path.dirname(out_dir))                       # BetaRobot/
    proto_c = os.path.join(out_dir, "core", "referee2026_proto.c")
    lib_c = os.path.join(repo, "lib", "lib_crc", "lib_crc_tables.c")

    # 1) 生成表
    tables = {var: gen_table(poly, width, refin) for var, poly, width, refin in TABLES}

    # 2) 自校验：标准向量 + 官方示例帧
    for name, data, expect in CASE_VECTORS:
        algo = ALGOS[name]
        got = table_calc(tables[algo["tab"]], algo, data)
        assert got == expect, "自校验失败: %s(%r) got=0x%X expect=0x%X" % (name, data[:4], got, expect)
    print("自校验 %d 组向量全 PASS（含官方示例帧）" % len(CASE_VECTORS))

    # 3) 交叉比对：与 lib 的同参数表逐项相等
    peers = parse_lib_tables(lib_c) if os.path.exists(lib_c) else {}
    for var, lib_name in LIB_PEER.items():
        if lib_name not in peers:
            print("跳过比对（lib 表未找到）: %s" % lib_name)
            continue
        assert tables[var] == peers[lib_name], "与 lib 表不一致: %s vs %s" % (var, lib_name)
    if peers:
        print("与 lib_crc_tables.c 逐项比对全 PASS（%d 张表，各 256 项）" % len(peers))

    if check_only:
        print("--check：未写文件")
        return

    # 4) 写回 proto.c 的生成区
    begin = "/* ==== 生成区开始（referee2026_crc_tables_gen.py） ==== */"
    end = "/* ==== 生成区结束 ==== */"
    with open(proto_c, encoding="utf-8") as f:
        text = f.read()
    if begin not in text or end not in text:
        sys.exit("找不到生成区标记，放弃写回：%s" % proto_c)
    head, rest = text.split(begin, 1)
    _, tail = rest.split(end, 1)
    new = head + begin + "\n\n" + build_text(tables) + "\n\n" + end + tail
    with open(proto_c, "w", encoding="utf-8", newline="\n") as f:
        f.write(new)
    print("写回完成：%s（3 张表，共 %d 项）" % (proto_c, 3 * 256))


if __name__ == "__main__":
    main()

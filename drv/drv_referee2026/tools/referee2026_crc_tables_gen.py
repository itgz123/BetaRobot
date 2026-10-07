#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成 referee2026_proto.c 的 CRC 查表数据（三张表）。

表值本身是协议常量，一辈子不会变；留脚本是为了可复现、可审计，而不是为了将来重生成。
生成逻辑与 lib_crc/tools/lib_crc_tables_gen.py 同源（逐位复刻 lib_crc.c 的 LIB_CRC_GenTable），
但**目的是核对而不是抄**：本脚本每次运行都会

  1. 用标准 check 向量（"123456789"）自校验生成逻辑；
  2. 用官方 0xA5 帧的示例数据核对本模块两条帧校验算法的实际输出；
  3. 与 lib/lib_crc/lib_crc_tables.c 的同参数表**逐项比对**（该文件已核对过官方附录表），
     证明"本模块自备的表"与"lib 的表"是同一套数 —— 若某天不一致，这里会直接报错。

⚠ **0xA9 图传遥控帧那条 CRC 目前没有权威出处，口径存疑**：
   本脚本按**非反射**（CRC-16/CCITT-FALSE）造表，出处是 `referee2026_proto.c` 里记的
   "VT03·VT13 图传说明书" —— 那份文档**不在本仓库**，无法核对。
   而参考工程 `rm_referee` 的 `imageRoad.c` 校验 `0xA9 0x53` 帧用的是 `Verify_CRC16_Check_Sum`
   （**反射**表，与 0xA5 帧同一张），方向与本脚本相反。
   官方 2026 协议与串口协议附录里**根本没有 0xA9 帧**（附录里的图传遥控是走 0xA5 的 0x0304），
   故二者必有一错、且现在判不了谁对。本模块不解析 0xA9，这张表目前**没有运行期消费者**；
   将来真要实现 0xA9 解析前，必须先拿到那份图传说明书把这条钉死。

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
    # 0xA9 遥控帧（非反射）—— 口径存疑且无权威出处，见文件开头的长警告
    ("s_referee2026_crc16_remote_tab", 0x1021, 16, 0),
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

# ---- 官方协议附录的示例帧（A5 帧头 4B / 整包 18B）----
# 注：0xA9 一帧**没有**官方样例可抄 —— 官方 2026 协议与串口协议附录里都没有 0xA9 帧，
#     故下面那条 remote 向量的期望值是本脚本自己按非反射口径算出来的，**不是外部锚点**，
#     它只能证明"生成逻辑没变过"，证明不了"口径对"。真正的口径问题见文件开头。
FRAME_HDR = bytes([0xA5, 0x0B, 0x00, 0x01])
FRAME_PKT = bytes([0xA5, 0x0B, 0x00, 0x00, 0x00, 0x01, 0x00, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11])
FRAME_REMOTE = bytes([0xA9, 0x53]) + bytes(19)   # 自造样本（21B），非官方

# (算法, 数据, 期望值)
CASE_VECTORS = [
    # 前两条：期望值由官方附录一的 C 代码与表格独立算出（a5/0b/00/01 与整包）
    ("crc8", FRAME_HDR, 0x5C),
    ("crc16", FRAME_PKT, 0x25C8),
    # 0xA9 那两条**没有外部出处**：前者是自造样本的自算结果，仅防生成逻辑被改动
    ("crc16_remote", FRAME_REMOTE, 0x9EE5),
    # 标准 check 向量（CRC 目录里的 "123456789"），用来校验生成逻辑本身 —— 这两个才是外部锚
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
    """复刻 C LIB_CRC_GenTable 的逐位逻辑（含"每步按 width 掩码"的语义）。"""
    mask = MASK32 if width >= 32 else (1 << width) - 1
    poly &= mask
    if refin:
        poly = reflect(poly, width)
    table = []
    for i in range(256):
        if refin:
            crc = i
            for _ in range(8):
                crc = ((crc >> 1) ^ poly) & mask if crc & 1 else (crc >> 1) & mask
        else:
            # 非反射路径每步按 width 掩码（不掩会让表项带上 width 以上的垃圾位）
            crc = (i << (width - 8)) & mask
            for _ in range(8):
                crc = ((crc << 1) ^ poly) & mask if crc & (1 << (width - 1)) else (crc << 1) & mask
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
    proto_c = os.path.join(out_dir, "public", "referee2026_proto.c")
    lib_c = os.path.join(repo, "lib", "lib_crc", "lib_crc_tables.c")

    # 1) 生成表
    tables = {var: gen_table(poly, width, refin) for var, poly, width, refin in TABLES}

    # 2) 自校验：标准向量 + 官方示例帧
    for name, data, expect in CASE_VECTORS:
        algo = ALGOS[name]
        got = table_calc(tables[algo["tab"]], algo, data)
        assert got == expect, "自校验失败: %s(%r) got=0x%X expect=0x%X" % (name, data[:4], got, expect)
    print("自校验 %d 组向量全 PASS（含官方 A5 示例帧；0xA9 两条无外部出处，见文件头警告）" % len(CASE_VECTORS))

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

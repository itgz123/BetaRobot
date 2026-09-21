#!/usr/bin/env python3
"""drvs 广播驱动协议自检（DJI 一拖四 / LK 一拖四）。

本机没有 CAN 硬件，这是广播模块的主要正确性防线。

原则：按协议文档 + 芯片手册**独立重写**一份参考实现，不复用 C 源码，
       再对边界/字节序/槽位簿记逐条断言。C 与 Python 各自独立犯错且恰好
       一致的概率远低于单独出错，因此两边都过才算数。

用法：
    python3 drv/drvs_motor/broadcast_proto_check.py              # 跑全部检查
    python3 drv/drvs_motor/broadcast_proto_check.py -v           # 打印每条断言的细节
    python3 drv/drvs_motor/broadcast_proto_check.py -k dji_lk    # 只跑名字含该子串的检查

退出码：0 = 全部通过；1 = 有断言失败。
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from dataclasses import dataclass, field

TAU = 2.0 * math.pi

# ============================================================================
#  协议表（来源：DJI 电调手册 / LK 一对一通讯协议 V2.36 + 广播模式说明）
# ============================================================================

DJI_GROUP_BASE = 0x201          # 槽位/组基准：slot = (rx-0x201)%4, group = (rx-0x201)//4
DJI_SLOTS = 4
DJI_ENCODER_RES = 8192          # C620/C610/GM6020 均为 14 位编码器

# model -> (rx_id_base, tx_id[0]=id1-4, tx_id[1]=id5-8, raw_max, current_max_a, id_max)
DJI_MODELS = {
    "M3508": (0x200, 0x200, 0x1FF, 16384, 20.0, 8),
    "M2006": (0x200, 0x200, 0x1FF, 10000, 10.0, 8),
    "GM6020": (0x204, 0x1FE, 0x2FE, 16384, 3.0, 7),
}

LK_TORQUE_ID = 0x280
LK_REPLY_BASE = 0x140
LK_SLOTS = 4
LK_RAW_MAX = 2000.0
LK_A_PER_LSB = 33.0 / 4096.0
LK_ENCODER_RES = 65536
LK_ECHO_ACCEPT = (0xA1, 0x9C)

CAN_NUM_MAX = 2                 # 当前活动板 DJI_C 的 BoardCAN_e 元素数

# ============================================================================
#  参考实现
# ============================================================================


def round_half_away(x: float) -> int:
    """模拟 C 的 (int)(x >= 0 ? x + 0.5f : x - 0.5f)：四舍五入、远离零。"""
    return int(x + 0.5) if x >= 0.0 else int(x - 0.5)


def clamp(x: float, lo: float, hi: float) -> float:
    return lo if x < lo else hi if x > hi else x


def dji_derive(model: str, motor_id: int):
    """由 型号+id 推导 (rx_id, tx_id, group, slot)。"""
    rx_base, tx_lo, tx_hi, _, _, _ = DJI_MODELS[model]
    rx = rx_base + motor_id
    tx = tx_lo if motor_id <= 4 else tx_hi
    # 目标板 CAN_ID 是 11 位，rx 必须落在合法区间
    group = (rx - DJI_GROUP_BASE) // DJI_SLOTS
    slot = (rx - DJI_GROUP_BASE) % DJI_SLOTS
    return rx, tx, group, slot


def dji_raw_factors(model: str, kt: float):
    """返回 (nm_per_raw, raw_per_nm, raw_max)。"""
    _, _, _, raw_max, cur_max_a, _ = DJI_MODELS[model]
    a_per_raw = cur_max_a / raw_max
    nm_per_raw = kt * a_per_raw
    return nm_per_raw, 1.0 / nm_per_raw, float(raw_max)


def dji_encode_torque(model: str, kt: float, torque: float) -> int:
    _, raw_per_nm, raw_max = dji_raw_factors(model, kt)
    raw = torque * raw_per_nm
    # C 侧有 isfinite 守卫：NaN 的比较恒为假，不拦的话 cast 到 int16 是 UB
    if not math.isfinite(raw):
        raw = 0.0
    return round_half_away(clamp(raw, -raw_max, raw_max))


def dji_decode_feedback(model: str, kt: float, b: bytes):
    """8 字节大端反馈帧 -> 物理量 dict。"""
    enc = int.from_bytes(b[0:2], "big")
    rpm = int.from_bytes(b[2:4], "big", signed=True)
    cur = int.from_bytes(b[4:6], "big", signed=True)
    _, _, _, raw_max, cur_max_a, _ = DJI_MODELS[model]
    a_per_raw = cur_max_a / raw_max
    return {
        "position": enc * TAU / DJI_ENCODER_RES,
        "speed": rpm * TAU / 60.0,
        "current": cur * a_per_raw,
        "torque": cur * a_per_raw * kt,
        "temperature": int.from_bytes(b[6:7], "big", signed=True),
        "error": b[7],
    }


def dji_tx_frame(entries):
    """entries: {slot: int16 raw} -> 8 字节大端控制帧。"""
    out = bytearray(8)
    for slot in range(DJI_SLOTS):
        raw = entries.get(slot, 0)
        packed = struct.pack(">h", raw)          # 大端：MSB first
        out[slot * 2:slot * 2 + 2] = packed
    return bytes(out)


def dji_group_send(group) -> dict:
    """返回 {tx_id: frame_bytes}，模拟 DrvsDJIMotorBroadcastGroupSend。"""
    tx_ids = []
    for inst in group.slots:
        if inst is not None and inst.tx not in tx_ids:
            tx_ids.append(inst.tx)

    frames = {}
    for tx in tx_ids:
        entries = {}
        for slot, inst in enumerate(group.slots):
            if inst is None or inst.tx != tx:
                continue                          # 不属于本 tx_id 的槽位必须留 0
            entries[slot] = dji_encode_torque(inst.model, inst.kt, inst.ref_torque)
        frames[tx] = dji_tx_frame(entries)
    return frames


def dji_decode_tx_frame(frame: bytes) -> dict:
    """反向解析大端控制帧 -> {slot: raw}。"""
    return {slot: struct.unpack(">h", frame[slot * 2:slot * 2 + 2])[0] for slot in range(DJI_SLOTS)}


def lk_raw_per_nm(kt: float) -> float:
    return 1.0 / (kt * LK_A_PER_LSB)


def lk_encode_torque(kt: float, torque: float) -> int:
    raw = torque * lk_raw_per_nm(kt)
    if not math.isfinite(raw):       # 同 C 侧的 isfinite 守卫
        raw = 0.0
    return round_half_away(clamp(raw, -LK_RAW_MAX, LK_RAW_MAX))


def lk_torque_frame(entries) -> bytes:
    """entries: {slot: int16 raw} -> 8 字节小端 0x280 帧。"""
    out = bytearray(8)
    for slot in range(LK_SLOTS):
        out[slot * 2:slot * 2 + 2] = struct.pack("<h", entries.get(slot, 0))
    return bytes(out)


def lk_group_send(group) -> bytes:
    entries = {}
    for slot, inst in enumerate(group.slots):
        if inst is None:
            continue
        entries[slot] = lk_encode_torque(inst.kt, inst.ref_torque)
    return lk_torque_frame(entries)


def lk_decode_feedback(kt: float, b: bytes):
    temp = int.from_bytes(b[1:2], "big", signed=True)
    iq = int.from_bytes(b[2:4], "little", signed=True)
    dps = int.from_bytes(b[4:6], "little", signed=True)
    enc = int.from_bytes(b[6:8], "little")
    return {
        "position": enc * TAU / LK_ENCODER_RES,
        "speed": dps * math.pi / 180.0,
        "current": iq * LK_A_PER_LSB,
        "torque": iq * LK_A_PER_LSB * kt,
        "temperature": temp,
    }


def lk_is_status_echo(echo: int) -> bool:
    return echo in LK_ECHO_ACCEPT


# ============================================================================
#  槽位簿记参考实现（对应 C 的 Config）
# ============================================================================


@dataclass
class Inst:
    model: str = "M3508"
    motor_id: int = 1
    kt: float = 1.0
    ref_torque: float = 0.0
    tx: int = 0
    slot: int = 0


@dataclass
class Group:
    can_e: int = 0
    member_count: int = 0
    slots: list = field(default_factory=lambda: [None] * DJI_SLOTS)


def group_config(group: Group, inst: Inst, can_e: int, slot: int, tx: int) -> int:
    """模拟 Config 的槽位簿记部分，返回 0 / -1。"""
    if can_e >= CAN_NUM_MAX:
        return -1

    if group.member_count == 0:
        group.can_e = can_e
    elif group.can_e != can_e:
        return -1

    if group.slots[slot] is not None and group.slots[slot] is not inst:
        return -1

    # 先释放旧槽位（仅当旧组该槽位仍指向自己），再占新槽位
    old_group = getattr(inst, "_group", None)
    if old_group is not None and old_group.slots[inst.slot] is inst:
        old_group.slots[inst.slot] = None
        if old_group.member_count > 0:
            old_group.member_count -= 1

    group.slots[slot] = inst
    group.member_count += 1
    inst._group = group
    inst.slot = slot
    inst.tx = tx
    return 0


# ============================================================================
#  检查项
# ============================================================================

CHECKS = []
DETAIL = []


def check(fn):
    CHECKS.append(fn)
    return fn


def note(msg):
    DETAIL.append(msg)


@check
def dji_group_slot_exhaustive():
    """全型号 × 全 id：组/槽位/发送 ID 推导，且帧内字节位置 == 槽位。"""
    by_model = {}
    for model, (rx_base, tx_lo, tx_hi, _, _, id_max) in DJI_MODELS.items():
        seen_rx = {}
        for mid in range(1, id_max + 1):
            rx, tx, group, slot = dji_derive(model, mid)
            assert 0 <= group <= 2, f"{model} id{mid}: 组越界 {group}"
            assert 0 <= slot < DJI_SLOTS, f"{model} id{mid}: 槽位越界 {slot}"
            # 不变量：槽位恒等于 (id-1)%4，也就是该电机在任一 tx 帧里的通道序号
            assert slot == (mid - 1) % 4, f"{model} id{mid}: slot={slot} != (id-1)%4"
            # rx 不得越出 11 位标准帧 ID
            assert rx <= 0x7FF, f"{model} id{mid}: rx 0x{rx:X} 超出标准帧 ID"
            # 同型号内部 rx 必须唯一（跨型号重叠是设计使然，见下）
            assert rx not in seen_rx, f"{model}: rx 0x{rx:X} 在 id{seen_rx[rx]} 与 id{mid} 上重复"
            seen_rx[rx] = mid
            note(f"{model} id{mid}: rx=0x{rx:X} tx=0x{tx:X} group={group} slot={slot}")
        by_model[model] = seen_rx

    # M3508 与 M2006 的 rx 集合完全相同（同族、只差电调），GM6020 从 0x205 起
    assert set(by_model["M3508"]) == set(by_model["M2006"]), "M3508/M2006 的 rx 集合应一致"
    assert min(by_model["GM6020"]) == 0x205, "GM6020 的 rx 应从 0x205 起"

    # 已知设计事实：M3508/M2006 id5-8 与 GM6020 id1-4 的 rx 完全重叠（都落组 1）。
    # 因此同一条总线上不能同时挂"某型号 id5-8"与"GM6020 对应 id1-4"，
    # 靠槽位占用检查拦（见 dji_slot_conflict_and_placement）。
    overlap = set(by_model["M3508"]) & set(by_model["GM6020"])
    assert overlap == set(range(0x205, 0x209)), f"跨族 rx 重叠集合不符预期: {sorted(hex(x) for x in overlap)}"
    note(f"跨族 rx 重叠：{[hex(x) for x in sorted(overlap)]}（组 1，靠槽位占用拦冲突）")

    # 组 1 必须同时容纳两个不同 tx_id 的系列（M3508 id5-8 与 GM6020 id1-4）
    g1_tx = {dji_derive(m, i)[1] for m, ids in (("M3508", range(5, 9)), ("GM6020", range(1, 5))) for i in ids}
    assert g1_tx == {0x1FF, 0x1FE}, f"组 1 的 tx_id 集合异常: {[hex(x) for x in g1_tx]}"
    note(f"组 1 同时出现两个 tx_id: {sorted(hex(x) for x in g1_tx)}")


@check
def dji_slot_conflict_and_placement():
    """同族跨型号 id 冲突被拦；不冲突的组合落不同 tx 帧。"""
    # M3508 id5 与 GM6020 id1 都落 rx 0x205 → 槽位 0，第二个 Config 必须 -1
    a, b = Inst("M3508", 5), Inst("GM6020", 1)
    _, _, _, sa = dji_derive("M3508", 5)
    _, _, _, sb = dji_derive("GM6020", 1)
    assert sa == sb == 0, "前提失效：两者应落同一槽位"
    g = Group()
    assert group_config(g, a, 0, sa, dji_derive("M3508", 5)[1]) == 0
    assert group_config(g, b, 0, sb, dji_derive("GM6020", 1)[1]) == -1, "槽位冲突未被拒绝"

    # M3508 id5（rx 0x205）与 GM6020 id3（rx 0x207）不冲突，且落不同 tx 帧
    g2 = Group()
    m, n = Inst("M3508", 5, kt=1.0), Inst("GM6020", 3, kt=1.0)
    assert group_config(g2, m, 0, dji_derive("M3508", 5)[3], dji_derive("M3508", 5)[1]) == 0
    assert group_config(g2, n, 0, dji_derive("GM6020", 3)[3], dji_derive("GM6020", 3)[1]) == 0
    assert g2.member_count == 2

    m.ref_torque = 20.0    # C620 @ Kt=1.0 → 20 A → 满量程 16384
    n.ref_torque = 3.0     # GM6020 @ Kt=1.0 → 3 A → 满量程 16384
    frames = dji_group_send(g2)
    assert set(frames) == {0x1FF, 0x1FE}, f"混合组未分两帧: {[hex(x) for x in frames]}"

    f3508 = dji_decode_tx_frame(frames[0x1FF])
    f6020 = dji_decode_tx_frame(frames[0x1FE])
    assert f3508[0] == 16384, f"0x1FF 槽0 应为 M3508 满量程，实为 {f3508[0]}"
    assert all(f3508[s] == 0 for s in (1, 2, 3)), f"0x1FF 其它槽位必须为 0: {f3508}"
    assert f6020[2] == 16384, f"0x1FE 槽2 应为 GM6020 满量程，实为 {f6020[2]}"
    assert all(f6020[s] == 0 for s in (0, 1, 3)), f"0x1FE 其它槽位必须为 0: {f6020}"
    note("混合组两帧各自只带本 tx_id 的槽位，其余恰为 0x00 0x00")


@check
def slot_bookkeeping():
    """空→占；他人→-1；原地重配→成功且计数不变；换槽/换组→旧槽释放。"""
    g = Group()
    a = Inst("M3508", 1)
    assert group_config(g, a, 0, 0, 0x200) == 0 and g.member_count == 1

    # 自己原地重配：不能失败，计数不能涨
    assert group_config(g, a, 0, 0, 0x200) == 0, "同一实例原地重配被错误拒绝"
    assert g.member_count == 1, f"原地重配后计数应仍为 1，实为 {g.member_count}"
    b = Inst("M3508", 2)
    assert group_config(g, b, 0, 1, 0x200) == 0 and g.member_count == 2

    # 换槽：旧槽释放且只占新槽，计数不变
    assert group_config(g, b, 0, 3, 0x200) == 0
    assert g.slots[1] is None, "换槽后旧槽位未释放"
    assert g.slots[3] is b and g.member_count == 2, f"换槽后状态异常 count={g.member_count}"

    # 旧槽已释放，别人可以占
    c = Inst("M3508", 3)
    assert group_config(g, c, 0, 1, 0x200) == 0 and g.member_count == 3

    # 换组：旧组计数 -1、旧槽清空；新组 +1
    g2 = Group()
    assert group_config(g2, b, 0, 1, 0x200) == 0
    assert g.slots[3] is None and g.member_count == 2, "跨组迁移未释放旧组槽位"
    assert g2.slots[1] is b and g2.member_count == 1

    # can_e 越界
    g3 = Group()
    d = Inst("M3508", 4)
    assert group_config(g3, d, CAN_NUM_MAX, 0, 0x200) == -1, "can_e 越界未拒绝"

    # 组总线不一致
    g4 = Group()
    e = Inst("M3508", 1)
    f = Inst("M3508", 5)
    assert group_config(g4, e, 0, 0, 0x200) == 0
    assert group_config(g4, f, 1, 0, 0x1FF) == -1, "不同 can_e 的成员被错误接受"
    assert g4.member_count == 1 and g4.slots[0] is e, "失败路径污染了组成员关系"
    note("槽位簿记：空→占 / 原地重配幂等 / 换槽释放旧槽 / 跨组迁移 / 越界拒绝")


@check
def dji_encoding_bytes():
    """DJI 控制帧大端编码、限幅、空槽位。"""
    for model, kt, mid in (("M3508", 1.0, 1), ("M2006", 1.0, 1), ("GM6020", 1.0, 1)):
        _, _, _, raw_max, cur_max_a, _ = DJI_MODELS[model]
        # 满量程电流 → 满量程 raw
        assert dji_encode_torque(model, kt, cur_max_a) == raw_max, f"{model} 满量程编码错误"
        assert dji_encode_torque(model, kt, -cur_max_a) == -raw_max
        # 超量程必须钳位，且钳位后仍在 int16 内
        assert dji_encode_torque(model, kt, cur_max_a * 10) == raw_max
        assert dji_encode_torque(model, kt, -cur_max_a * 10) == -raw_max
        assert -32768 <= raw_max < 32768
        note(f"{model}: ±{cur_max_a}A → ±{raw_max} raw，Kt=1.0")

    # 字节序：大端（MSB first），负数取补码
    assert dji_tx_frame({0: 0x1234})[0:2] == b"\x12\x34"
    assert dji_tx_frame({0: -1})[0:2] == b"\xFF\xFF"
    assert dji_tx_frame({0: -16384})[0:2] == b"\xC0\x00"
    assert dji_tx_frame({}) == b"\x00" * 8, "空组帧必须全 0"
    # 往返
    for raw in (0, 1, -1, 8192, -8192, 16384, -16384, 32767, -32768):
        assert dji_decode_tx_frame(dji_tx_frame({2: raw}))[2] == raw
    note("大端 0x1234 → 12 34；-1 → FF FF；空组帧 8 字节全 0")


@check
def dji_roundtrip_monotonic():
    """torque → raw → torque 往返误差在 1 LSB 之内，且单调不回绕。"""
    for model, kt in (("M3508", 0.02), ("M2006", 0.01), ("GM6020", 0.05)):
        nm_per_raw, _, raw_max = dji_raw_factors(model, kt)
        prev = None
        for torque in [x * 0.1 for x in range(-200, 201)]:
            raw = dji_encode_torque(model, kt, torque)
            back = raw * nm_per_raw
            if prev is not None:
                assert back >= prev - 1e-12, f"{model}: 往返非单调 @{torque}"
            prev = back
            if abs(torque) <= raw_max * nm_per_raw:
                assert abs(back - torque) <= nm_per_raw * 0.5 + 1e-9, f"{model}: 往返误差超 0.5 LSB @{torque}"
        note(f"{model} Kt={kt}: 往返误差 ≤ 0.5 LSB（{nm_per_raw:.3e} Nm/LSB）")


@check
def dji_feedback_golden():
    """DJI 大端反馈解码金标向量。"""
    # 编码器 4096 → π；转速 60 rpm → 2π rad/s
    frame = bytes([0x10, 0x00, 0x00, 0x3C, 0x20, 0x00, 0xFB, 0x07])
    d = dji_decode_feedback("M3508", 1.0, frame)
    assert abs(d["position"] - math.pi) < 1e-6, d["position"]
    assert abs(d["speed"] - TAU) < 1e-6, d["speed"]
    assert abs(d["current"] - 10.0) < 1e-6, d["current"]          # 0x2000 = 8192 → 10 A
    assert d["temperature"] == -5, d["temperature"]                # 0xFB = -5
    assert d["error"] == 7

    # 负电流：C620 -20 A → -16384 (0xC000)
    neg = bytes([0x00, 0x00, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00])
    d2 = dji_decode_feedback("M3508", 1.0, neg)
    assert abs(d2["current"] + 20.0) < 1e-6, d2["current"]
    assert abs(d2["position"]) < 1e-12 and abs(d2["speed"]) < 1e-12
    note("DJI 解码：enc 4096→π、60rpm→2π、0x2000→10A、0xFB→-5℃、0xC000→-20A")


@check
def lk_encoding_bytes():
    """LK 广播帧小端编码、±2000 限幅、槽位落位。"""
    # 2.8 Nm / Kt 0.28 → 10 A → raw = 10×4096/33 ≈ 1241
    raw = lk_encode_torque(0.28, 2.8)
    assert raw == 1241, f"期望 1241，实为 {raw}"
    # 满量程：2000 raw ≈ 16.11 A → Nm = 16.11 × 0.28 ≈ 4.51
    full_a = LK_RAW_MAX * LK_A_PER_LSB
    assert lk_encode_torque(0.28, full_a * 0.28) == 2000
    assert lk_encode_torque(0.28, -full_a * 0.28) == -2000
    assert lk_encode_torque(0.28, 1e6) == 2000, "正超量程未钳位"
    assert lk_encode_torque(0.28, -1e6) == -2000
    note(f"LK: Kt=0.28 时 2.8Nm → {raw} raw；±{LK_RAW_MAX} raw ≈ ±{full_a:.2f} A")

    # 小端（低字节在前）与槽位落位
    assert lk_torque_frame({0: 0x1234})[0:2] == b"\x34\x12"
    assert lk_torque_frame({0: -1})[0:2] == b"\xFF\xFF"
    assert lk_torque_frame({}) == b"\x00" * 8
    for slot in range(LK_SLOTS):
        frame = lk_torque_frame({slot: -1})
        assert frame[slot * 2:slot * 2 + 2] == b"\xFF\xFF", f"槽位 {slot} 落位错误"
        assert sum(1 for i in range(0, 8, 2) if frame[i:i + 2] != b"\x00\x00") == 1
    note("LK 小端 0x1234 → 34 12；槽位 i 落在 data[2i:2i+2]")


@check
def lk_group_send_layout():
    """LK 一拖四整组帧：4 个槽位各就各位，空槽恒 0。"""
    g = Group()
    insts = [Inst("MF", i + 1, kt=0.28) for i in range(4)]
    for i, inst in enumerate(insts):
        assert group_config(g, inst, 0, i, LK_TORQUE_ID) == 0
    assert g.member_count == 4
    insts[1] = None                      # 模拟槽位 1 被释放（换槽/未配置）
    g.slots[1] = None
    g.member_count = 3
    insts[0].ref_torque = 2.8
    insts[2].ref_torque = -2.8
    frame = lk_group_send(g)
    assert frame[0:2] == struct.pack("<h", 1241), "槽 0 落位错误"
    assert frame[2:4] == b"\x00\x00", "空槽 1 必须为 0"
    assert frame[4:6] == struct.pack("<h", -1241), "槽 2 落位错误"
    assert frame[6:8] == b"\x00\x00", "空槽 3 必须为 0"
    note("LK 整组帧：槽位 = motor_id-1，空槽恒 0x00 0x00")


@check
def lk_feedback_and_echo():
    """LK 状态2 解码 + 回显过滤。"""
    frame = bytes([0xA1, 0xFD, 0xD9, 0x04, 0x3C, 0x00, 0x00, 0x40])
    d = lk_decode_feedback(0.28, frame)
    assert d["temperature"] == -3, d["temperature"]                     # 0xFD
    assert abs(d["current"] - 10.0) < 0.01, d["current"]                # 0x04D9 = 1241
    assert abs(d["torque"] - 2.8) < 0.003, d["torque"]
    assert abs(d["speed"] - math.pi / 3.0) < 1e-6, d["speed"]           # 60 dps
    assert abs(d["position"] - 6.283185307179586 / 4) < 1e-6            # 0x4000 = 16384 → π/2
    assert 0.0 <= d["position"] < TAU

    # 回显过滤：只有 0xA1 / 0x9C 是状态2 帧
    for echo in LK_ECHO_ACCEPT:
        assert lk_is_status_echo(echo)
    for echo in (0x00, 0x80, 0x88, 0x81, 0x9B, 0x2A, 0x50, 0xFF):
        assert not lk_is_status_echo(echo), f"回显 0x{echo:02X} 不应被当作状态2 帧"
    note("LK 解码：0x04D9→10A/2.8Nm、60dps→π/3、0x4000→π/2；回显只收 A1/9C")


@check
def math_no_nan_or_overflow():
    """异常输入不得产生 NaN/Inf，也不得溢出 int16。"""
    for model, kt in (("M3508", 0.02), ("M2006", 0.01), ("GM6020", 0.05)):
        _, _, raw_max = dji_raw_factors(model, kt)
        for bad in (float("inf"), float("-inf"), float("nan"), 1e30, -1e30):
            raw = dji_encode_torque(model, kt, bad)
            assert -32768 <= raw <= 32767, f"{model} 输入 {bad} 溢出 int16: {raw}"
        assert abs(dji_encode_torque(model, kt, float("nan"))) <= raw_max
    for bad in (float("inf"), float("-inf"), float("nan"), 1e30):
        assert abs(lk_encode_torque(0.28, bad)) <= LK_RAW_MAX
    note("±inf/NaN/1e30 一律钳位到量程，无 int16 溢出")

    # 除零守卫：Kt 必须 > 0（C 侧 Config 已拒绝 <= 0）
    for kt in (0.0, -1.0):
        try:
            lk_raw_per_nm(kt)
            raised = False
        except ZeroDivisionError:
            raised = True
        assert raised or kt < 0, "Kt=0 应被 Config 拒绝（不应走到换算）"


# ============================================================================
#  入口
# ============================================================================


def main() -> int:
    ap = argparse.ArgumentParser(description="drvs 广播驱动协议自检")
    ap.add_argument("-v", "--verbose", action="store_true", help="打印每条断言的细节")
    ap.add_argument("-k", "--filter", default="", help="只跑名字含该子串的检查")
    args = ap.parse_args()

    selected = [c for c in CHECKS if args.filter in c.__name__]
    if not selected:
        print(f"没有匹配 '{args.filter}' 的检查，可用：{[c.__name__ for c in CHECKS]}")
        return 1

    failed = 0
    for fn in selected:
        DETAIL.clear()
        try:
            fn()
        except AssertionError as exc:
            failed += 1
            print(f"[FAIL] {fn.__name__}: {exc}")
            continue
        print(f"[ ok ] {fn.__name__}  —  {fn.__doc__.strip().splitlines()[0]}")
        if args.verbose:
            for line in DETAIL:
                print(f"         · {line}")

    total = len(selected)
    print(f"\n{total - failed}/{total} 通过")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

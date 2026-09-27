#!/usr/bin/env python3
"""把 drv_daemon 的离线日志解析回模块名，并给出掉线时间线/统计。

为什么需要这个脚本
------------------
1) drv_daemon.c 里的两条日志打印的是 `owner_id`：

    BSPLOG(..., "Module 0x%08X OFFLINE", (uint32_t)(uintptr_t)dins->owner_id)

   而 owner_id 是各模块**实例结构体的绝对地址**（DaemonConfig 把 `inst` 填进去）。
   所以日志里的 `0x20000114` 既不是 CAN ID 也不是模块号，只能拿"同一次编译出的固件"
   的符号表（nm -S / .map）反查——即所谓"反汇编检查哪个模块"。

2) 时间戳是 `DWT_GetTimeUs()` 的原始值，单位由 bsp_log.h 的 TIME_STAMP_STYLE 决定
   （0=无时间戳 / 1=us（默认） / 2=ms）。**读错单位会让结论整体差 1000 倍**，
   所以本脚本把单位做成显式参数，并顺手自检（见下）。

用法
----
    # 1) 单板（默认按 TIME_STAMP_STYLE==1，即微秒）
    python3 scripts/daemon-offline-report.py build/Debug/half_rudder_chassis.elf chassis.log

    # 2) 多板合并统计（分别给各自的 elf）
    python3 scripts/daemon-offline-report.py --pair build/Debug/half_rudder_chassis.elf chassis.log \
                                                    build/Debug/half_rudder_gimbal.elf  gimbal.log

    # 3) 没有 arm-none-eabi-nm 时，自己先导出 nm 再喂进来
    python3 scripts/daemon-offline-report.py --nm nm.txt chassis.log

    # 4) 某个 app 的 app_cfg.h 里把 TIME_STAMP_STYLE 改成了 2
    python3 scripts/daemon-offline-report.py --unit ms <elf> <log>

自检
----
- 符号命中率：日志地址落在符号区间内的比例。全部命中 → 符号表与日志同源；
  大量落空 → elf 与日志不是同一次编译，模块名不可信（脚本会警告）。
- 时间单位：daemon 任务周期是 DAEMON_FREQ_MS（默认 1ms），而模块"离线→恢复"
  的最小间隔受它量化——也就是说最短的那批间隔应该 ≈ 1ms。若算出来的最短间隔
  离 1ms 很远（≈0.001ms 或 ≈1000ms），说明 --unit 传错了。
"""

import argparse
import bisect
import os
import re
import shutil
import subprocess
import sys

# 日志行： [E][101169][drv_daemon]:Module 0x200007C8 OFFLINE
LINE_RE = re.compile(
    r"^\[(?P<lvl>[DIWE])\]\[(?P<t>\d+)\]\[(?P<tag>[^\]]*)\]:"
    r"Module 0x(?P<id>[0-9A-Fa-f]+) (?P<ev>OFFLINE|back ONLINE)"
)

NM_CANDIDATES = ["arm-none-eabi-nm", "nm"]

# 各时间戳单位对应的"1 个计数 = 多少 us"
TICK_US = {"us": 1.0, "ms": 1000.0}

# daemon 任务周期（drv_daemon.h 的 DAEMON_FREQ_MS，ms）：离线→恢复间隔的量化台阶
DAEMON_FREQ_MS = 1.0


def load_symbols_from_nm(nm_path):
    """解析 `nm -S --defined-only <elf>` 输出 → [(addr, size, name)] 有序表。"""
    syms = []
    with open(nm_path, "r", errors="replace") as fh:
        for line in fh:
            parts = line.split(None, 3)
            if len(parts) < 3:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue  # 非符号行
            if len(parts) == 3:  # 无 size 列
                size, name = 0, parts[2]
            else:
                try:
                    size = int(parts[1], 16)
                except ValueError:
                    continue
                name = parts[3]
            syms.append((addr, size, name.strip()))
    if not syms:
        sys.exit(f"[!] {nm_path} 里没解析出任何符号，确认它是 nm 的输出")
    syms.sort()
    return syms


def dump_nm(elf_path):
    tool = next((t for t in NM_CANDIDATES if shutil.which(t)), None)
    if tool is None:
        sys.exit("[!] 找不到 arm-none-eabi-nm/nm，用 --nm 传入自行导出的 nm 输出")
    out = subprocess.run([tool, "-S", "--defined-only", elf_path],
                         capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"[!] {tool} 失败: {out.stderr.strip()}")
    tmp = f"/tmp/{os.path.basename(elf_path)}.nm"
    with open(tmp, "w") as fh:
        fh.write(out.stdout)
    return tmp, out.stdout.splitlines()


def make_resolver(syms):
    addrs = [s[0] for s in syms]

    def resolve(addr):
        i = bisect.bisect_right(addrs, addr) - 1
        if i < 0:
            return None, False
        base, size, name = syms[i]
        off = addr - base
        inside = (size > 0 and off < size) or (size == 0 and off == 0)
        return f"{name}+{off:#x}" if off else name, inside

    return resolve


def parse_log(path):
    """→ [(t_tick, addr, event)]，保持日志出现顺序。"""
    rows, unmatched = [], 0
    with open(path, "r", errors="replace") as fh:
        for line in fh:
            m = LINE_RE.search(line)
            if not m:
                if "Module 0x" in line:
                    unmatched += 1
                continue
            rows.append((int(m.group("t")), int(m.group("id"), 16), m.group("ev")))
    if not rows:
        sys.exit(f"[!] {path} 里没找到 drv_daemon 的 'Module 0x... OFFLINE' 行")
    return rows, unmatched


def concurrency(intervals):
    """找"同一时刻有 ≥2 个模块在离线"的窗口 —— 判断系统级卡顿 vs 单模块故障。

    intervals: [(name, t_off_tick, t_on_tick|None)]，t_on 为 None 表示日志里没有配对的
    "back ONLINE"（可能是真没恢复，也可能是那条日志被 bsp_log 丢掉了，见文件头自检）。
    → [(t_start, t_end|None, [names])] 按开始时间排序。
    """
    points = []
    for name, off, on in intervals:
        points.append((off, 1, name))
        if on is not None:
            points.append((on, -1, name))
    points.sort(key=lambda p: (p[0], p[1]))  # 同一时刻先处理"上线"，避免虚报重叠

    live, out, cur = set(), [], None
    for t, delta, name in points:
        if delta == 1:
            live.add(name)
            if len(live) >= 2 and cur is None:
                cur = {"start": t, "names": set()}
        if cur is not None:
            cur["names"] |= live
        if delta == -1:
            live.discard(name)
            if len(live) < 2 and cur is not None:
                out.append((cur["start"], t, sorted(cur["names"])))
                cur = None
    if cur is not None:
        out.append((cur["start"], None, sorted(cur["names"])))
    return out


def report(elf, log, syms, unit):
    tick_us = TICK_US[unit]

    def ms(tick):
        """原始计数 → 毫秒。"""
        return tick * tick_us / 1000.0

    rows, _ = parse_log(log)
    resolve = make_resolver(syms)

    stats = {}
    intervals = []
    hit = miss = 0
    for t, addr, ev in rows:
        name, inside = resolve(addr)
        hit += inside
        miss += not inside
        name = name or f"0x{addr:08X}(未命中)"
        st = stats.setdefault(name, {"n": 0, "first": t, "last": t,
                                     "dur": 0, "t_off": None})
        if ev == "OFFLINE":
            st["n"] += 1
            st["t_off"] = t
        else:  # back ONLINE：结算一次掉线时长
            st["last"] = t
            if st["t_off"] is not None:
                st["dur"] += t - st["t_off"]
                intervals.append((name, st["t_off"], t))
                st["t_off"] = None
    # 收尾：日志结束时仍离线（或配对行被丢）的区间，按开放式处理
    intervals += [(name, st["t_off"], None)
                  for name, st in stats.items() if st["t_off"] is not None]

    total = hit + miss
    span_ms = ms(max(t for t, _, _ in rows) - min(t for t, _, _ in rows))
    print(f"\n=== {log}  (符号表: {elf}) ===")
    print(f"地址回查命中 {hit}/{total}"
          + ("  ✅ 符号表与日志同源" if miss == 0 else
             "  ⚠️ 有未命中：elf 与日志可能不是同一次编译，模块名存疑"))
    print(f"时间单位按 --unit={unit} 解读，日志跨度 {span_ms:.3f} ms  "
          f"（末条 {ms(max(t for t, _, _ in rows)):.3f} ms）")

    # 单位自检：离线→恢复的间隔由 daemon 任务周期（DAEMON_FREQ_MS）量化，
    # 故**中位数**应与之同量级。取中位而非最短：中断喂狗的模块（如 BMI088 的 DRDY）
    # 恢复时刻与轮询相位无关，间隔可以远小于一个周期（实测有 46us 的）。
    # 单位传错（us/ms 混）会让这个中位数整体差 1000 倍，一眼可辨。
    gaps = sorted(on - off for _, off, on in intervals if on is not None and on > off)
    if gaps:
        med = ms(gaps[len(gaps) // 2])
        ok = 0.01 * DAEMON_FREQ_MS <= med <= 20 * DAEMON_FREQ_MS
        print(f"单位自检：{len(gaps)} 次离线→恢复的中位间隔 = {med:.3f} ms，"
              + (f"与 daemon 周期 {DAEMON_FREQ_MS:g}ms 同量级 ✅ 单位可信"
                 if ok else
                 f"⚠️ 与 daemon 周期 {DAEMON_FREQ_MS:g}ms 差了几个数量级，--unit 可能传错"))

    print("\n-- 时间线 --")
    for t, addr, ev in sorted(rows):
        name, _ = resolve(addr)
        print(f"  {ms(t):12.3f}ms  {ev:12s}  {name or f'0x{addr:08X}'}")

    print("\n-- 按模块统计（按掉线次数降序） --")
    print(f"  {'模块':<28}{'掉线次数':>8}{'累计离线(ms)':>14}{'首次(ms)':>12}{'末次(ms)':>12}")
    for name, st in sorted(stats.items(), key=lambda kv: -kv[1]["n"]):
        print(f"  {name:<28}{st['n']:>8}{ms(st['dur']):>14.3f}"
              f"{ms(st['first']):>12.3f}{ms(st['last']):>12.3f}")

    conc = concurrency(intervals)
    print(f"\n-- 同时掉线（≥2 个模块同时离线）: {len(conc)} 段 --")
    if not conc:
        print("  无：各模块的掉线互不重叠 → 更像各自的链路/电机问题，不是系统级卡顿")
    for t0, t1, names in conc:
        end = "日志结束仍未配对到 ONLINE（可能是真没恢复，也可能是该行被日志池丢了）" \
            if t1 is None else f"{ms(t1):.3f}ms（持续 {ms(t1 - t0):.3f}ms）"
        print(f"  {ms(t0):12.3f}ms → {end}")
        print(f"      涉及: {' + '.join(names)}")
    print()


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--unit", choices=sorted(TICK_US), default="us",
                    help="时间戳单位（bsp_log.h 的 TIME_STAMP_STYLE：1=us 默认，2=ms）")
    ap.add_argument("--nm", help="已导出的 nm -S 输出（替代 <elf>，只能配一个日志）")
    ap.add_argument("--pair", nargs="+", metavar="ELF LOG",
                    help="多板：交替给出 elf 与其日志")
    ap.add_argument("targets", nargs="*", help="<elf> <log> [<log> ...]")
    args = ap.parse_args()

    jobs = []  # (elf_label, syms, log)
    if args.nm:
        syms = load_symbols_from_nm(args.nm)
        if not args.targets:
            sys.exit("[!] --nm 模式还需要一个日志路径")
        jobs += [(args.nm, syms, p) for p in args.targets]
    elif args.pair:
        if len(args.pair) % 2:
            sys.exit("[!] --pair 需要成对的 <elf> <log>")
        for elf, log in zip(args.pair[::2], args.pair[1::2]):
            nm_tmp, _ = dump_nm(elf)
            jobs.append((elf, load_symbols_from_nm(nm_tmp), log))
    else:
        if len(args.targets) < 2:
            sys.exit("[!] 用法: daemon-offline-report.py <elf> <log> [<log> ...]")
        elf, logs = args.targets[0], args.targets[1:]
        nm_tmp, _ = dump_nm(elf)
        syms = load_symbols_from_nm(nm_tmp)
        jobs += [(elf, syms, p) for p in logs]

    for elf, syms, log in jobs:
        report(elf, log, syms, args.unit)


if __name__ == "__main__":
    main()

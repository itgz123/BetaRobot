#!/usr/bin/env python3
"""生成 drv_buzzer_note_table.h（音名偏移表 + 绝对偏移 → Hz 表）。

基准：C4 = 0，A4 = +9 = 440 Hz；十二平均律四舍五入：
    f(off) = floor(440 * 2 ** ((60 + off - 69) / 12) + 0.5)
覆盖 C0(-48) ~ B8(+59)。生成前用外部锚点自检公式与基准，口径不对直接报错。

用法：
    python3 gen_note_table.py           # 写回 ../drv_buzzer_note_table.h
    python3 gen_note_table.py --check   # 只比较，不写回（CI/提交前用）
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "drv_buzzer_note_table.h"

PITCH_NAMES = ("C", "CS", "D", "DS", "E", "F", "FS", "G", "GS", "A", "AS", "B")
FLAT_ALIASES = {
    "DB": "CS",
    "EB": "DS",
    "GB": "FS",
    "AB": "GS",
    "BB": "AS",
}
OCT_MIN = 0
OCT_MAX = 8
OFF_MIN = 12 * (OCT_MIN - 4)
OFF_MAX = 12 * (OCT_MAX - 4) + 11


def hz_for_offset(off: int) -> int:
    """C4 = 0 时，绝对偏移 off 对应的频率（四舍五入）。"""
    midi = 60 + off
    freq = 440.0 * (2.0 ** ((midi - 69) / 12.0))
    return int(math.floor(freq + 0.5))


def check_anchors() -> None:
    """外部锚点，防止公式/基准被改错。"""
    anchors = {-48: 16, 0: 262, 9: 440, 10: 466, 12: 523, 59: 7902}
    for off, expected in anchors.items():
        actual = hz_for_offset(off)
        if actual != expected:
            sys.exit(f"锚点不一致：off={off} 预期 {expected}，得到 {actual}")


def check_aliases() -> None:
    """降号别名必须指向同八度的升号拼写。"""
    names = set(PITCH_NAMES)
    for flat, sharp in FLAT_ALIASES.items():
        if sharp not in names:
            sys.exit(f"降号别名映射不存在：{flat} -> {sharp}")


def render() -> str:
    lines: list[str] = []
    lines.append("/**")
    lines.append(" * @file drv_buzzer_note_table.h")
    lines.append(" * @brief 音名 → 半音偏移 / 绝对偏移 → Hz 的编译期宏表")
    lines.append(" *")
    lines.append(" * @note 由 tools/gen_note_table.py 生成，勿手改。")
    lines.append(" *       基准：C4 = 0，A4 = +9 = 440 Hz；覆盖 C0(-48) ~ B8(+59)。")
    lines.append(" */")
    lines.append("")
    lines.append("#ifndef __DRV_BUZZER_NOTE_TABLE_H")
    lines.append("#define __DRV_BUZZER_NOTE_TABLE_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("/* 音名 token → 相对 C4 的半音偏移（升号拼写） */")
    for octave in range(OCT_MIN, OCT_MAX + 1):
        base = 12 * (octave - 4)
        for pitch, name in enumerate(PITCH_NAMES):
            lines.append(f"#define BUZZER_PITCH_{name}{octave} ({base + pitch})")
    lines.append("")
    lines.append("/* 降号等音别名（♭B = A♯，以此类推） */")
    for octave in range(OCT_MIN, OCT_MAX + 1):
        for flat, sharp in FLAT_ALIASES.items():
            lines.append(
                f"#define BUZZER_PITCH_{flat}{octave} BUZZER_PITCH_{sharp}{octave}"
            )
    lines.append("")
    lines.append("/* 绝对半音偏移 → Hz；越界返回 0 */")
    lines.append("#define BUZZER_HZ_AT(off) ((uint16_t)BUZZER_HZ_AT_I((int32_t)(off)))")
    lines.append("#define BUZZER_HZ_AT_I(off) \\")
    entries: list[str] = []
    for off in range(OFF_MIN, OFF_MAX + 1):
        entries.append(f"    (off) == {off:>4} ? {hz_for_offset(off):>4}u")
    for i, entry in enumerate(entries):
        suffix = " : \\" if i != len(entries) - 1 else " : 0u"
        lines.append(entry + suffix)
    lines.append("")
    lines.append("#endif /* __DRV_BUZZER_NOTE_TABLE_H */")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="只比较生成结果与现有文件，不写回",
    )
    args = parser.parse_args()

    check_anchors()
    check_aliases()
    generated = render()

    if args.check:
        if not OUT.exists():
            sys.exit(f"缺少生成文件：{OUT}")
        if OUT.read_text(encoding="utf-8") != generated:
            sys.exit(f"生成文件已过期：{OUT}（运行本脚本重新生成）")
        print(f"check ok: {OUT}")
        return 0

    OUT.write_text(generated, encoding="utf-8")
    print(f"generated: {OUT} ({OFF_MIN}..{OFF_MAX})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

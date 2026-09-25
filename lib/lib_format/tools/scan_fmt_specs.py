#!/usr/bin/env python3
"""扫描源码里的格式串，列出 lib_format 不支持的转换说明。

背景：lib_format 只实现 %d %u %x %X %s（可带宽度与 l/ll），其余转换说明落到
`BSPFmtV` 的 default 分支。default 分支的行为在「按字面输出」之外还决定**参数槽
是否被消费**——不消费就会让同一格式串里**后续所有转换**读到别人的参数（无任何
报错）。本脚本用来找出全仓库还有哪些调用点在用不支持的说明符。

用法：
    python3 scan_fmt_specs.py [目录...]        # 默认扫仓库根的 bsp drv drvlib app lib
    python3 scan_fmt_specs.py --all app        # 也报"不在日志宏调用里"的含 % 字面量
    python3 scan_fmt_specs.py --quiet drv      # 只输出不支持项，不输出统计

判定说明（有意宽松，宁可多报）：
- 只检查 C 字符串字面量，跳过注释与字符字面量；
- 只检查出现在这些宏/函数调用里的字面量：BSPLOG / BSPLog / BSPLogV / LIBFORMAT
  （--all 时检查全部含 % 的字面量）；
- 支持的写法：%%（字面一个 %）、%<数字宽度><l|ll|><d|u|x|X|s>；
- 其余（%c %i %p %f %e %g %E %o %zu %-5d 等）都算不支持并列出；
- `%%` 与"以 % 结尾"永不报（那是 lib_format 明确支持的写法）；消息正文里的单个 %
  （"100% 占空比"）会被报出来 —— 有意如此，宁可多报，由人看一眼定性。

退出码：发现不支持的说明符返回 1，否则 0（可用于 CI/提交前检查）。
"""

import argparse
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEFAULT_DIRS = ["bsp", "drv", "drvlib", "app", "lib"]

# 触发"这是格式串"的调用名（出现在字面量同一行之前的调用括号内）
FMT_CALLS = ("BSPLOG", "BSPLog", "BSPLogV", "LIBFORMAT", "LOG_")

# 支持的转换：%% | %<width><len><conv>
SUPPORTED = re.compile(r"%(\d*)(ll|l)?([duxXs])")
PERCENT = re.compile(r"%%")
ANY_SPEC = re.compile(r"%[-+ #0-9.*]*(?:ll|l|h|hh|z|j|t|L)?[a-zA-Z]")


def iter_literals(path: str):
    """产出 (行号, 字面量内容, 该行原文)，顺序与源码一致。

    逐字符扫全文的状态机，而不是按行套正则：注释与字符串会互相包含 ——
    `/* ... "handle=0x%p, channel=%lu" ... */` 这种**文档注释里的示例**会被按行版当成
    真格式串报出来（lib_format.h 的说明段、lib_format.c 的 default 分支注释都是），
    而字符串里的 `//` 又会被按行版当成注释从中间切开。状态机两种都不会错。
    """
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    lines = text.splitlines()

    i, line_no, n = 0, 1, len(text)
    while i < n:
        c = text[i]
        if c == "\n":
            line_no += 1
            i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j  # 行尾换行留给下一轮计数
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            if j < 0:  # 未闭合的块注释：后面全在里面
                line_no += text.count("\n", i)
                i = n
            else:
                line_no += text.count("\n", i, j)
                i = j + 2
        elif c == "'":  # 字符字面量：整体跳过（里面的 % 不是格式说明）
            i += 1
            while i < n and text[i] != "'":
                i += 2 if text[i] == "\\" else 1
            i += 1
        elif c == '"':
            start_line = line_no
            i += 1
            buf = []
            while i < n and text[i] != '"':
                if text[i] == "\\":  # 转义对整体保留（% 不会被误吞）
                    buf.append(text[i:i + 2])
                    i += 2
                else:
                    if text[i] == "\n":
                        line_no += 1
                    buf.append(text[i])
                    i += 1
            i += 1
            src = lines[start_line - 1] if start_line - 1 < len(lines) else ""
            yield start_line, "".join(buf), src
        else:
            i += 1


def bad_specs(lit: str):
    """返回该字面量里不支持的说明符列表（`%%` 与结尾单 % 永不入列，见文件头判定说明）。"""
    cleaned = PERCENT.sub("", lit)
    cleaned = SUPPORTED.sub("", cleaned)
    out = []
    for m in ANY_SPEC.finditer(cleaned):
        out.append(m.group(0))
    # 结尾孤立的 '%'（格式串写漏了转换符）：lib_format 会原样输出它，不算不支持；
    # 这里只是不再当可疑项报出，故直接忽略。
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="列出 lib_format 不支持的格式说明符")
    ap.add_argument("dirs", nargs="*", default=None, help="待扫描目录（默认 bsp drv drvlib app lib）")
    ap.add_argument("--all", action="store_true", help="检查所有含 %% 的字面量，而不只是日志宏调用里的")
    ap.add_argument("--quiet", action="store_true", help="只输出不支持项，不输出统计")
    args = ap.parse_args()

    dirs = args.dirs or DEFAULT_DIRS
    hits = []
    n_lit = 0
    n_fmt = 0

    for d in dirs:
        root = d if os.path.isabs(d) else os.path.join(REPO_ROOT, d)
        for dirpath, _dirnames, filenames in os.walk(root):
            for name in sorted(filenames):
                if not name.endswith((".c", ".h")):
                    continue
                path = os.path.join(dirpath, name)
                for no, lit, src in iter_literals(path):
                    if "%" not in lit:
                        continue
                    n_lit += 1
                    if not args.all and not any(c in src for c in FMT_CALLS):
                        continue
                    n_fmt += 1
                    bad = bad_specs(lit)
                    if bad:
                        rel = os.path.relpath(path, REPO_ROOT)
                        hits.append((rel, no, sorted(set(bad)), lit))

    for rel, no, bad, lit in hits:
        print(f"{rel}:{no}: {','.join(bad)}  <- \"{lit}\"")
    if not args.quiet:
        print(f"\n扫描 {n_lit} 个含 % 的字面量，其中 {n_fmt} 个按格式串检查；不支持项 {len(hits)} 处。")
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())

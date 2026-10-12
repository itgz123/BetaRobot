#!/usr/bin/env python3
"""AV Rule 119 递归检查：用 -fcallgraph-info 生成调用图，检测环。

JSF AV C++ 的 AV Rule 119：
    "Functions shall not call themselves, either directly or indirectly
     (i.e. recursion shall not be allowed)."

GCC 没有能表达"禁递归"的开关，所以这条靠调用图判：环 == 递归。
本脚本从 compile_commands.json 重放编译（与 audit_jsf_warnings.py 同一套路），
给编译命令追加 -fcallgraph-info=su,da 拿到每个编译单元的 .ci 调用图，
合并成全程序图后用 Tarjan 求强连通分量，大小 > 1 或含自环的就是递归。

用法:
    cmake --preset default && cmake --build --preset Debug   # 先生成 compile_commands.json
    python3 scripts/jsf-av/check_recursion.py [--db build/compile_commands.json]
        [--scope first-party|cubemx|all] [--jobs N] [--json OUT] [--keep]

退出码:  0 = 无环   1 = 发现环   2 = 工具/环境错误

--------------------------------------------------------------------------
已知边界（重要，别把"0 环"当成"绝无递归"）
--------------------------------------------------------------------------
1. **函数指针的间接调用查不到目标**。GCC 在 .ci 里只给一个占位节点
   `__indirect_call`（"Indirect Call Placeholder"），不展开可能的被调函数。
   本仓大量使用函数表派发（`inst->vtable->xxx(inst)`、`Task_Init_Config_s.func`、
   daemon 的 offline 钩子…），所以**经函数表形成的环本工具看不见**。
   脚本会把这类调用点逐条列出来（见输出"间接调用点"），那是需要人工核的部分。
   也就是说 AV Rule 119 只做到"直接调用链全覆盖 + 间接调用点清单"，不是全自动。

2. **必须在 Debug（-Og）上分析**。Release 是 -O2/-O3，内联 + 尾调用优化会把
   真实存在的环从图里抹掉（实测 -O1 就能让 ping⇄pong 的双向边整个消失）。
   脚本只重放 Debug 那条编译命令；若发现优化级别高于 -Og 会直接报错退出。

3. **是保守近似，只会多报不会漏报**。图上的一条边只代表"存在调用点"，
   不代表运行时一定执行（-Og 不消除死分支）。对"函数不得调用自己"这种
   静态性质来说，这个方向是对的。

4. 第三方（cubemx / Middlewares）默认不重放（--scope first-party）。
   它们的节点会以"仅声明"（ellipse）出现，没有出边，因此不会参与成环。
"""

import argparse
import collections
import concurrent.futures
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile

THIRD_PARTY = ("cubemx", "Middlewares")
INDIRECT = "__indirect_call"

# node: { title: "X" label: "名字\n文件:行:列[\nN bytes (static)][\nM dynamic objects]" [shape : ellipse] }
NODE_RE = re.compile(r'node:\s*\{\s*title:\s*"([^"]*)"\s*label:\s*"([^"]*)"([^}]*)\}')
# edge: { sourcename: "A" targetname: "B" [label: "文件:行:列"] }
EDGE_RE = re.compile(r'edge:\s*\{\s*sourcename:\s*"([^"]*)"\s*targetname:\s*"([^"]*)"([^}]*)\}')
LOC_RE = re.compile(r'label:\s*"([^"]*)"')
BYTES_RE = re.compile(r"^(\d+)\s+bytes")
# -O2 / -O3 / -Os 会内联掉环；-Og / -O1 / -O0 保留。只认 Debug 档。
LOSSY_OPT_RE = re.compile(r"(?<![\w-])-O([23s]|fast)\b")
# Ninja Multi-Config 把配置写进 -DCMAKE_INTDIR=\"Debug\"（command 字段里带转义反斜杠，
# 不能按裸字符串 "CMAKE_INTDIR=\"Debug\"" 匹配）。退回看 -o 产物路径里的 /Debug/。
INTDIR_RE = re.compile(r'CMAKE_INTDIR=\\?"(\w+)\\?"')


def entry_config(cmd):
    m = INTDIR_RE.search(cmd)
    if m:
        return m.group(1)
    m = re.search(r"-o\s+\S*/(\w+)/", cmd)
    return m.group(1) if m else ""


def parse_ci(path):
    """解析一个 VCG .ci，返回 (定义, 声明的名字集合) 和 (边列表)。"""
    text = open(path, encoding="utf-8", errors="replace").read()
    defs, decls = {}, set()
    for title, label, tail in NODE_RE.findall(text):
        parts = label.split("\\n")  # .ci 里是字面反斜杠 + n
        loc = parts[1] if len(parts) > 1 else ""
        stack = None
        for p in parts[1:]:
            m = BYTES_RE.match(p.strip())
            if m:
                stack = int(m.group(1))
        if stack is None:
            decls.add(title)  # shape: ellipse —— 外部/内置，只有声明
        else:
            defs[title] = {"loc": loc, "stack": stack}
    edges = []
    for src, dst, tail in EDGE_RE.findall(text):
        m = LOC_RE.search(tail)
        edges.append((src, dst, m.group(1) if m else ""))
    return defs, decls, edges


def db_app_name(cmd, root):
    """从编译命令的 -I 里认出这份 compile_commands.json 是按哪个 app 配置生成的。"""
    m = re.search(r"-I" + re.escape(root) + r"/app/([A-Za-z0-9_]+)/", cmd)
    return m.group(1) if m else ""


def replay(entry, tmpdir, idx, root):
    """重放一条编译命令，额外产出 .ci。返回 (源文件, .ci 路径, 错误信息)。"""
    src = entry["file"]
    rel = src.replace(root + "/", "")
    args = entry.get("command") or " ".join(shlex.quote(a) for a in entry["arguments"])
    # 每个 TU 一个唯一 -o，否则同名文件（3 个 app 都有 app.c）会互相覆盖 .ci
    obj = os.path.join(tmpdir, "%04d-%s.o" % (idx, os.path.basename(src)))
    if " -o " in args:
        args = re.sub(r" -o \S+", " -o " + shlex.quote(obj), args, count=1)
    else:
        return rel, None, "编译命令里没有 -o，无法定位 .ci"
    # -w 静音告警；-Wno-error 抵消 -Werror（否则告警会中止编译、拿不到 .ci）
    args += " -w -Wno-error -fcallgraph-info=su,da"
    p = subprocess.run(args, shell=True, cwd=entry["directory"],
                       capture_output=True, text=True)
    ci = os.path.splitext(obj)[0] + ".ci"
    if p.returncode != 0:
        return rel, None, (p.stderr.strip().splitlines() or ["compiler failed"])[0]
    if not os.path.exists(ci):
        return rel, None, "编译器没有产出 .ci"
    return rel, ci, None


def tarjan_scc(nodes, succ):
    """迭代版 Tarjan，避免脚本自身递归爆栈（本脚本要查的正是递归）。"""
    index, low, on = {}, {}, {}
    stack, comps = [], []
    counter = 0
    for root in nodes:
        if root in index:
            continue
        work = [(root, 0)]
        while work:
            v, pi = work.pop()
            if pi == 0:  # 首次访问
                index[v] = low[v] = counter
                counter += 1
                stack.append(v)
                on[v] = True
            recurse = False
            nbrs = succ.get(v, ())
            for i in range(pi, len(nbrs)):
                w = nbrs[i]
                if w not in index:
                    work.append((v, i + 1))
                    work.append((w, 0))
                    recurse = True
                    break
                if on.get(w):
                    low[v] = min(low[v], index[w])
            if recurse:
                continue
            if low[v] == index[v]:  # v 是 SCC 根
                comp = []
                while True:
                    w = stack.pop()
                    on[w] = False
                    comp.append(w)
                    if w == v:
                        break
                comps.append(comp)
            if work:
                u = work[-1][0]
                low[u] = min(low[u], low[v])
    return comps


def extract_cycle(comp, succ):
    """在强连通分量里找一条简单回路，返回节点序列（首尾同一节点）。"""
    members = set(comp)
    start = comp[0]
    prev = {start: None}
    queue = collections.deque([start])
    while queue:
        v = queue.popleft()
        for w in succ.get(v, ()):
            if w not in members:
                continue
            if w == start:
                path, cur = [], v
                while cur is not None:
                    path.append(cur)
                    cur = prev[cur]
                path.reverse()
                return path + [start]
            if w not in prev:
                prev[w] = v
                queue.append(w)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="build/compile_commands.json")
    ap.add_argument("--scope", default="first-party", choices=["all", "first-party", "cubemx"])
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    ap.add_argument("--json", default=None)
    ap.add_argument("--keep", action="store_true", help="保留临时 .ci 目录")
    a = ap.parse_args()

    if not os.path.exists(a.db):
        print(f"找不到 {a.db}，先跑 cmake --preset default", file=sys.stderr)
        return 2
    root = os.path.abspath(os.path.dirname(a.db) + "/..")
    db = json.load(open(a.db))

    # compile_commands.json 里每个源文件有 Debug / Release 两条，只取 Debug。
    dbg = [e for e in db
           if entry_config(e.get("command") or " ".join(e.get("arguments", []))) == "Debug"]
    if not dbg:
        print("compile_commands.json 里没有 Debug 条目，先 cmake --preset default", file=sys.stderr)
        return 2
    tp = tuple(os.path.join(root, t) for t in THIRD_PARTY)
    if a.scope == "first-party":
        dbg = [e for e in dbg if not e["file"].startswith(tp)]
    elif a.scope == "cubemx":
        dbg = [e for e in dbg if e["file"].startswith(tp)]

    # compile_commands.json 是 CMake 配置时生成的，改 user_cfg.h 的 APP_NAME 后必须重跑
    # cmake --preset default 才会更新。不查这一下会拿旧 app 的图当成新 app 的结论
    # （本例就踩过：以为在查 gimbal，实际查的是 example）。
    cfg_path = os.path.join(root, "user_cfg.h")
    if os.path.exists(cfg_path):
        m = re.search(r"^\s*#define\s+APP_NAME\s+(\S+)",
                      open(cfg_path, encoding="utf-8").read(), re.M)
        want = m.group(1) if m else ""
        got = db_app_name(dbg[0].get("command") or " ".join(dbg[0].get("arguments", [])), root)
        if want and got and want != got:
            print(f"user_cfg.h 的 APP_NAME={want}，但 {a.db} 是按 app={got} 生成的（陈旧）。"
                  f"\n请先跑：cmake --preset default", file=sys.stderr)
            return 2

    for e in dbg:
        cmd = e.get("command") or " ".join(e.get("arguments", []))
        m = LOSSY_OPT_RE.search(cmd)
        if m:
            print(f"优化级别 {m.group(0)} 会内联掉调用边、让环从图上消失，"
                  f"本工具只能在 Debug(-Og) 上分析。请先 cmake --build --preset Debug。",
                  file=sys.stderr)
            return 2

    # 汇编（.s/.S）不参与 -fcallgraph-info，跳过而不是当"编译失败"报出来。
    asm = [e for e in dbg if e["file"].lower().endswith((".s", ".asm"))]
    dbg = [e for e in dbg if e not in asm]

    tmpdir = tempfile.mkdtemp(prefix="jsf-callgraph-")
    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        res = list(ex.map(lambda p: replay(p[1], tmpdir, p[0], root), enumerate(dbg)))

    defs, decls = {}, set()
    succ = collections.OrderedDict()
    edge_loc = {}
    indirect_sites = []
    empty, failed = [], []
    for rel, ci, err in res:
        if err:
            failed.append((rel, err))
            continue
        d, dc, edges = parse_ci(ci)
        if not d:
            empty.append(rel)  # 该 TU 在当前 app 配置下被 #if 整段裁掉
        defs.update({k: v for k, v in d.items() if k not in defs})
        decls |= dc
        for src, dst, loc in edges:
            if dst == INDIRECT:
                indirect_sites.append((rel, src, loc))
            succ.setdefault(src, [])
            if dst not in succ[src]:
                succ[src].append(dst)
            edge_loc.setdefault((src, dst), loc)

    nodes = set(succ)
    for v in succ.values():
        nodes.update(v)
    comps = tarjan_scc(sorted(nodes), succ)
    cycles = [c for c in comps if len(c) > 1 or (len(c) == 1 and c[0] in succ.get(c[0], ()))]

    print(f"# 范围 {a.scope}   重放 {len(dbg)} 个编译单元"
          f"（其中 {len(empty)} 个在本 app 配置下为空、{len(failed)} 个编译失败、"
          f"{len(asm)} 个汇编跳过）")
    only_decl = decls - set(defs)  # 同名可能在一个 TU 只有声明、在另一个有定义
    print(f"# 图：{len(nodes)} 个节点 / {sum(len(v) for v in succ.values())} 条边"
          f"（其中 {len(only_decl)} 个只有声明=外部或内置，无出边、不会成环）")

    if failed:
        print("\n## 编译失败（图不完整）")
        for rel, err in failed:
            print(f"  {rel}\n      {err}")

    if cycles:
        print(f"\n## 递归：{len(cycles)} 处（违反 AV Rule 119）")
        for i, comp in enumerate(cycles, 1):
            if len(comp) == 1:
                print(f"\n[{i}] 直接递归：{comp[0]}  ({defs.get(comp[0], {}).get('loc', '?')})")
                print(f"    调用点 {edge_loc.get((comp[0], comp[0]), '?')}")
            else:
                print(f"\n[{i}] 间接递归：{len(comp)} 个函数互相成环")
                path = extract_cycle(comp, succ) or comp + [comp[0]]
                for u, v in zip(path, path[1:]):
                    print(f"    {u}  ->  {v}")
                    print(f"        定义 {defs.get(u, {}).get('loc', '?')}"
                          f"   调用点 {edge_loc.get((u, v), '?')}")
    else:
        print("\n## 递归：未发现（直接调用链内无环）")

    if indirect_sites:
        by_file = collections.Counter(f for f, _, _ in indirect_sites)
        print(f"\n## 间接调用点：{len(indirect_sites)} 处 —— 本工具覆盖不到（函数指针目标未知）")
        for f, c in by_file.most_common():
            print(f"  {c:4d}  {f}")
        print("  这些调用点是否构成环，需人工核对所选函数是否可达回自身。")

    if a.json:
        with open(a.json, "w") as f:
            json.dump({"cycles": cycles, "indirect": indirect_sites,
                       "empty": empty, "failed": failed}, f, indent=2, ensure_ascii=False)
        print(f"\n# JSON 已写入 {a.json}")

    if a.keep:
        print(f"\n# .ci 保留在 {tmpdir}")
    else:
        subprocess.run(["rm", "-rf", tmpdir])

    return 1 if cycles else 0


if __name__ == "__main__":
    sys.exit(main())

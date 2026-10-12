#!/usr/bin/env python3
"""JSF AV 严格警告审计：用 compile_commands.json 重放编译，统计新增警告。

用法:
    cmake --preset default && cmake --build --preset Debug   # 先生成 compile_commands.json
    python3 scripts/jsf-av/audit_jsf_warnings.py [--db build/compile_commands.json]
        [--flags "-Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow"]
        [--scope first-party|cubemx|all] [--top N] [--jobs N] [--json OUT]

用途：评估「把 JSF AV 的严格警告集加进构建」的代价 —— 逐文件重放同一批编译命令，
只多带 --flags，按 flag / 目录 / 文件 / 消息归并计数。退出码恒为 0。
"""
import argparse, collections, concurrent.futures, json, os, re, shlex, subprocess, sys

DEFAULT_FLAGS = "-Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow"
THIRD_PARTY = ("cubemx", "Middlewares")


def load(db_path):
    with open(db_path) as f:
        return json.load(f)


def replay(entry, flags, root):
    args = entry.get("command") or " ".join(shlex.quote(a) for a in entry["arguments"])
    # 插在 -o 之前：保持语义不变，只是多带一组 flag
    if " -o " in args:
        args = args.replace(" -o ", " " + flags + " -o ", 1)
    else:
        args = args + " " + flags
    p = subprocess.run(args, shell=True, cwd=entry["directory"],
                       capture_output=True, text=True)
    wflags, msgs = [], []
    for line in p.stderr.splitlines():
        m = re.search(r"\[(-W[a-zA-Z0-9-]+)\]", line)
        if m and "warning:" in line or "警告" in line:
            wflags.append(m.group(1) if m else "?")
            msgs.append(line)
    hard = [l for l in p.stderr.splitlines() if "error:" in l]
    return entry["file"].replace(root + "/", ""), wflags, msgs, hard


def norm(msg):
    m = re.sub(r"^.*?:\d+:\d+: ", "", msg)
    m = re.sub(r"\[-W[a-zA-Z0-9-]+\]", "", m).strip()
    m = re.sub(r"'[^']*'", "'X'", m)
    m = re.sub(r"\{aka '[^']*'\}", "{aka X}", m)
    m = re.sub(r"[A-Za-z_][A-Za-z0-9_]*\.(c|cc|cpp|h)", "F.\\1", m)
    return m


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default="build/compile_commands.json")
    ap.add_argument("--flags", default=DEFAULT_FLAGS)
    ap.add_argument("--scope", default="all", choices=["all", "first-party", "cubemx"])
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    ap.add_argument("--json", default=None)
    a = ap.parse_args()

    root = os.path.abspath(os.path.dirname(a.db) + "/..")
    db = load(a.db)
    # Ninja Multi-Config 生成的 compile_commands.json 里，每个源文件有 Debug / Release
    # 两条记录，诊断内容一致；按文件去重，否则所有计数恰好翻倍。
    _seen, _uniq = set(), []
    for _e in db:
        if _e["file"] not in _seen:
            _seen.add(_e["file"])
            _uniq.append(_e)
    db = _uniq
    if a.scope == "first-party":
        db = [e for e in db if not e["file"].startswith(
            tuple(os.path.join(root, t) for t in THIRD_PARTY))]
    elif a.scope == "cubemx":
        db = [e for e in db if e["file"].startswith(
            tuple(os.path.join(root, t) for t in THIRD_PARTY))]

    with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
        res = list(ex.map(lambda e: replay(e, a.flags, root), db))

    by_flag = collections.Counter()
    by_dir = collections.Counter()
    by_file = collections.Counter()
    by_msg = collections.Counter()
    msg_eg = {}
    first_party = collections.Counter()
    hard = []
    for f, wf, msgs, errs in res:
        top = f.split("/")[0]
        is_tp = top in THIRD_PARTY
        for fl, m in zip(wf + ["?"] * (len(msgs) - len(wf)), msgs):
            by_flag[fl] += 1
            by_dir[top] += 1
            by_file[f] += 1
            k = norm(m)
            by_msg[k] += 1
            msg_eg.setdefault(k, f"{f}  |  {m.strip()[:150]}")
        if errs:
            hard.append((f, errs[0].strip()[:200]))
        if not is_tp:
            first_party["total"] += len(msgs)

    print(f"# flags : {a.flags}")
    print(f"# scope : {a.scope}   文件数 {len(db)}   错误(编译失败) {len(hard)}")
    print(f"# 警告总数 {sum(by_flag.values())}   其中一方代码 {first_party['total']}")
    print("\n## 按警告开关")
    for k, v in by_flag.most_common():
        print(f"  {v:6d}  {k}")
    print("\n## 按目录")
    for k, v in by_dir.most_common():
        mark = "  (第三方)" if k in THIRD_PARTY else ""
        print(f"  {v:6d}  {k}{mark}")
    print(f"\n## 警告最多的文件 (top {a.top})")
    for k, v in by_file.most_common(a.top):
        print(f"  {v:5d}  {k}")
    print("\n## 按消息归类（决定要怎么改）")
    for k, v in by_msg.most_common():
        print(f"  {v:6d}  {k}")
        print(f"           e.g. {msg_eg[k]}")
    if hard:
        print("\n## 编译失败")
        for f, m in hard:
            print(f"  {f}\n      {m}")
    if a.json:
        with open(a.json, "w") as f:
            json.dump({"by_flag": by_flag, "by_dir": by_dir, "by_file": by_file,
                       "by_msg": by_msg, "hard": hard}, f, indent=2)
        print(f"\n# JSON 已写入 {a.json}")


if __name__ == "__main__":
    main()

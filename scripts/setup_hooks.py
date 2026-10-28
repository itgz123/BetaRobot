#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
为 BetaRobot 主仓库启用（或取消）版本钩子。

钩子文件随仓库放在 .githooks/ 下，本脚本只设置本仓库的本地配置：
    git config --local core.hooksPath .githooks

用法：
    python3 scripts/setup_hooks.py             # 启用
    python3 scripts/setup_hooks.py --uninstall # 取消（不删除 .githooks/）
    python3 scripts/setup_hooks.py --force     # 覆盖已存在的其他 core.hooksPath

启用后，每次在主仓库提交时，.githooks/pre-commit 会调用 pre_commit.py，
把根目录 BetaRobot_version.h 更新为提交时刻（V_年_月_日_时_分_秒）并暂存。

只依赖 Python 标准库，兼容老版本 Python 3（不使用 f-string / 类型标注）。
"""

import argparse
import os
import subprocess
import sys

PY3 = sys.version_info[0] >= 3

HOOKS_PATH = ".githooks"
HOOK_FILES = ("pre-commit", "pre_commit.py")


def say(msg):
    """输出到 stdout，兼容 Python 2/3 与 Windows 控制台编码。"""
    text = msg + u"\n"
    if PY3:
        sys.stdout.write(text)
    else:
        enc = sys.stdout.encoding or "utf-8"
        sys.stdout.write(text.encode(enc, "replace"))


def say_err(msg):
    """输出到 stderr，兼容 Python 2/3 与 Windows 控制台编码。"""
    text = msg + u"\n"
    if PY3:
        sys.stderr.write(text)
    else:
        enc = sys.stderr.encoding or "utf-8"
        sys.stderr.write(text.encode(enc, "replace"))


def decode(data):
    if isinstance(data, bytes):
        return data.decode("utf-8", "replace")
    return data


def run_git(args, cwd):
    """执行 git 命令，返回 (returncode, stdout, stderr)，均为文本。"""
    proc = subprocess.Popen(
        ["git"] + args,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    out, err = proc.communicate()
    return proc.returncode, decode(out), decode(err)


def norm(path):
    return os.path.normcase(os.path.realpath(os.path.abspath(path)))


def git_root():
    rc, out, err = run_git(["rev-parse", "--show-toplevel"], os.getcwd())
    if rc != 0:
        say_err("[setup_hooks] 当前目录不在 git 仓库内：%s" % err.strip())
        return None
    return out.strip()


def get_hooks_path(root):
    """返回 core.hooksPath 的当前值；未设置返回 None。"""
    rc, out, err = run_git(["config", "--local", "--get", "core.hooksPath"], root)
    if rc != 0:
        return None
    return out.strip()


def chmod_exec(path):
    """POSIX 上补可执行位；Windows 上 Git 不依赖执行位，忽略失败。"""
    if os.name == "nt":
        return
    try:
        os.chmod(path, 0o755)
    except OSError as exc:
        say_err("[setup_hooks] 警告：无法设置执行位 %s：%s" % (path, exc))


def install(root, force):
    hooks_dir = os.path.join(root, HOOKS_PATH)
    for name in HOOK_FILES:
        path = os.path.join(hooks_dir, name)
        if not os.path.isfile(path):
            say_err("[setup_hooks] 缺少钩子文件：%s" % path)
            say_err("[setup_hooks] 请确认已拉取完整仓库（.githooks/ 随主仓库跟踪）")
            return 1

    current = get_hooks_path(root)
    if current is not None and current != HOOKS_PATH:
        if not force:
            say_err("[setup_hooks] core.hooksPath 已被设置为：%s" % current)
            say_err("[setup_hooks] 为避免覆盖其他工具的钩子，已中止；确认要覆盖请加 --force")
            return 1
        say("[setup_hooks] 覆盖已有的 core.hooksPath：%s" % current)

    rc, out, err = run_git(["config", "--local", "core.hooksPath", HOOKS_PATH], root)
    if rc != 0:
        say_err("[setup_hooks] 设置 core.hooksPath 失败：%s" % err.strip())
        return 1

    for name in HOOK_FILES:
        path = os.path.join(hooks_dir, name)
        if name == "pre-commit":
            chmod_exec(path)

    say("[setup_hooks] 已启用：core.hooksPath = %s" % HOOKS_PATH)
    say("[setup_hooks] 之后每次在主仓库提交，都会自动更新并暂存 %s"
        % os.path.join("BetaRobot_version.h"))
    say("[setup_hooks] 取消启用：python3 scripts/setup_hooks.py --uninstall")
    return 0


def uninstall(root):
    current = get_hooks_path(root)
    if current is None:
        say("[setup_hooks] 当前未设置 core.hooksPath，无需取消")
        return 0

    if current != HOOKS_PATH:
        say("[setup_hooks] core.hooksPath 当前为 %s，不是本脚本设置的 %s，未做修改"
            % (current, HOOKS_PATH))
        return 0

    rc, out, err = run_git(["config", "--local", "--unset", "core.hooksPath"], root)
    if rc != 0:
        say_err("[setup_hooks] 取消 core.hooksPath 失败：%s" % err.strip())
        return 1

    say("[setup_hooks] 已取消 core.hooksPath（.githooks/ 文件保留）")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="为 BetaRobot 主仓库启用/取消 .githooks 版本钩子",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--uninstall", action="store_true",
                        help="取消 core.hooksPath（仅当它指向 .githooks）")
    parser.add_argument("--force", action="store_true",
                        help="覆盖已存在的其他 core.hooksPath")
    args = parser.parse_args()

    root = git_root()
    if root is None:
        return 1

    # scripts/setup_hooks.py -> 仓库根
    script_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if norm(root) != norm(script_root):
        say_err("[setup_hooks] 当前仓库根与脚本所在仓库不一致：")
        say_err("[setup_hooks]   git 根目录：%s" % root)
        say_err("[setup_hooks]   脚本目录：%s" % script_root)
        say_err("[setup_hooks] 请在 BetaRobot 主仓库中运行（app/* 是独立仓库，不适用本钩子）")
        return 1

    if args.uninstall:
        return uninstall(root)
    return install(root, args.force)


if __name__ == "__main__":
    sys.exit(main())

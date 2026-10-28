#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
BetaRobot 主仓库 pre-commit 业务逻辑。

每次提交时把根目录 BetaRobot_version.h 的版本宏更新为当前本地时间，
格式 V_年_月_日_时_分_秒，并把该文件加入本次提交的暂存区。

启用方式见 scripts/setup_hooks.py（设置 core.hooksPath=.githooks）。
本脚本也可以手动运行：python3 .githooks/pre_commit.py

只依赖 Python 标准库，兼容老版本 Python 3（不使用 f-string / 类型标注）。
退出码：0 成功；非 0 失败（Git 会因此中止本次提交）。
"""

import io
import os
import sys
import time
import subprocess

PY3 = sys.version_info[0] >= 3

VERSION_FILE = "BetaRobot_version.h"

# 每次生成都保留这段固定内容：app 定义 APP_LOCKED_BETAROBOT_VERSION 后
# 与当前版本不一致会在预处理阶段直接 #error。
HEADER_TEMPLATE = u"""/**
 * @file BetaRobot_version.h
 * @brief BetaRobot 主仓库版本号。由 .githooks/pre_commit.py 自动生成，请勿手动修改。
 * @note app 必须锁定内核版本（写在 app 的 app_cfg.h 里，且在 include 之前）：
 *         #define APP_LOCKED_BETAROBOT_VERSION %(ver_value)s
 *         #include "BetaRobot_version.h"
 *       未定义该宏或版本不匹配都会在下面的检查里直接 #error。
 */
#ifndef BETAROBOT_VERSION_H
#define BETAROBOT_VERSION_H

/* 版本号格式：V_年_月_日_时_分_秒（本地时间） */
#define %(ver_name)s   %(ver_value)s
#define BETAROBOT_VERSION        %(ver_name)s
#define BETAROBOT_VERSION_STRING "%(ver_name)s"

/* app 必须锁定版本：未定义或版本不一致都在这里 #error */
#ifndef APP_LOCKED_BETAROBOT_VERSION
#error "app 未锁定 BetaRobot 版本：请在 app_cfg.h 中定义 APP_LOCKED_BETAROBOT_VERSION"
#elif BETAROBOT_VERSION != APP_LOCKED_BETAROBOT_VERSION
#error "BetaRobot 版本不匹配：app 的 APP_LOCKED_BETAROBOT_VERSION 与主仓库 BetaRobot_version.h 不一致"
#endif

#endif /* BETAROBOT_VERSION_H */
"""


def say(msg):
    """向 stdout 输出，兼容 Python 2/3 与 Windows 控制台编码。"""
    text = msg + u"\n"
    if PY3:
        sys.stdout.write(text)
    else:
        enc = sys.stdout.encoding or "utf-8"
        sys.stdout.write(text.encode(enc, "replace"))


def say_err(msg):
    """向 stderr 输出，兼容 Python 2/3 与 Windows 控制台编码。"""
    text = msg + u"\n"
    if PY3:
        sys.stderr.write(text)
    else:
        enc = sys.stderr.encoding or "utf-8"
        sys.stderr.write(text.encode(enc, "replace"))


def decode(data):
    """subprocess 输出统一转成文本。"""
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


def repo_root():
    """取当前工作区根目录；失败返回 None。"""
    rc, out, err = run_git(["rev-parse", "--show-toplevel"], os.getcwd())
    if rc != 0:
        say_err("[pre-commit] 无法定位 git 仓库根目录：%s" % err.strip())
        return None
    return out.strip()


def write_text(path, text):
    """以 UTF-8 + LF 原子写入；内容相同则跳过。"""
    old = None
    if os.path.isfile(path):
        try:
            f = io.open(path, "r", encoding="utf-8")
            try:
                old = f.read()
            finally:
                f.close()
        except (IOError, OSError):
            old = None

    if old == text:
        return False

    tmp = path + ".tmp"
    f = io.open(tmp, "w", encoding="utf-8", newline="\n")
    try:
        f.write(text)
    finally:
        f.close()

    replace = getattr(os, "replace", None)
    if replace is not None:
        replace(tmp, path)
    else:
        # Python 2.7：Windows 上 rename 不能覆盖已有文件
        if os.path.exists(path):
            os.remove(path)
        os.rename(tmp, path)
    return True


def main():
    root = repo_root()
    if root is None:
        return 1

    now = time.localtime()
    ver_name = time.strftime("V_%Y_%m_%d_%H_%M_%S", now)
    ver_value = time.strftime("%Y%m%d%H%M%S", now)

    text = HEADER_TEMPLATE % {"ver_name": ver_name, "ver_value": ver_value}
    path = os.path.join(root, VERSION_FILE)

    changed = write_text(path, text)

    rc, out, err = run_git(["add", "--", VERSION_FILE], root)
    if rc != 0:
        say_err("[pre-commit] git add %s 失败：%s" % (VERSION_FILE, err.strip()))
        return 1

    if changed:
        say("[pre-commit] %s -> %s" % (VERSION_FILE, ver_name))
    return 0


if __name__ == "__main__":
    sys.exit(main())

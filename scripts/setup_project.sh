#!/usr/bin/env bash
#
# 新工作区引导：拉主仓库 → 拉 app 模板仓库 → 生成 user_cfg.h
#
# 背景：编译系统按 user_cfg.h 里的 APP_NAME 编译 app/<APP_NAME>/，
#       而 app/ 下的应用各自是独立 git 仓库（主仓库不追踪、不绑定，见 .gitignore）。
#       主仓库 clone 后 app/ 为空，本脚本把"从零搭起一个工作区"这几步串起来。
#
# 用法：
#   bash scripts/setup_project.sh                       # 交互输入
#   bash scripts/setup_project.sh <主仓库目录> <app 名>   # 也可直接带参数
#
# 前置条件：已配置对 GitHub 的 SSH 访问（仓库为私有）。
set -u

MAIN_REPO_URL="git@github.com:itgz123/BetaRobot.git"            # 主仓库
APP_REPO_URL="git@github.com:itgz123/BetaRobot-App-Example.git" # app 模板仓库（新 app 从它开始改）

DEFAULT_MAIN_DIR="BetaRobot"
DEFAULT_APP_NAME="example"

# ---- 输入：主仓库文件夹名称 / app 名称 ----
MAIN_DIR="${1-}"
APP_NAME="${2-}"

if [ -z "$MAIN_DIR" ]; then
  read -r -p "主仓库文件夹名称 [$DEFAULT_MAIN_DIR]: " MAIN_DIR
  MAIN_DIR="${MAIN_DIR:-$DEFAULT_MAIN_DIR}"
fi

if [ -z "$APP_NAME" ]; then
  read -r -p "app 名称（clone 到 $MAIN_DIR/app/<名称>，并写入 user_cfg.h）[$DEFAULT_APP_NAME]: " APP_NAME
  APP_NAME="${APP_NAME:-$DEFAULT_APP_NAME}"
fi

# app 目录名即 CMake target 名，先挡掉明显不合法的
case "$APP_NAME" in
  all | clean | install | test | help | package)
    echo "app 名称 '$APP_NAME' 与 CMake 保留 target 名冲突，请换一个" >&2
    exit 1
    ;;
  *[!A-Za-z0-9_]*)
    echo "app 名称 '$APP_NAME' 含非法字符，只允许字母/数字/下划线" >&2
    exit 1
    ;;
esac

APP_DIR="$MAIN_DIR/app/$APP_NAME"

# ---- 1. 主仓库 ----
if [ -d "$MAIN_DIR/.git" ]; then
  echo "==> $MAIN_DIR：主仓库已存在，跳过"
else
  echo "==> 拉取主仓库 → $MAIN_DIR"
  git clone "$MAIN_REPO_URL" "$MAIN_DIR" || exit 1
fi

# ---- 2. app 仓库（模板）----
if [ -d "$APP_DIR/.git" ]; then
  echo "==> $APP_DIR：app 仓库已存在，跳过"
else
  echo "==> 拉取 app 模板 → $APP_DIR"
  git clone "$APP_REPO_URL" "$APP_DIR" || exit 1
fi

# ---- 3. user_cfg.h：把编译目标指向刚拉下来的 app ----
if [ -f "$MAIN_DIR/user_cfg.h" ]; then
  echo "==> $MAIN_DIR/user_cfg.h：已存在，跳过（切换 app 改这里的 APP_NAME）"
elif [ -f "$MAIN_DIR/user_cfg.h.example" ]; then
  echo "==> 生成 $MAIN_DIR/user_cfg.h，APP_NAME = $APP_NAME"
  sed "s|^#define APP_NAME .*|#define APP_NAME $APP_NAME|" \
    "$MAIN_DIR/user_cfg.h.example" > "$MAIN_DIR/user_cfg.h"
else
  echo "==> 生成 $MAIN_DIR/user_cfg.h，APP_NAME = $APP_NAME"
  printf '#ifndef __USER_CFG_H\n#define __USER_CFG_H\n\n#define APP_NAME %s\n\n#endif // __USER_CFG_H\n' \
    "$APP_NAME" > "$MAIN_DIR/user_cfg.h"
fi

echo "完成。接下来："
echo "  cd $MAIN_DIR"
echo "  cmake --preset default && cmake --build --preset Debug"

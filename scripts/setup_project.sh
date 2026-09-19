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
# 克隆通道：两个仓库都是公开的，按下面顺序尝试，成功即停：
#   1. SSH      git@github.com:...                （需要已注册 SSH 公钥）
#   2. SSH/443  ssh://git@ssh.github.com:443/...  （22 端口被封时的绕法，同样要公钥）
#   3. HTTPS    https://github.com/...            （免密钥）
#   4. 镜像      $BETAROBOT_GIT_MIRROR 前缀 + HTTPS 地址
# github.com 完全不通时，用镜像前缀再跑一次，例如：
#   BETAROBOT_GIT_MIRROR=https://ghfast.top/ bash scripts/setup_project.sh
set -u

OWNER="itgz123"
MAIN_REPO_SLUG="$OWNER/BetaRobot"            # 主仓库
APP_REPO_SLUG="$OWNER/BetaRobot-App-Example" # app 模板仓库（新 app 从它开始改）

DEFAULT_MAIN_DIR="BetaRobot"
DEFAULT_APP_NAME="example"

# 每条通道都得快速失败：卡在口令/指纹的交互确认上，后面的通道就没机会试了
export GIT_SSH_COMMAND="${GIT_SSH_COMMAND:-ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10}"
export GIT_TERMINAL_PROMPT=0

# 镜像前缀（结尾斜杠统一成一个）。不设这个变量时第 4 条通道自动跳过。
MIRROR="${BETAROBOT_GIT_MIRROR-}"
if [ -n "$MIRROR" ]; then
  MIRROR="${MIRROR%/}/"
fi

# clone_repo <owner/name> <目标目录>：按通道顺序尝试，全部失败返回 1
clone_repo() {
  slug="$1"
  dest="$2"

  for url in \
    "git@github.com:$slug.git" \
    "ssh://git@ssh.github.com:443/$slug.git" \
    "https://github.com/$slug.git"; do
    echo "    尝试 $url"
    if git clone "$url" "$dest"; then
      return 0
    fi
    # 这一条通道留下的半成品会挡住下一条（git 拒绝 clone 进非空目录），清掉它。
    # 只删带 .git 的目录：能确定是本函数刚建的，不是用户自己的东西。
    if [ -d "$dest/.git" ]; then
      rm -rf "$dest"
    fi
  done

  if [ -n "$MIRROR" ]; then
    url="${MIRROR}https://github.com/$slug.git"
    echo "    尝试 $url"
    if git clone "$url" "$dest"; then
      return 0
    fi
    if [ -d "$dest/.git" ]; then
      rm -rf "$dest"
    fi
  fi

  echo "    所有通道都失败。若 github.com 不可达，试试镜像前缀：" >&2
  echo "      BETAROBOT_GIT_MIRROR=<镜像地址>/ bash $0" >&2
  return 1
}

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
  clone_repo "$MAIN_REPO_SLUG" "$MAIN_DIR" || exit 1
fi

# ---- 2. app 仓库（模板）----
if [ -d "$APP_DIR/.git" ]; then
  echo "==> $APP_DIR：app 仓库已存在，跳过"
else
  echo "==> 拉取 app 模板 → $APP_DIR"
  clone_repo "$APP_REPO_SLUG" "$APP_DIR" || exit 1
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

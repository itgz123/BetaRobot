#!/usr/bin/env bash
#
# 初始化缺失的独立 app 仓库
#
# 背景：app/ 下的应用各自是独立的 git 仓库，主仓库不追踪、不绑定（见 .gitignore）。
#       主仓库 clone 后 app/ 为空，运行本脚本即可拉取全部 app 仓库。
#
# 用法：
#   bash scripts/setup_apps.sh
#
# 前置条件：已配置对 GitHub 的 SSH 访问（app 仓库为私有）。
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# 需要拉取的 app：<目录名> <GitHub 仓库名>
APPS=(
  "half_rudder_gimbal BetaRobot-App-Half-Rudder-Gimbal"
  "half_rudder_chassis BetaRobot-App-Half-Rudder-Chassis"
)

fail=0
for entry in "${APPS[@]}"; do
  name="${entry%% *}"
  repo="${entry#* }"
  dest="$ROOT/app/$name"
  if [ -d "$dest/.git" ]; then
    echo "==> $name 已存在，跳过"
  else
    echo "==> 拉取 $name"
    if git clone "git@github.com:itgz123/$repo.git" "$dest"; then
      echo "    ok"
    else
      echo "    失败！请检查 SSH 访问后重试"
      fail=1
    fi
  fi
done

if [ "$fail" -ne 0 ]; then
  exit 1
fi
echo "全部 app 仓库就绪。"

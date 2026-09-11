#!/usr/bin/env bash
#
# check-docs.sh — BetaRobot 文档站点本地构建校验
#
# 功能：
#   1. 检查依赖（node、npm）
#   2. 缺少 node_modules 时安装依赖
#   3. 执行 npm run docs:build，等价于 CI 的构建步骤
#
# 用途：推送 docs/ 改动前本地跑一遍，提前发现构建失败
#
set -euo pipefail

# 切换到脚本所在目录
cd "$(dirname "$0")" || {
    echo "ERROR: Cannot find docs folder" >&2
    exit 1
}

echo "Checking Node.js..."

if ! command -v node &>/dev/null; then
    echo "ERROR: Node.js not found. Install Node.js first." >&2
    exit 1
fi

if ! command -v npm &>/dev/null; then
    echo "ERROR: npm not found. Install Node.js first." >&2
    exit 1
fi

# 检查 node_modules（.gitignore 中已忽略该目录）
if [ ! -d "node_modules" ]; then
    echo "(First run - installing dependencies...)"
    npm install
    echo "Dependencies installed."
fi

echo ""
echo "Building BetaRobot docs..."
echo ""

npm run docs:build

echo ""
echo "Docs build OK."

#!/usr/bin/env bash
#
# start-docs.sh — BetaRobot 文档站点启动脚本
#
# 功能：
#   1. 检查依赖（node、npm）
#   2. 根据 .gitignore 规则安装被忽略的依赖（node_modules）
#   3. 启动 VitePress 开发服务器
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
echo "Starting BetaRobot docs..."
echo ""

exec npm run docs:dev

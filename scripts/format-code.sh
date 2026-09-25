#!/usr/bin/env bash
#
# format-code.sh — 用 clang-format 就地格式化主仓库全部 .c/.h
#
# 文件表取自 git ls-files，因此：
#   - app/ 下的应用各自是独立 git 仓库（主仓库下钻不进去），天然不在列表里；
#   - 被 .gitignore 忽略的文件天然不在列表里；
#   - 再显式排除 cubemx/（CubeMX 生成的 HAL 代码）。
# 风格见仓库根目录的 .clang-format（与 VS Code C/C++ 插件共用同一份）。
#
# 用法：bash scripts/format-code.sh
#
set -euo pipefail

cd "$(dirname "$0")/.." || {
    echo "ERROR: Cannot find the repository root" >&2
    exit 1
}

if ! command -v clang-format &>/dev/null; then
    echo "ERROR: clang-format not found. Install LLVM/clang-format first." >&2
    exit 1
fi

# -z / -0 让含空格、含非 ASCII 的路径也能正确分隔；xargs 在任一文件失败时以 123 退出
git ls-files -z '*.c' '*.h' ':(exclude)cubemx/*' | xargs -0 clang-format -i --

echo "format-code: done"

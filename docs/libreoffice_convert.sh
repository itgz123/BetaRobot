#!/usr/bin/env bash
# ============================================================
# PDF -> DOCX 批量转换 + 解压
# 支持 Windows(Git Bash) / Linux / macOS
#   对目录下每个 PDF：
#     0. 文件名缺 .pdf 后缀的，按文件头(%PDF-)识别并补回后缀
#     1. LibreOffice 转 docx           -> xxx.pdf.docx
#     2. 命令行解压 docx(zip) 为文件夹   -> xxx.pdf_extracted/
#   每个 pdf 处理完保留三样：
#     源 pdf + xxx.pdf.docx + xxx.pdf_extracted/ 文件夹
#
# 用法：
#   bash libreoffice_convert.sh             # 扫描脚本所在目录
#   bash libreoffice_convert.sh /path/dir   # 扫描指定目录
# ============================================================
set -uo pipefail

# 默认扫描脚本所在目录，可用第一个参数覆盖
DOCS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ $# -ge 1 ]; then
    DOCS_DIR="$1"
fi
if [ ! -d "$DOCS_DIR" ]; then
    echo "[FATAL] 目录不存在: $DOCS_DIR" >&2
    exit 1
fi

# --- 定位 LibreOffice 命令行工具 ---
detect_soffice() {
    local cand
    # 1) PATH 中的命令名
    for cand in soffice soffice.com libreoffice; do
        command -v "$cand" >/dev/null 2>&1 && { echo "$cand"; return 0; }
    done
    # 2) 常见安装路径（glob 不匹配时会保留原样，-x 判断失败即可）
    for cand in \
        /usr/bin/libreoffice* \
        /usr/lib/libreoffice/program/soffice \
        /usr/lib64/libreoffice/program/soffice \
        /opt/libreoffice*/program/soffice \
        /Applications/LibreOffice.app/Contents/MacOS/soffice \
        "/c/Program Files/LibreOffice/program/soffice.com" \
        "/c/Program Files (x86)/LibreOffice/program/soffice.com" \
        ; do
        [ -x "$cand" ] && { echo "$cand"; return 0; }
    done
    return 1
}

if ! SOFFICE="$(detect_soffice)"; then
    echo "[FATAL] 找不到 LibreOffice，请先安装或把它加入 PATH" >&2
    exit 1
fi

# --- 解压工具（优先 python，备选 unzip） ---
if command -v python3 >/dev/null 2>&1; then
    UNZIP_CMD=(python3 -m zipfile -e)
elif command -v python >/dev/null 2>&1; then
    UNZIP_CMD=(python -m zipfile -e)
elif command -v unzip >/dev/null 2>&1; then
    UNZIP_CMD=(unzip -qo)
else
    echo "[FATAL] 找不到解压工具（python3 / python / unzip 都没有）" >&2
    exit 1
fi

# --- timeout 可选（macOS 默认没有） ---
TIMEOUT_BIN=""
command -v timeout >/dev/null 2>&1 && TIMEOUT_BIN="timeout"

# 转换（单个文件最长 20 分钟，防止大文件卡死）
run_convert() {
    if [ -n "$TIMEOUT_BIN" ]; then
        "$TIMEOUT_BIN" 1200 "$SOFFICE" "$@"
    else
        "$SOFFICE" "$@"
    fi
}

echo "扫描目录: $DOCS_DIR"
echo "LibreOffice: $SOFFICE"
echo

count=0
skipped=0
failed=0
renamed=0

# 不递归进已生成的 _extracted 目录和 .git
while IFS= read -r -d '' f; do
    name="$(basename "$f")"

    # --- 只处理 PDF：先看后缀，再看文件头 ---
    is_pdf=0
    case "$name" in
        *.pdf|*.PDF) is_pdf=1 ;;
    esac
    if [ "$is_pdf" -eq 0 ]; then
        if [ "$(head -c 5 -- "$f" 2>/dev/null)" = "%PDF-" ]; then
            is_pdf=1
            # 补回 .pdf 后缀，后续命名/复用都靠它
            if [ -e "${f}.pdf" ]; then
                echo "[WARN] $name 是 PDF，但 ${name}.pdf 已存在，跳过重命名"
            else
                mv -- "$f" "${f}.pdf"
                f="${f}.pdf"
                name="${name}.pdf"
                renamed=$((renamed+1))
                echo "[RENAME] 补回 .pdf 后缀 -> $name"
            fi
        fi
    fi
    [ "$is_pdf" -eq 1 ] || continue

    dir="${f}_extracted"             # xxx.pdf_extracted/
    cand_full="${f}.docx"            # xxx.pdf.docx
    cand_base="${f%.*}.docx"         # xxx.docx（LibreOffice 有时去掉 .pdf）

    # 已处理过则跳过（并顺手统一命名）
    if [ -d "$dir" ] && { [ -f "$cand_full" ] || [ -f "$cand_base" ]; }; then
        if [ -f "$cand_base" ] && [ ! -f "$cand_full" ]; then
            mv -- "$cand_base" "$cand_full"
        fi
        echo "[SKIP] 已处理: $name"
        skipped=$((skipped+1))
        continue
    fi

    echo "[CONVERT] $name"
    rm -f -- "$cand_full" "$cand_base"

    log="$(mktemp)"
    if ! run_convert --headless --norestore --invisible \
         --infilter="writer_pdf_import" \
         --convert-to "docx:Office Open XML Text" \
         --outdir "$(dirname "$f")" "$f" >"$log" 2>&1; then
        echo "[ERROR] 转换失败: $name"
        tail -n 5 "$log" | sed 's/^/        /'
        rm -f -- "$log"
        failed=$((failed+1))
        continue
    fi
    rm -f -- "$log"

    # 统一 docx 命名
    if [ -f "$cand_base" ] && [ ! -f "$cand_full" ]; then
        mv -- "$cand_base" "$cand_full"
    fi
    if [ ! -f "$cand_full" ]; then
        echo "[ERROR] 未生成 docx: $name"
        failed=$((failed+1))
        continue
    fi

    echo "[EXTRACT] ${name}.docx -> ${name}_extracted/"
    rm -rf -- "$dir"
    if ! "${UNZIP_CMD[@]}" "$cand_full" "$dir" >/dev/null 2>&1; then
        echo "[ERROR] 解压失败: $cand_full"
        failed=$((failed+1))
        continue
    fi
    count=$((count+1))
done < <(find "$DOCS_DIR" \
            \( -name '*_extracted' -o -name '.git' \) -prune -o \
            -type f -print0)

echo
echo "================================="
echo "完成: 处理 $count 个, 跳过 $skipped 个, 失败 $failed 个, 补后缀 $renamed 个"
[ "$failed" -eq 0 ]

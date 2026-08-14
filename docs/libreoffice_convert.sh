#!/usr/bin/env bash
# ============================================================
# PDF -> DOCX 批量转换 + 解压
#   对目录下每个 PDF：
#     1. LibreOffice 转 docx           -> xxx.pdf.docx
#     2. 命令行解压 docx(zip) 为文件夹   -> xxx.pdf_extracted/
#   每个 pdf 处理完保留三样：
#     源 pdf + xxx.pdf.docx + xxx.pdf_extracted/ 文件夹
#
# 用法：
#   bash libreoffice_convert.sh             # 扫描脚本所在目录
#   bash libreoffice_convert.sh /path/dir   # 扫描指定目录
# ============================================================
set -u

# 默认扫描脚本所在目录，可用第一个参数覆盖
DOCS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ $# -ge 1 ]; then
    DOCS_DIR="$1"
fi

# --- 定位 LibreOffice 命令行工具 ---
if command -v soffice.com >/dev/null 2>&1; then
    SOFFICE="soffice.com"
else
    SOFFICE="/c/Program Files/LibreOffice/program/soffice.com"
fi

# --- 解压工具（优先 python，备选 unzip） ---
if command -v python >/dev/null 2>&1; then
    UNZIP_CMD=(python -m zipfile -e)
else
    UNZIP_CMD=(unzip -qo)
fi

echo "扫描目录: $DOCS_DIR"
echo "LibreOffice: $SOFFICE"
echo

count=0
skipped=0
failed=0

while IFS= read -r -d '' f; do
    name="$(basename "$f")"

    # 只处理 PDF
    case "$name" in
        *.pdf|*.PDF) ;;
        *) continue ;;
    esac

    dir="${f}_extracted"             # xxx.pdf_extracted/

    # LibreOffice 输出名有两种可能（有时保留 .pdf、有时去掉），
    # 因此用两个候选名，最终统一重命名为 xxx.pdf.docx
    cand_full="${f}.docx"            # xxx.pdf.docx
    cand_base="${f%.*}.docx"         # xxx.docx

    # 已处理过则跳过（并顺手统一命名）
    if [ -d "$dir" ] && { [ -f "$cand_full" ] || [ -f "$cand_base" ]; }; then
        if [ -f "$cand_base" ] && [ ! -f "$cand_full" ]; then
            mv "$cand_base" "$cand_full"
        fi
        echo "[SKIP] 已处理: $name"
        skipped=$((skipped+1))
        continue
    fi

    echo "[CONVERT] $name"
    rm -f "$cand_full" "$cand_base"
    if ! "$SOFFICE" --headless --infilter="writer_pdf_import" \
         --convert-to "docx:Office Open XML Text" \
         --outdir "$(dirname "$f")" "$f" >/dev/null 2>&1; then
        echo "[ERROR] 转换失败: $name"
        failed=$((failed+1))
        continue
    fi

    # 统一 docx 命名
    if [ -f "$cand_base" ] && [ ! -f "$cand_full" ]; then
        mv "$cand_base" "$cand_full"
    fi
    if [ ! -f "$cand_full" ]; then
        echo "[ERROR] 未生成 docx: $name"
        failed=$((failed+1))
        continue
    fi

    echo "[EXTRACT] ${name}.docx -> ${name}_extracted/"
    rm -rf "$dir"
    if ! "${UNZIP_CMD[@]}" "$cand_full" "$dir" >/dev/null 2>&1; then
        echo "[ERROR] 解压失败: $cand_full"
        failed=$((failed+1))
        continue
    fi
    count=$((count+1))
done < <(find "$DOCS_DIR" -type f -print0)

echo
echo "================================="
echo "完成: 处理 $count 个, 跳过 $skipped 个, 失败 $failed 个"

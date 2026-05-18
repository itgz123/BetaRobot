"""将当前文件夹及子文件夹中所有 PDF 转换为 Markdown 文件。

优化点：
- 模糊匹配去除跨页重复行（仅匹配由空格分隔的尾部数字变化，避免误伤产品名）
- 智能标题提取：优先选择含产品型号/名称的行
- 使用 --- 分隔页面，清洗空白行
"""

import fitz  # PyMuPDF
import re
import sys
from collections import Counter
from pathlib import Path


def _clean_line(line: str) -> str:
    return line.rstrip()


def _normalize(line: str) -> str:
    """标准化：仅去除行尾由空格分隔的纯数字（页码），不动与字母粘连的数字。"""
    return re.sub(r'\s+\d{1,4}\s*$', '', line.strip()).strip()


def _is_noise_line(line: str) -> bool:
    stripped = line.strip()
    if not stripped:
        return True
    # 纯页码 "2 | 62"
    if re.match(r'^\d{1,4}\s*[|/]\s*\d{1,4}$', stripped):
        return True
    # 纯短数字
    if re.match(r'^\d{1,4}$', stripped) and len(stripped) <= 4:
        return True
    # 纯分隔符
    if re.match(r'^[-=_.]{3,}$', stripped):
        return True
    # 版本/修订元数据
    if re.match(r'^(Revision|Modifications|Document (number|revision|release))', stripped, re.IGNORECASE):
        return True
    return False


def _find_repeated_normals(all_page_lines: list[list[str]], threshold: float = 0.5) -> set[str]:
    """找出跨页频繁出现的标准化行（页眉/页脚）。"""
    if len(all_page_lines) < 3:
        return set()
    counter = Counter()
    for page_lines in all_page_lines:
        seen = set()
        for line in page_lines:
            s = line.strip()
            if s and s not in seen:
                norm = _normalize(s)
                if len(norm) >= 6:  # 至少 6 字符才算重复模板
                    counter[norm] += 1
                    seen.add(s)
    page_count = len(all_page_lines)
    threshold_count = max(2, int(page_count * threshold))
    return {norm for norm, cnt in counter.items() if cnt >= threshold_count}


def _should_skip_line(stripped: str, repeated_normals: set[str]) -> bool:
    if not stripped:
        return False
    return _normalize(stripped) in repeated_normals


def _title_score(line: str) -> int:
    """给候选标题行打分，越高越可能是标题。"""
    score = 0
    # 含产品型号格式（如 BMI088 / 3D / M3508）加分
    if re.search(r'[A-Z]\d|\d[A-Z]', line):
        score += 4
    # 含中文加分
    if re.search(r'[一-鿿]', line):
        score += 2
    # 含大写开头的英文词（标题风格）
    if re.search(r'\b[A-Z][a-zA-Z]{2,}\b', line):
        score += 2
    # 像文档编号（纯大写/数字/横线，无有意义空格）扣分
    if re.match(r'^[A-Z\d\-_\.]+$', line) and len(line) > 10:
        score -= 4
    # 太短扣分
    if len(line) <= 4:
        score -= 3
    # 太通用扣分
    if line.strip().lower() in ('datasheet', '用户手册', 'user manual', 'data sheet'):
        score -= 3
    # 太长扣分
    if len(line) > 70:
        score -= 2
    # 句子标点扣分
    if re.search(r'[.,;:]\s', line):
        score -= 1
    # 纯数字/日期或只有分隔符扣分
    if re.match(r'^[\d./\s\-]+$', line):
        score -= 3
    # 元数据行扣分
    if re.match(r'^(Notes|Data and|Document|Revision|Version|Page|Modifications)', line, re.IGNORECASE):
        score -= 5
    return score


def _try_extract_title(first_page_text: str) -> str | None:
    """从第一页提取文档标题。"""
    lines = [l.strip() for l in first_page_text.strip().splitlines() if l.strip()]
    if not lines:
        return None
    # 过滤明显不是标题的行
    candidates = []
    for line in lines[:10]:
        if len(line) < 2 or len(line) > 80:
            continue
        if re.match(r'^[vV]\d', line):
            continue
        candidates.append(line)
    if not candidates:
        return None
    # 按评分排序
    candidates.sort(key=_title_score, reverse=True)
    best = candidates[0]
    if _title_score(best) > 0:
        return best
    return None


def pdf_to_markdown(pdf_path: Path, out_path: Path) -> str:
    doc = fitz.open(str(pdf_path))

    all_page_lines: list[list[str]] = []
    for page in doc:
        text = page.get_text("text")
        page_lines = [_clean_line(l) for l in text.splitlines()]
        page_lines = [l for l in page_lines if not _is_noise_line(l)]
        if page_lines:
            all_page_lines.append(page_lines)
    doc.close()

    if not all_page_lines:
        return ""

    repeated_normals = _find_repeated_normals(all_page_lines)

    clean_pages: list[str] = []
    for page_lines in all_page_lines:
        kept = []
        prev_empty = False
        for line in page_lines:
            stripped = line.strip()
            if _should_skip_line(stripped, repeated_normals):
                continue
            if not stripped:
                if not prev_empty:
                    kept.append("")
                    prev_empty = True
            else:
                kept.append(line)
                prev_empty = False
        while kept and kept[0] == "":
            kept.pop(0)
        while kept and kept[-1] == "":
            kept.pop()
        if kept:
            clean_pages.append("\n".join(kept))

    if not clean_pages:
        return ""

    title = _try_extract_title(clean_pages[0])

    parts = []
    if title:
        parts.append(f"# {title}\n")
        first = clean_pages[0]
        if first.lstrip().startswith(title):
            first = first.replace(title, "", 1).strip()
        if first:
            parts.append(first)
        parts.extend(clean_pages[1:])
    else:
        parts = clean_pages

    content = "\n\n---\n\n".join(parts)
    content = re.sub(r'\n{4,}', '\n\n\n', content)
    content = content.strip() + "\n"

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(content, encoding="utf-8")
    return str(out_path)


def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent
    pdf_files = list(root.rglob("*.pdf"))
    count = 0
    for i, pdf in enumerate(pdf_files, 1):
        rel = pdf.relative_to(root)
        out = pdf.with_suffix(".md")
        print(f"[{i}/{len(pdf_files)}] {rel}")
        try:
            pdf_to_markdown(pdf, out)
            count += 1
        except Exception as e:
            print(f"  错误: {e}")
    print(f"\n完成: {count}/{len(pdf_files)} 个文件")


if __name__ == "__main__":
    main()

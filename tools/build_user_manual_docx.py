#!/usr/bin/env python3
"""Build the FireEye user manual DOCX from the Markdown source."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_ALIGN_VERTICAL, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor


BODY_FONT = "Microsoft YaHei"
MONO_FONT = "Consolas"
BLACK = "000000"
BLUE = "1F4E79"
PALE_BLUE = "F3F6FA"
CODE_FILL = "F4F5F7"
BORDER = "D9D9D9"


def set_cell_shading(cell, fill: str) -> None:
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_cell_margins(cell, top=100, start=110, bottom=100, end=110) -> None:
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for margin, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{margin}"))
        if node is None:
            node = OxmlElement(f"w:{margin}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def set_table_borders(table) -> None:
    tbl_pr = table._tbl.tblPr
    borders = tbl_pr.first_child_found_in("w:tblBorders")
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        tbl_pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        tag = f"w:{edge}"
        element = borders.find(qn(tag))
        if element is None:
            element = OxmlElement(tag)
            borders.append(element)
        element.set(qn("w:val"), "single")
        element.set(qn("w:sz"), "6")
        element.set(qn("w:space"), "0")
        element.set(qn("w:color"), BORDER)


def set_repeat_table_header(row) -> None:
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def prevent_row_split(row) -> None:
    tr_pr = row._tr.get_or_add_trPr()
    cant_split = OxmlElement("w:cantSplit")
    tr_pr.append(cant_split)


def set_font(run, name=BODY_FONT, size=None, bold=None, color=BLACK) -> None:
    run.font.name = name
    run._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), name)
    if size is not None:
        run.font.size = Pt(size)
    if bold is not None:
        run.bold = bold
    if color:
        run.font.color.rgb = RGBColor.from_string(color)


def set_style_font(style, name=BODY_FONT, size=None, bold=None, color=BLACK) -> None:
    style.font.name = name
    style._element.get_or_add_rPr().rFonts.set(qn("w:eastAsia"), name)
    if size is not None:
        style.font.size = Pt(size)
    if bold is not None:
        style.font.bold = bold
    if color:
        style.font.color.rgb = RGBColor.from_string(color)


def remove_paragraph_borders(paragraph) -> None:
    p_pr = paragraph._p.get_or_add_pPr()
    for border in list(p_pr.findall(qn("w:pBdr"))):
        p_pr.remove(border)


def remove_style_borders(style) -> None:
    p_pr = style._element.get_or_add_pPr()
    for border in list(p_pr.findall(qn("w:pBdr"))):
        p_pr.remove(border)


def add_page_number(paragraph) -> None:
    paragraph.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = paragraph.add_run()
    set_font(run, size=9, color="666666")
    begin = OxmlElement("w:fldChar")
    begin.set(qn("w:fldCharType"), "begin")
    instr = OxmlElement("w:instrText")
    instr.set(qn("xml:space"), "preserve")
    instr.text = " PAGE "
    separate = OxmlElement("w:fldChar")
    separate.set(qn("w:fldCharType"), "separate")
    end = OxmlElement("w:fldChar")
    end.set(qn("w:fldCharType"), "end")
    run._r.extend([begin, instr, separate, end])


def inline_runs(paragraph, text: str, size=10.5, color=BLACK) -> None:
    token = re.compile(r"(\*\*[^*]+\*\*|`[^`]+`)")
    pos = 0
    for match in token.finditer(text):
        if match.start() > pos:
            run = paragraph.add_run(text[pos:match.start()])
            set_font(run, size=size, color=color)
        value = match.group(0)
        if value.startswith("**"):
            run = paragraph.add_run(value[2:-2])
            set_font(run, size=size, bold=True, color=color)
        else:
            run = paragraph.add_run(value[1:-1])
            set_font(run, name=MONO_FONT, size=size - 0.5, color=color)
        pos = match.end()
    if pos < len(text):
        run = paragraph.add_run(text[pos:])
        set_font(run, size=size, color=color)


def configure_document(doc: Document) -> None:
    section = doc.sections[0]
    section.page_width = Cm(21)
    section.page_height = Cm(29.7)
    section.top_margin = Cm(2.2)
    section.bottom_margin = Cm(2.0)
    section.left_margin = Cm(2.3)
    section.right_margin = Cm(2.3)

    normal = doc.styles["Normal"]
    set_style_font(normal, size=10.5)
    normal.paragraph_format.line_spacing = 1.35
    normal.paragraph_format.space_after = Pt(6)

    title = doc.styles["Title"]
    set_style_font(title, size=26, bold=True)
    title.paragraph_format.space_after = Pt(15)
    remove_style_borders(title)

    h1 = doc.styles["Heading 1"]
    set_style_font(h1, size=16, bold=True)
    h1.paragraph_format.space_before = Pt(16)
    h1.paragraph_format.space_after = Pt(7)
    h1.paragraph_format.keep_with_next = True

    h2 = doc.styles["Heading 2"]
    set_style_font(h2, size=12.5, bold=True)
    h2.paragraph_format.space_before = Pt(12)
    h2.paragraph_format.space_after = Pt(5)
    h2.paragraph_format.keep_with_next = True

    h3 = doc.styles["Heading 3"]
    set_style_font(h3, size=11, bold=True)
    h3.paragraph_format.space_before = Pt(9)
    h3.paragraph_format.space_after = Pt(4)
    h3.paragraph_format.keep_with_next = True


def add_cover(doc: Document, title: str, intro: str) -> None:
    section = doc.sections[0]
    section.different_first_page_header_footer = True

    spacer = doc.add_paragraph()
    spacer.paragraph_format.space_before = Pt(66)
    title_p = doc.add_paragraph(style="Title")
    title_p.alignment = WD_ALIGN_PARAGRAPH.LEFT
    title_p.add_run(title)
    remove_paragraph_borders(title_p)

    subtitle = doc.add_paragraph()
    subtitle.paragraph_format.space_after = Pt(26)
    inline_runs(subtitle, intro, size=12)

    meta = doc.add_table(rows=4, cols=2)
    meta.alignment = WD_TABLE_ALIGNMENT.LEFT
    set_table_borders(meta)
    set_repeat_table_header(meta.rows[0])
    labels = [
        "\u6587\u6863\u7248\u672c",
        "\u9002\u7528\u65e5\u671f",
        "\u5f53\u524d\u6e90\u7801\u7248\u672c",
        "\u9879\u76ee\u56e2\u961f",
    ]
    values = ["v1.2", "2026-11-01", "FireEye 2.1.1", "\u98ce\u6b47\u5ba2"]
    for idx, (label, value) in enumerate(zip(labels, values)):
        set_cell_shading(meta.cell(idx, 0), PALE_BLUE)
        for cell, text, bold in (
            (meta.cell(idx, 0), label, True),
            (meta.cell(idx, 1), value, False),
        ):
            set_cell_margins(cell, top=130, start=130, bottom=130, end=130)
            cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
            p = cell.paragraphs[0]
            p.paragraph_format.space_after = Pt(0)
            run = p.add_run(text)
            set_font(run, size=10.5, bold=bold)
    meta.columns[0].width = Cm(4.2)
    meta.columns[1].width = Cm(7.2)

    note = doc.add_paragraph()
    note.paragraph_format.space_before = Pt(30)
    inline_runs(
        note,
        "\u672c\u8bf4\u660e\u4e66\u4ec5\u8986\u76d6\u5f53\u524d\u5df2\u5b9e\u73b0\u80fd\u529b\u548c\u5df2\u5b8c\u6210\u9a8c\u8bc1\u7684\u64cd\u4f5c\u3002\u672a\u5b8c\u6210\u9a8c\u6536\u7684\u529f\u80fd\u5df2\u5728\u6b63\u6587\u4e2d\u5355\u72ec\u6807\u6ce8\u3002",
        size=9.5,
        color="555555",
    )


def parse_blocks(lines: list[str]):
    blocks = []
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        if not line or line == "---":
            i += 1
            continue
        if line.startswith("### "):
            blocks.append(("h3", line[4:].strip()))
            i += 1
            continue
        if line.startswith("## "):
            blocks.append(("h2", line[3:].strip()))
            i += 1
            continue
        if line.startswith("# "):
            blocks.append(("h1", line[2:].strip()))
            i += 1
            continue
        if line.startswith("```"):
            code = []
            i += 1
            while i < len(lines) and not lines[i].startswith("```"):
                code.append(lines[i].rstrip("\n"))
                i += 1
            i += 1
            blocks.append(("code", "\n".join(code)))
            continue
        if line.startswith("|"):
            table_lines = []
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                table_lines.append(lines[i].strip())
                i += 1
            if len(table_lines) >= 2 and re.match(
                r"^\|?\s*:?-+", table_lines[1]
            ):
                rows = []
                for raw in [table_lines[0], *table_lines[2:]]:
                    cells = [c.strip() for c in raw.strip("|").split("|")]
                    rows.append(cells)
                blocks.append(("table", rows))
            continue
        if re.match(r"^\d+\.\s+", line):
            items = []
            while i < len(lines) and re.match(r"^\d+\.\s+", lines[i].strip()):
                items.append(re.sub(r"^\d+\.\s+", "", lines[i].strip()))
                i += 1
            blocks.append(("numbered", items))
            continue
        if line.startswith("- "):
            items = []
            while i < len(lines) and lines[i].strip().startswith("- "):
                items.append(lines[i].strip()[2:].strip())
                i += 1
            blocks.append(("bullets", items))
            continue
        if line.startswith("> "):
            quote = []
            while i < len(lines) and lines[i].strip().startswith(">"):
                quote.append(lines[i].strip()[1:].strip())
                i += 1
            blocks.append(("quote", " ".join(quote)))
            continue
        para = [line.strip()]
        i += 1
        while i < len(lines):
            nxt = lines[i].strip()
            if not nxt or nxt.startswith(("#", "```", "|", "> ", "- ")) or re.match(
                r"^\d+\.\s+", nxt
            ):
                break
            para.append(nxt)
            i += 1
        blocks.append(("paragraph", " ".join(para)))
    return blocks


def add_table(doc: Document, rows: list[list[str]]) -> None:
    if not rows:
        return
    cols = max(len(row) for row in rows)
    table = doc.add_table(rows=len(rows), cols=cols)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    table.autofit = True
    set_table_borders(table)
    for r_idx, row_values in enumerate(rows):
        row = table.rows[r_idx]
        prevent_row_split(row)
        if r_idx == 0:
            set_repeat_table_header(row)
        for c_idx in range(cols):
            cell = row.cells[c_idx]
            set_cell_margins(cell)
            cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
            if c_idx < len(row_values):
                p = cell.paragraphs[0]
                p.paragraph_format.space_after = Pt(0)
                inline_runs(
                    p,
                    row_values[c_idx],
                    size=9.2 if r_idx else 9.5,
                    color="FFFFFF" if r_idx == 0 else BLACK,
                )
            if r_idx == 0:
                set_cell_shading(cell, BLUE)
            elif r_idx % 2 == 0:
                set_cell_shading(cell, PALE_BLUE)
    spacer = doc.add_paragraph()
    spacer.paragraph_format.space_after = Pt(0)


def add_code_block(doc: Document, code: str) -> None:
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Cm(0.35)
    p.paragraph_format.right_indent = Cm(0.35)
    p.paragraph_format.space_before = Pt(3)
    p.paragraph_format.space_after = Pt(8)
    p.paragraph_format.line_spacing = 1.05
    p_pr = p._p.get_or_add_pPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), CODE_FILL)
    p_pr.append(shd)
    run = p.add_run(code)
    set_font(run, name=MONO_FONT, size=9)


def add_toc(doc: Document, headings: list[str]) -> None:
    h = doc.add_paragraph()
    h.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = h.add_run("\u76ee\u5f55")
    set_font(run, size=18, bold=True)
    h.paragraph_format.space_after = Pt(12)

    table = doc.add_table(rows=len(headings), cols=2)
    table.alignment = WD_TABLE_ALIGNMENT.CENTER
    set_table_borders(table)
    set_repeat_table_header(table.rows[0])
    for idx, heading in enumerate(headings, 1):
        row = table.rows[idx - 1]
        prevent_row_split(row)
        for cell_idx, text in ((0, f"{idx:02d}"), (1, heading)):
            cell = row.cells[cell_idx]
            set_cell_margins(cell, top=85, start=110, bottom=85, end=110)
            cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
            p = cell.paragraphs[0]
            p.paragraph_format.space_after = Pt(0)
            p.alignment = (
                WD_ALIGN_PARAGRAPH.CENTER if cell_idx == 0 else WD_ALIGN_PARAGRAPH.LEFT
            )
            run = p.add_run(text)
            set_font(run, size=10, bold=(cell_idx == 0), color=BLUE if cell_idx == 0 else BLACK)
        if idx % 2 == 0:
            set_cell_shading(row.cells[0], PALE_BLUE)
            set_cell_shading(row.cells[1], PALE_BLUE)
    table.columns[0].width = Cm(1.5)
    table.columns[1].width = Cm(10.5)
    doc.add_page_break()


def build(source: Path, output: Path) -> None:
    text = source.read_text(encoding="utf-8")
    lines = text.splitlines()
    title = next((line[2:].strip() for line in lines if line.startswith("# ")), "FireEye")
    first_h1 = next(i for i, line in enumerate(lines) if line.startswith("# "))
    body_lines = lines[first_h1 + 1 :]
    first_para = ""
    for line in body_lines:
        if line.strip() and not line.startswith(("#", ">", "|", "- ", "```")):
            first_para = line.strip()
            break

    doc = Document()
    configure_document(doc)
    props = doc.core_properties
    props.title = title
    props.subject = "FireEye user manual"
    props.author = "\u98ce\u6b47\u5ba2"
    props.keywords = "FireEye openvela NuttX W5500 MiMo"

    add_cover(doc, title, first_para)
    body_section = doc.add_section(WD_SECTION.NEW_PAGE)
    body_section.page_width = Cm(21)
    body_section.page_height = Cm(29.7)
    body_section.top_margin = Cm(2.2)
    body_section.bottom_margin = Cm(2.0)
    body_section.left_margin = Cm(2.3)
    body_section.right_margin = Cm(2.3)
    body_section.footer.is_linked_to_previous = False
    add_page_number(body_section.footer.paragraphs[0])

    blocks = parse_blocks(body_lines)
    h2_headings = [value for kind, value in blocks if kind == "h2"]
    add_toc(doc, h2_headings)

    for kind, value in blocks:
        if kind == "h2":
            doc.add_paragraph(value, style="Heading 1")
        elif kind == "h3":
            doc.add_paragraph(value, style="Heading 2")
        elif kind == "h1":
            doc.add_paragraph(value, style="Heading 1")
        elif kind == "paragraph":
            p = doc.add_paragraph()
            inline_runs(p, value)
        elif kind == "quote":
            p = doc.add_paragraph()
            p.paragraph_format.left_indent = Cm(0.4)
            inline_runs(p, value, size=9.5, color="555555")
        elif kind == "bullets":
            for item in value:
                p = doc.add_paragraph(style="List Bullet")
                p.paragraph_format.space_after = Pt(3)
                inline_runs(p, item)
        elif kind == "numbered":
            for number, item in enumerate(value, 1):
                p = doc.add_paragraph()
                p.paragraph_format.left_indent = Cm(0.72)
                p.paragraph_format.first_line_indent = Cm(-0.72)
                p.paragraph_format.space_after = Pt(3)
                prefix = p.add_run(f"{number}. ")
                set_font(prefix, size=10.5)
                inline_runs(p, item)
        elif kind == "code":
            add_code_block(doc, value)
        elif kind == "table":
            add_table(doc, value)

    output.parent.mkdir(parents=True, exist_ok=True)
    doc.save(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build(args.source, args.output)


if __name__ == "__main__":
    main()

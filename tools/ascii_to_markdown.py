#!/usr/bin/env python3
from __future__ import annotations
import argparse
import os
import re
from typing import List, Tuple

def _is_border(line: str) -> bool:
    l = line.lstrip()
    if not l.startswith("+"):
        return False
    # Consider borders made from + - = and spaces. 
    # Must contain at least one dash to differentiate from headers like "+ Timing:"
    chars = set(l.strip())
    if "-" not in chars:
        return False
    return chars <= set("+-=| ")

def _is_table_line(line: str) -> bool:
    l = line.lstrip()
    return l.startswith("|") or _is_border(l)

def _split_cells(row: str) -> List[str]:
    # Remove leading/trailing pipes then split
    return [c.strip() for c in row.strip().strip("|").split("|")]

def _pad_to_len(rows: List[List[str]], n: int) -> List[List[str]]:
    return [r + [""] * (n - len(r)) for r in rows]

def ascii_to_markdown(ascii_table: str) -> str:
    """
    Convert a single ASCII table (box-drawing using '+' and '|' rows)
    to GitHub-flavored Markdown. Handles 1 or 2 header rows.
    """
    # 1) Normalize & split
    lines = [l.rstrip("\n") for l in ascii_table.splitlines() if l.strip()]
    lines = [l.lstrip() for l in lines]  # ignore leading indentation

    # 2) Find all border lines
    borders = [i for i, l in enumerate(lines) if _is_border(l)]
    if len(borders) < 2:
        raise ValueError("Couldn't find enough table borders")

    # The first border is the top; the next border after header rows is header sep.
    # The last border is the bottom (some tables have more inner borders; we use the last).
    top = borders[0]

    # Find the first border that appears after at least one non-border (header) line
    header_sep = None
    for idx in borders[1:]:
        # Ensure there is at least one non-border line between top and this border
        if any(not _is_border(l) for l in lines[top+1:idx]):
            header_sep = idx
            break
    if header_sep is None:
        raise ValueError("Couldn't identify header separator border")

    bottom = borders[-1]

    # 3) Header rows (could be 1 or 2+)
    header_rows_raw = [l for l in lines[top+1:header_sep] if l.lstrip().startswith("|")]
    if len(header_rows_raw) == 0:
        raise ValueError("No header rows found between borders")

    header_matrix = [_split_cells(r) for r in header_rows_raw]
    max_cols = max(len(r) for r in header_matrix)
    header_matrix = _pad_to_len(header_matrix, max_cols)

    # Combine header rows by column (join with a space, strip)
    combined_headers = [" ".join(col).strip() for col in zip(*header_matrix)]

    # 4) Data rows (between header_sep and bottom)
    data_lines = [l for l in lines[header_sep+1:bottom] if l.lstrip().startswith("|")]
    data_matrix = [_split_cells(r) for r in data_lines]
    # Pad data rows to max_cols too (just in case)
    data_matrix = [_pad_to_len([r], max_cols)[0] for r in data_matrix]

    # 5) Emit Markdown
    out = []
    out.append("| " + " | ".join(combined_headers) + " |")
    out.append("| " + " | ".join("---" for _ in combined_headers) + " |")
    for r in data_matrix:
        out.append("| " + " | ".join(r) + " |")

    return "\n".join(out)

def extract_ascii_tables(text: str) -> List[Tuple[str, str]]:
    """
    Find all ASCII tables in the text (blocks of consecutive lines where
    each non-empty line starts with '|' or is a border '+---').
    Returns a list of tuples: (Title, TableBlock).
    Scans the text for tables and tracks the hierarchical context (Major Section > Sub Section > Table Label).
    """
    lines = text.splitlines()
    tables = []
    i = 0
    n = len(lines)

    current_major = ""
    current_sub = ""

    # Regex for "== Name ==" or "== Name"
    # It must start with ==, allow optional whitespace, capture text, allow optional trailing ==
    re_major = re.compile(r"^==\s*(.+?)(?:\s*==)?$")
    
    # Regex for "+ Name:"
    re_sub = re.compile(r"^\+\s*(.+?):$")

    while i < n:
        line = lines[i]
        stripped = line.strip()

        # 1. Detect Major Section (e.g. "== Performance Estimates")
        # Ensure it's not just a separator line like "========"
        if stripped.startswith("==") and any(c not in "= " for c in stripped):
            match = re_major.match(stripped)
            if match:
                current_major = match.group(1).strip()
                current_sub = "" # Reset subsection on new major section
            i += 1
            continue

        # 2. Detect Sub Section (e.g. "+ Timing:")
        # Must start with +, end with :, and NOT be a table border (no dashes)
        if stripped.startswith("+") and stripped.endswith(":") and "-" not in stripped:
            match = re_sub.match(stripped)
            if match:
                current_sub = match.group(1).strip()
            i += 1
            continue

        # 3. Detect Table Start
        if _is_border(line):
            # Look backwards for a local label (e.g. "* Summary:")
            local_label = ""
            k = i - 1
            while k >= 0:
                prev_line = lines[k].strip()
                if not prev_line:
                    k -= 1
                    continue
                # Stop looking if we hit a header pattern
                if prev_line.startswith("==") or (prev_line.startswith("+") and prev_line.endswith(":")):
                    break
                
                # Assume the last non-empty line is the label
                # Clean up bullets (*, -) and colons
                clean_label = prev_line.lstrip("*+- \t").rstrip(":")
                if clean_label:
                    local_label = clean_label
                break

            # Build Full Title
            # Filter out empty parts and join
            parts = [p for p in [current_major, current_sub, local_label] if p]
            full_title = " > ".join(parts)

            # Extract table block
            start = i
            i += 1
            while i < n and (lines[i].strip() == "" or _is_table_line(lines[i])):
                i += 1
            end = i 
            block = "\n".join(lines[start:end]).strip("\n")
            
            if sum(1 for l in block.splitlines() if _is_border(l)) >= 2:
                tables.append((full_title, block))
            continue
        
        i += 1

    return tables

def convert_file(input_path: str) -> str:
    with open(input_path, "r", encoding="utf-8", errors="replace") as f:
        txt = f.read()

    tables = extract_ascii_tables(txt)
    
    if not tables:
        md_out = "_No ASCII tables found._\n"
    else:
        md_blocks = []
        for k, (title, t) in enumerate(tables, 1):
            try:
                md_table = ascii_to_markdown(t)
            except Exception as e:
                md_table = f"> **Table {k}** (kept as-is; conversion failed: {e})\n\n```\n{t}\n```"
            
            header = f"### {title}" if title else f"### Table {k}"
            md_blocks.append(f"{header}\n\n{md_table}")
        
        md_out = ("\n\n").join(md_blocks) + "\n"

    base, _ = os.path.splitext(input_path)
    output_path = f"{base}_md.txt"
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(md_out)
    return output_path

def main():
    ap = argparse.ArgumentParser(description="Extract and convert ASCII tables in a file to Markdown.")
    ap.add_argument("input_file", help="Path to the text file containing ASCII tables.")
    args = ap.parse_args()
    out = convert_file(args.input_file)
    print(f"Wrote: {out}")

if __name__ == "__main__":
    main()
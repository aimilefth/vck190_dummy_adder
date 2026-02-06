#!/usr/bin/env python3
import argparse
from pathlib import Path

def patch_pragmas(text: str) -> str:
    out_lines = []
    for line in text.splitlines(keepends=True):
        if "#pragma HLS pipeline" not in line:
            out_lines.append(line)
            continue

        # Preserve trailing // comments (common in generated code)
        code, comment = line, ""
        idx = line.find("//")
        if idx != -1:
            code, comment = line[:idx], line[idx:]

        # Only target: "#pragma HLS pipeline II=1" (with any indentation/spaces)
        stripped = code.strip()
        if stripped.startswith("#pragma") and "HLS" in stripped and "pipeline" in stripped and "II=1" in stripped:
            # Avoid changing other II values; ensure it specifically contains II=1
            # and avoid double-adding rewind
            if "II=1" in stripped and "rewind" not in stripped.split():
                # Add ' rewind' before any trailing whitespace
                # Keep original indentation exactly
                newline = ""
                if code.endswith("\r\n"):
                    newline = "\r\n"
                    code_body = code[:-2]
                elif code.endswith("\n"):
                    newline = "\n"
                    code_body = code[:-1]
                else:
                    code_body = code

                out_lines.append(code_body.rstrip() + " rewind" + newline + comment)
                continue

        out_lines.append(line)

    return "".join(out_lines)

def main():
    ap = argparse.ArgumentParser(description="Patch HLS pragmas and rename file to gemm_kernel.hpp")
    ap.add_argument("input", help="Path to the .cpp file")
    ap.add_argument("--force", action="store_true", help="Overwrite gemm_kernel.hpp if it already exists")
    args = ap.parse_args()

    src = Path(args.input).expanduser().resolve()
    if not src.is_file():
        raise SystemExit(f"Input file not found: {src}")

    text = src.read_text(encoding="utf-8", errors="replace")
    patched = patch_pragmas(text)
    src.write_text(patched, encoding="utf-8")

    dst = src.with_name("gemm_kernel.hpp")
    if dst.exists():
        if not args.force:
            raise SystemExit(f"Refusing to overwrite existing file: {dst} (use --force)")
        dst.unlink()

    src.rename(dst)
    print(f"Patched pragmas and renamed:\n  {src}  ->  {dst}")

if __name__ == "__main__":
    main()

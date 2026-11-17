"""Converts a text pixel sheet (docs/pixel-sheets.md) to PNGs and a .sheet manifest.

    py tools/convert_sheet.py game/assets/sprites/sprites.txt --each      # one PNG per sprite
    py tools/convert_sheet.py game/assets/sprites/font.txt --atlas 16     # one PNG, 16 sprites per row

The manifest and PNGs are written next to the text file. Standard library only.
"""

import argparse
import struct
import sys
import zlib
from pathlib import Path


def parse_sheet(text):
    """Returns [(name, frame_w, frame_h, frames, rows_of_rgba)] in file order."""
    palette = {}
    sprites = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        i += 1
        words = line.split()
        if not words or words[0].startswith("#"):
            continue
        if words[0] == "color":
            alpha = int(words[5]) if len(words) > 5 else 255
            palette[words[1]] = (int(words[2]), int(words[3]), int(words[4]), alpha)
        elif words[0] == "sprite":
            name, fw, fh = words[1], int(words[2]), int(words[3])
            frames = int(words[4]) if len(words) > 4 else 1
            rows = []
            for _ in range(fh):
                row = lines[i]
                i += 1
                rows.append([(0, 0, 0, 0) if c == "." else palette[c] for c in row])
            sprites.append((name, fw, fh, frames, rows))
        else:
            sys.exit(f"unexpected line: {line!r}")
    return sprites


def write_png(path, rows):
    height, width = len(rows), len(rows[0])
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px) for row in rows)

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("sheet", type=Path, help="the .txt pixel sheet")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--each", action="store_true", help="one PNG per sprite, in a folder named after the sheet")
    mode.add_argument("--atlas", type=int, metavar="PER_ROW", help="all sprites in one PNG, PER_ROW to a row")
    args = parser.parse_args()

    sprites = parse_sheet(args.sheet.read_text(encoding="utf-8"))
    stem = args.sheet.stem
    out = args.sheet.parent
    manifest = [f"# Generated from {args.sheet.name} by tools/convert_sheet.py. The art now lives in the PNGs."]

    if args.each:
        folder = out / stem
        folder.mkdir(exist_ok=True)
        for name, fw, fh, frames, rows in sprites:
            write_png(folder / f"{name}.png", rows)
            manifest.append(f"sprite {name} {stem}/{name}.png {fw} {fh} {frames}")
    else:
        cell_w = max(fw * frames for _, fw, _, frames, _ in sprites) + 1
        cell_h = max(fh for _, _, fh, _, _ in sprites) + 1
        per_row = args.atlas
        count_rows = -(-len(sprites) // per_row)
        canvas = [[(0, 0, 0, 0)] * (cell_w * per_row) for _ in range(cell_h * count_rows)]
        manifest.append(f"image {stem}.png")
        for index, (name, fw, fh, frames, rows) in enumerate(sprites):
            x0, y0 = index % per_row * cell_w, index // per_row * cell_h
            for y, row in enumerate(rows):
                canvas[y0 + y][x0 : x0 + len(row)] = row
            manifest.append(f"region {name} {x0} {y0} {fw} {fh} {frames}")
        write_png(out / f"{stem}.png", canvas)

    (out / f"{stem}.sheet").write_text("\n".join(manifest) + "\n", encoding="utf-8", newline="\n")
    print(f"wrote {stem}.sheet with {len(sprites)} sprites")


if __name__ == "__main__":
    main()

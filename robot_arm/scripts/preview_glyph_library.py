#!/usr/bin/env python3
"""Preview glyph strokes from a Hershey glyph library JSON as SVG."""

import argparse
import html
import json
import math
import os


def distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def glyph_stats(character, glyph):
    strokes = glyph.get("strokes", [])
    lines = []
    lines.append("character: {}".format(repr(character)))
    lines.append("advance: {:.6f}".format(float(glyph.get("advance", 0.0))))
    lines.append("bounds: {}".format(glyph.get("bounds", {})))
    lines.append("stroke count: {}".format(len(strokes)))
    for index, stroke in enumerate(strokes):
        points = stroke.get("points", [])
        first = points[0] if points else None
        last = points[-1] if points else None
        close_distance = distance(first, last) if first and last else 0.0
        lines.append(
            "  stroke {}: closed={} points={} first={} last={} first_last_distance={:.6f}".format(
                index,
                bool(stroke.get("closed", False)),
                len(points),
                first,
                last,
                close_distance,
            )
        )
    return "\n".join(lines)


def collect_bounds(glyphs):
    points = []
    cursor = 0.0
    for _, glyph in glyphs:
        for stroke in glyph.get("strokes", []):
            for x, y in stroke.get("points", []):
                points.append((cursor + float(x), float(y)))
        cursor += float(glyph.get("advance", 0.8)) + 0.18

    if not points:
        return 0.0, 0.0, 1.0, 1.0
    min_x = min(p[0] for p in points)
    max_x = max(p[0] for p in points)
    min_y = min(p[1] for p in points)
    max_y = max(p[1] for p in points)
    return min_x, min_y, max_x, max_y


def write_svg(path, chars, glyph_map):
    selected = [(c, glyph_map[c]) for c in chars if c in glyph_map]
    min_x, min_y, max_x, max_y = collect_bounds(selected)
    scale = 260.0
    margin = 40.0
    width = max(160.0, (max_x - min_x) * scale + margin * 2.0)
    height = max(160.0, (max_y - min_y) * scale + margin * 2.0)

    def sx(x, cursor):
        return margin + (cursor + x - min_x) * scale

    def sy(y):
        return height - margin - (y - min_y) * scale

    cursor = 0.0
    parts = [
        '<svg xmlns="http://www.w3.org/2000/svg" width="{:.1f}" height="{:.1f}" viewBox="0 0 {:.1f} {:.1f}">'.format(width, height, width, height),
        '<rect width="100%" height="100%" fill="white"/>',
        '<g fill="none" stroke="black" stroke-width="3" stroke-linecap="round" stroke-linejoin="round">',
    ]

    for character, glyph in selected:
        for stroke in glyph.get("strokes", []):
            points = stroke.get("points", [])
            if not points:
                continue
            command = ["M {:.3f} {:.3f}".format(sx(points[0][0], cursor), sy(points[0][1]))]
            for point in points[1:]:
                command.append("L {:.3f} {:.3f}".format(sx(point[0], cursor), sy(point[1])))
            if stroke.get("closed", False):
                command.append("Z")
            parts.append('<path d="{}"/>'.format(" ".join(command)))
        parts.append(
            '<text x="{:.3f}" y="{:.3f}" font-size="18" fill="#555">{}</text>'.format(
                sx(0.0, cursor),
                height - 10.0,
                html.escape(character),
            )
        )
        cursor += float(glyph.get("advance", 0.8)) + 0.18

    parts.extend(["</g>", "</svg>"])
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(parts))
        f.write("\n")


def main():
    parser = argparse.ArgumentParser(description="Preview Hershey glyph library strokes.")
    parser.add_argument("--input", required=True)
    parser.add_argument("--chars", default="roq")
    parser.add_argument("--output", required=True)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        data = json.load(f)

    glyph_map = data.get("glyphs", {})
    if args.verbose:
        print("source:", data.get("source"))
        print("font:", data.get("font"))
        for character in args.chars:
            glyph = glyph_map.get(character)
            if glyph is None:
                print("character {!r}: missing".format(character))
                continue
            print(glyph_stats(character, glyph))

    write_svg(args.output, args.chars, glyph_map)
    print("Wrote preview SVG to {}".format(args.output))


if __name__ == "__main__":
    main()

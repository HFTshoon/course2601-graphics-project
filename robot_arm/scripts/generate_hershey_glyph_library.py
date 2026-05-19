#!/usr/bin/env python3
"""Generate a reusable Hershey glyph library JSON.

This is a preprocessing tool. The OpenGL application loads the generated JSON
at runtime and composes text in C++; it never invokes Python automatically.
"""

import argparse
import json
import math
import os
import string
import sys


DEFAULT_PUNCTUATION = ".,!?-_"


def parse_bool(value):
    if isinstance(value, bool):
        return value
    normalized = str(value).strip().lower()
    if normalized in ("1", "true", "yes", "on"):
        return True
    if normalized in ("0", "false", "no", "off"):
        return False
    raise argparse.ArgumentTypeError("expected true or false")


def distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def unpack_segment(segment):
    if len(segment) == 2 and len(segment[0]) == 2 and len(segment[1]) == 2:
        return (float(segment[0][0]), float(segment[0][1])), (float(segment[1][0]), float(segment[1][1]))
    if len(segment) == 4:
        return (float(segment[0]), float(segment[1])), (float(segment[2]), float(segment[3]))
    raise ValueError("Unsupported Hershey line segment format: {}".format(repr(segment)))


def append_segment_as_stroke(strokes, start, end, epsilon=1e-5):
    if distance(start, end) <= epsilon:
        return
    if strokes and distance(strokes[-1]["points"][-1], start) <= epsilon:
        strokes[-1]["points"].append([end[0], end[1]])
    else:
        strokes.append({"closed": False, "points": [[start[0], start[1]], [end[0], end[1]]]})


def bounds_for_strokes(strokes):
    points = [point for stroke in strokes for point in stroke["points"]]
    if not points:
        return {
            "min": [0.0, 0.0],
            "max": [0.0, 0.0],
            "width": 0.0,
            "height": 0.0,
        }

    min_x = min(point[0] for point in points)
    min_y = min(point[1] for point in points)
    max_x = max(point[0] for point in points)
    max_y = max(point[1] for point in points)
    return {
        "min": [min_x, min_y],
        "max": [max_x, max_y],
        "width": max_x - min_x,
        "height": max_y - min_y,
    }


def normalize_glyph_strokes(strokes, denominator, normalize):
    raw_bounds = bounds_for_strokes(strokes)
    min_x, min_y = raw_bounds["min"]
    denom = max(denominator, 1e-6) if normalize else 1.0

    normalized = []
    for stroke in strokes:
        points = []
        for x, y in stroke["points"]:
            if normalize:
                points.append([(x - min_x) / denom, (y - min_y) / denom])
            else:
                points.append([x, y])
        if len(points) >= 2:
            normalized.append({"closed": bool(stroke.get("closed", False)), "points": points})
    return normalized


def character_list(args):
    chars = []
    if args.characters:
        chars.extend(args.characters)
    else:
        chars.extend(string.ascii_lowercase)
        if args.include_uppercase:
            chars.extend(string.ascii_uppercase)
        if args.include_digits:
            chars.extend(string.digits)
        chars.extend(DEFAULT_PUNCTUATION)
        chars.append(" ")

    deduped = []
    for c in chars:
        if c not in deduped:
            deduped.append(c)
    if " " not in deduped:
        deduped.append(" ")
    return deduped


def load_hershey_raw_glyphs(chars, font_name):
    from HersheyFonts import HersheyFonts

    font = HersheyFonts()
    try:
        font.load_default_font(font_name)
    except Exception as exc:
        available = getattr(font, "default_font_names", None)
        if callable(available):
            available = available()
        suffix = ""
        if available:
            suffix = "\nAvailable default fonts include: {}".format(", ".join(str(name) for name in available))
        raise RuntimeError("Failed to load Hershey font '{}': {}{}".format(font_name, exc, suffix))

    raw_glyphs = {}
    for c in chars:
        if c == " ":
            raw_glyphs[c] = []
            continue
        strokes = []
        for segment in font.lines_for_text(c):
            start, end = unpack_segment(segment)
            append_segment_as_stroke(strokes, start, end)
        raw_glyphs[c] = strokes
    return raw_glyphs, "hershey-fonts"


def fallback_raw_glyphs(chars):
    sys.stderr.write(
        "Warning: Hershey-Fonts package is not installed. "
        "Install with: pip install Hershey-Fonts\n"
        "Writing fallback glyph library instead.\n"
    )

    raw_glyphs = {}
    for c in chars:
        if c == " ":
            raw_glyphs[c] = []
            continue
        lower = c.lower()
        if lower == "a":
            strokes = [
                {"closed": False, "points": [[0.45, 0.05], [0.2, 0.0], [0.0, 0.25], [0.08, 0.55], [0.35, 0.65], [0.55, 0.42], [0.45, 0.05]]},
                {"closed": False, "points": [[0.52, 0.60], [0.55, 0.18], [0.68, 0.02]]},
            ]
        elif lower == "b":
            strokes = [
                {"closed": False, "points": [[0.0, 0.0], [0.0, 0.9]]},
                {"closed": False, "points": [[0.0, 0.5], [0.28, 0.68], [0.58, 0.48], [0.52, 0.18], [0.20, 0.04], [0.0, 0.18]]},
            ]
        elif lower == "c":
            strokes = [
                {"closed": False, "points": [[0.6, 0.52], [0.38, 0.70], [0.08, 0.55], [0.0, 0.25], [0.22, 0.02], [0.55, 0.12]]},
            ]
        elif c.isdigit():
            strokes = [
                {"closed": False, "points": [[0.0, 0.0], [0.0, 0.7], [0.46, 0.7], [0.46, 0.0], [0.0, 0.0]]},
            ]
        else:
            strokes = [
                {"closed": False, "points": [[0.0, 0.0], [0.0, 0.7], [0.42, 0.7], [0.42, 0.0], [0.0, 0.0]]},
            ]
        raw_glyphs[c] = strokes
    return raw_glyphs, "fallback-no-hershey-fonts"


def build_library(raw_glyphs, source, font_name, normalize):
    non_empty_bounds = [bounds_for_strokes(strokes) for strokes in raw_glyphs.values() if strokes]
    common_height = max([bounds["height"] for bounds in non_empty_bounds] + [1.0])
    default_advance = 1.0
    default_space_advance = 0.6

    glyphs = {}
    for c, raw_strokes in raw_glyphs.items():
        if c == " ":
            glyphs[c] = {
                "advance": default_space_advance,
                "bounds": {
                    "min": [0.0, 0.0],
                    "max": [0.0, 0.0],
                    "width": 0.0,
                    "height": 0.0,
                },
                "strokes": [],
            }
            continue

        normalized_strokes = normalize_glyph_strokes(raw_strokes, common_height, normalize)
        bounds = bounds_for_strokes(normalized_strokes)
        advance = max(bounds["width"] + 0.12, 0.35)
        glyphs[c] = {
            "advance": advance,
            "bounds": bounds,
            "strokes": normalized_strokes,
        }

    return {
        "source": source,
        "font": font_name,
        "coordinate_system": "local_2d_xz",
        "units": "normalized" if normalize else "raw",
        "default_advance": default_advance,
        "default_space_advance": default_space_advance,
        "glyphs": glyphs,
    }


def main():
    parser = argparse.ArgumentParser(description="Generate a reusable Hershey glyph library JSON.")
    parser.add_argument("--font", default="futural")
    parser.add_argument("--output", required=True)
    parser.add_argument("--characters", default="")
    parser.add_argument("--normalize", nargs="?", const=True, default=True, type=parse_bool)
    parser.add_argument("--include-uppercase", nargs="?", const=True, default=True, type=parse_bool)
    parser.add_argument("--include-digits", nargs="?", const=True, default=True, type=parse_bool)
    args = parser.parse_args()

    chars = character_list(args)
    try:
        raw_glyphs, source = load_hershey_raw_glyphs(chars, args.font)
    except ImportError:
        raw_glyphs, source = fallback_raw_glyphs(chars)
    except RuntimeError as exc:
        sys.stderr.write(str(exc) + "\n")
        return 2

    library = build_library(raw_glyphs, source, args.font, args.normalize)

    output_dir = os.path.dirname(os.path.abspath(args.output))
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(library, f, indent=2)
        f.write("\n")

    print("Wrote {} glyphs to {}".format(len(library["glyphs"]), args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())

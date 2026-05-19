#!/usr/bin/env python3
"""Export Hershey-Fonts text as local 2D stroke JSON.

This is a preprocessing tool, not a build step. When Hershey-Fonts is
available it uses that package directly. If the package is missing, it emits
a clear warning and writes a tiny fallback vector path so the C++ app and JSON
loader can still be exercised.
"""

import argparse
import json
import math
import os
import sys


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


def normalize_strokes(strokes, scale, normalize):
    raw_bounds = bounds_for_strokes(strokes)
    min_x, min_y = raw_bounds["min"]
    width = raw_bounds["width"]
    height = raw_bounds["height"]
    denominator = max(height, width, 1e-6) if normalize else 1.0

    normalized = []
    for stroke in strokes:
        points = []
        for x, y in stroke["points"]:
            if normalize:
                points.append([(x - min_x) / denominator * scale, (y - min_y) / denominator * scale])
            else:
                points.append([x * scale, y * scale])
        if len(points) >= 2:
            normalized.append({"closed": bool(stroke.get("closed", False)), "points": points})
    return normalized


def resample_stroke_points(points, spacing):
    if spacing <= 0.0 or len(points) < 2:
        return points

    sampled = [points[0]]
    for index in range(len(points) - 1):
        start = points[index]
        end = points[index + 1]
        segment_length = distance(start, end)
        sample_count = max(1, int(math.ceil(segment_length / spacing)))
        for sample_index in range(1, sample_count + 1):
            t = sample_index / float(sample_count)
            sampled.append([
                start[0] * (1.0 - t) + end[0] * t,
                start[1] * (1.0 - t) + end[1] * t,
            ])
    return sampled


def resample_strokes(strokes, spacing):
    if spacing <= 0.0:
        return strokes
    result = []
    for stroke in strokes:
        result.append({
            "closed": bool(stroke.get("closed", False)),
            "points": resample_stroke_points(stroke["points"], spacing),
        })
    return result


def load_hershey_lines_for_text(text, font_name, character_spacing, word_spacing):
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

    strokes = []
    cursor_x = 0.0
    for character in text:
        if character == " ":
            cursor_x += word_spacing
            continue

        segments = [unpack_segment(segment) for segment in font.lines_for_text(character)]
        if not segments:
            cursor_x += character_spacing
            continue

        min_x = min(min(start[0], end[0]) for start, end in segments)
        max_x = max(max(start[0], end[0]) for start, end in segments)
        for start, end in segments:
            shifted_start = (start[0] - min_x + cursor_x, start[1])
            shifted_end = (end[0] - min_x + cursor_x, end[1])
            append_segment_as_stroke(strokes, shifted_start, shifted_end)

        cursor_x += max(max_x - min_x, 0.0) + character_spacing

    return strokes, "hershey-fonts"


def fallback_glyph_strokes(text, character_spacing, word_spacing):
    sys.stderr.write(
        "Warning: Hershey-Fonts package is not installed. "
        "Install with: pip install Hershey-Fonts\n"
        "Writing fallback sample strokes instead.\n"
    )

    glyphs = {
        "a": [
            [[0.45, 0.05], [0.2, 0.0], [0.0, 0.25], [0.08, 0.55], [0.35, 0.65], [0.55, 0.42], [0.45, 0.05]],
            [[0.52, 0.60], [0.55, 0.18], [0.68, 0.02]],
        ],
        "b": [
            [[0.0, 0.0], [0.0, 0.9]],
            [[0.0, 0.5], [0.28, 0.68], [0.58, 0.48], [0.52, 0.18], [0.20, 0.04], [0.0, 0.18]],
        ],
        "c": [
            [[0.6, 0.52], [0.38, 0.70], [0.08, 0.55], [0.0, 0.25], [0.22, 0.02], [0.55, 0.12]],
        ],
    }

    strokes = []
    cursor_x = 0.0
    for character in text.lower():
        if character == " ":
            cursor_x += word_spacing
            continue
        character_strokes = glyphs.get(character)
        if character_strokes is None:
            character_strokes = [[[0.0, 0.0], [0.0, 0.6], [0.36, 0.6], [0.36, 0.0], [0.0, 0.0]]]
        max_x = 0.0
        for stroke_points in character_strokes:
            points = [[point[0] + cursor_x, point[1]] for point in stroke_points]
            max_x = max(max_x, max(point[0] for point in points) - cursor_x)
            strokes.append({"closed": False, "points": points})
        cursor_x += max_x + character_spacing

    return strokes, "fallback-no-hershey-fonts"


def main():
    parser = argparse.ArgumentParser(description="Generate local 2D Hershey text path JSON.")
    parser.add_argument("--text", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--font", default="futural")
    parser.add_argument("--scale", type=float, default=1.0)
    parser.add_argument("--character-spacing", type=float, default=2.0)
    parser.add_argument("--word-spacing", type=float, default=8.0)
    parser.add_argument("--sample-spacing", type=float, default=0.0)
    parser.add_argument("--normalize", nargs="?", const=True, default=True, type=parse_bool)
    args = parser.parse_args()

    try:
        strokes, source = load_hershey_lines_for_text(
            args.text,
            args.font,
            args.character_spacing,
            args.word_spacing,
        )
    except ImportError:
        strokes, source = fallback_glyph_strokes(args.text, args.character_spacing, args.word_spacing)
    except RuntimeError as exc:
        sys.stderr.write(str(exc) + "\n")
        return 2

    strokes = normalize_strokes(strokes, args.scale, args.normalize)
    strokes = resample_strokes(strokes, args.sample_spacing)
    bounds = bounds_for_strokes(strokes)

    output = {
        "source": source,
        "font": args.font,
        "text": args.text,
        "coordinate_system": "local_2d_xz",
        "units": "normalized" if args.normalize else "raw_scaled",
        "bounds": bounds,
        "strokes": strokes,
    }

    output_dir = os.path.dirname(os.path.abspath(args.output))
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2)
        f.write("\n")

    print("Wrote {} strokes to {}".format(len(strokes), args.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())

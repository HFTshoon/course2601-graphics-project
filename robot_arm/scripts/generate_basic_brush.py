#!/usr/bin/env python3
"""Generate the placeholder brush image used by StrokeRenderer.

The generated asset is intentionally simple and replaceable:
assets/brushes/basic_circle.png can be swapped with another RGBA brush image.
"""

import os
import struct
import zlib


def png_chunk(chunk_type, data):
    return (
        struct.pack(">I", len(data))
        + chunk_type
        + data
        + struct.pack(">I", zlib.crc32(chunk_type + data) & 0xFFFFFFFF)
    )


def write_png_rgba(path, width, height, pixels):
    raw_rows = []
    stride = width * 4
    for y in range(height):
        raw_rows.append(b"\x00" + pixels[y * stride : (y + 1) * stride])

    png = [
        b"\x89PNG\r\n\x1a\n",
        png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)),
        png_chunk(b"IDAT", zlib.compress(b"".join(raw_rows), 9)),
        png_chunk(b"IEND", b""),
    ]

    with open(path, "wb") as f:
        f.write(b"".join(png))


def smoothstep(edge0, edge1, x):
    if edge0 == edge1:
        return 0.0
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def main():
    width = 256
    height = 256
    center = (width - 1) * 0.5
    radius = width * 0.46
    hard_core = radius * 0.35

    pixels = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            dx = x - center
            dy = y - center
            distance = (dx * dx + dy * dy) ** 0.5
            fade = 1.0 - smoothstep(hard_core, radius, distance)
            alpha = int(max(0.0, min(1.0, fade)) * 255)
            i = (y * width + x) * 4
            pixels[i + 0] = 0
            pixels[i + 1] = 0
            pixels[i + 2] = 0
            pixels[i + 3] = alpha

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    output_dir = os.path.join(project_dir, "assets", "brushes")
    os.makedirs(output_dir, exist_ok=True)
    write_png_rgba(os.path.join(output_dir, "basic_circle.png"), width, height, pixels)


if __name__ == "__main__":
    main()

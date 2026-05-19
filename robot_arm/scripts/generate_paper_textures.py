#!/usr/bin/env python3
"""Generate placeholder paper albedo textures.

These PNGs are intentionally simple and replaceable. Swap the files under
assets/papers/ with higher-quality scanned or procedural paper textures later.
"""

import math
import os
import random
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
    stride = width * 4
    rows = []
    for y in range(height):
        rows.append(b"\x00" + pixels[y * stride : (y + 1) * stride])

    png = [
        b"\x89PNG\r\n\x1a\n",
        png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)),
        png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9)),
        png_chunk(b"IEND", b""),
    ]
    with open(path, "wb") as f:
        f.write(b"".join(png))


def clamp_byte(value):
    return max(0, min(255, int(round(value))))


def hash_noise(x, y, seed):
    value = math.sin(x * 127.1 + y * 311.7 + seed * 74.7) * 43758.5453
    return value - math.floor(value)


def value_noise(x, y, scale, seed):
    sx = x / scale
    sy = y / scale
    ix = math.floor(sx)
    iy = math.floor(sy)
    fx = sx - ix
    fy = sy - iy
    fx = fx * fx * (3.0 - 2.0 * fx)
    fy = fy * fy * (3.0 - 2.0 * fy)

    a = hash_noise(ix, iy, seed)
    b = hash_noise(ix + 1, iy, seed)
    c = hash_noise(ix, iy + 1, seed)
    d = hash_noise(ix + 1, iy + 1, seed)
    return (a * (1.0 - fx) + b * fx) * (1.0 - fy) + (c * (1.0 - fx) + d * fx) * fy


def make_texture(width, height, base, fine_noise, fiber_strength, warm_speckles, seed):
    rng = random.Random(seed)
    speckles = []
    for _ in range(warm_speckles):
        speckles.append((
            rng.randrange(width),
            rng.randrange(height),
            rng.uniform(1.5, 5.5),
            rng.uniform(-18.0, 14.0),
        ))

    pixels = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            broad = value_noise(x, y, 42.0, seed) - 0.5
            fine = hash_noise(x, y, seed + 13) - 0.5
            horizontal_fiber = math.sin(y * 0.20 + value_noise(x, y, 28.0, seed + 5) * 8.0)
            vertical_fiber = math.sin(x * 0.045 + value_noise(x, y, 70.0, seed + 8) * 6.0)
            fiber = (horizontal_fiber * 0.65 + vertical_fiber * 0.35) * fiber_strength
            variation = broad * fine_noise * 0.8 + fine * fine_noise + fiber

            r, g, b = base
            r += variation
            g += variation
            b += variation

            for sx, sy, radius, tint in speckles:
                dx = x - sx
                dy = y - sy
                d2 = dx * dx + dy * dy
                if d2 < radius * radius:
                    amount = (1.0 - math.sqrt(d2) / radius) * tint
                    r += amount * 1.05
                    g += amount * 0.92
                    b += amount * 0.62

            i = (y * width + x) * 4
            pixels[i + 0] = clamp_byte(r)
            pixels[i + 1] = clamp_byte(g)
            pixels[i + 2] = clamp_byte(b)
            pixels[i + 3] = 255

    return pixels


def main():
    width = 512
    height = 512
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.dirname(script_dir)
    output_dir = os.path.join(project_dir, "assets", "papers")
    os.makedirs(output_dir, exist_ok=True)

    textures = [
        ("smooth_paper.png", (246, 245, 238), 2.2, 0.3, 4, 11),
        ("rough_paper.png", (214, 214, 207), 11.0, 8.0, 28, 23),
        ("recycled_paper.png", (216, 199, 150), 8.0, 4.7, 44, 37),
    ]

    for filename, base, fine_noise, fiber_strength, speckles, seed in textures:
        pixels = make_texture(width, height, base, fine_noise, fiber_strength, speckles, seed)
        write_png_rgba(os.path.join(output_dir, filename), width, height, pixels)


if __name__ == "__main__":
    main()

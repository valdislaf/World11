#!/usr/bin/env python3
"""Generate the World 11 tube sponge surface texture (asset 0x1D).

A seamless 512x512 tile: rounded knobs (periodic Worley cells) separated
by darker creases, scattered pores and slow orange-to-yellow variation.
The tube mesh repeats it around and along each tube.

Writes assets/world11_tube_sponge.png and packs datasets/0x0000001D.fget.
Deterministic and dependency-free, like scripts/assets.py.
"""
import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SIZE = 512
CELLS = 14
FGET_KEY = 0x2D
PNG_NAME = 'world11_tube_sponge.png'
FGET_NAME = '0x0000001D.fget'

ORANGE = (226, 128, 26)
YELLOW = (246, 190, 52)
CREASE = (120, 58, 14)
PORE = (70, 34, 10)


def hash_unit(x, y, salt):
    h = (x * 374761393 + y * 668265263 + salt * 2246822519) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65536.0


def smoothstep(edge0, edge1, value):
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def mix(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def periodic_value_noise(u, v, period, salt):
    """Bilinear value noise that tiles every `period` lattice cells."""
    x = u * period
    y = v * period
    x0 = int(math.floor(x))
    y0 = int(math.floor(y))
    fx = x - x0
    fy = y - y0
    sx = fx * fx * (3.0 - 2.0 * fx)
    sy = fy * fy * (3.0 - 2.0 * fy)

    def corner(i, j):
        return hash_unit((x0 + i) % period, (y0 + j) % period, salt)

    top = corner(0, 0) + (corner(1, 0) - corner(0, 0)) * sx
    bottom = corner(0, 1) + (corner(1, 1) - corner(0, 1)) * sx
    return top + (bottom - top) * sy


def main():
    # One feature point per cell, jittered; wrapping makes the tile seamless.
    points = {}
    for cy in range(CELLS):
        for cx in range(CELLS):
            points[(cx, cy)] = (
                (cx + 0.05 + 0.9 * hash_unit(cx, cy, 1)) / CELLS,
                (cy + 0.05 + 0.9 * hash_unit(cx, cy, 2)) / CELLS,
                hash_unit(cx, cy, 3))

    stride = SIZE * 4
    pixels = bytearray(SIZE * stride)
    for y in range(SIZE):
        v = (y + 0.5) / SIZE
        cell_y = int(v * CELLS)
        for x in range(SIZE):
            u = (x + 0.5) / SIZE
            cell_x = int(u * CELLS)
            nearest = second = 9.0
            nearest_seed = 0.0
            for oy in (-1, 0, 1):
                for ox in (-1, 0, 1):
                    px, py, seed = points[((cell_x + ox) % CELLS,
                                           (cell_y + oy) % CELLS)]
                    # Shift the wrapped point next to this pixel.
                    px += (cell_x + ox - (cell_x + ox) % CELLS) / CELLS
                    py += (cell_y + oy - (cell_y + oy) % CELLS) / CELLS
                    d = math.hypot(u - px, v - py) * CELLS
                    if d < nearest:
                        second = nearest
                        nearest, nearest_seed = d, seed
                    elif d < second:
                        second = d

            tone = periodic_value_noise(u, v, 4, 7) * 0.7 + \
                periodic_value_noise(u, v, 16, 9) * 0.3
            base = mix(ORANGE, YELLOW, smoothstep(0.25, 0.8, tone))
            # Knob dome: brighter centre, shading falls off towards creases.
            dome = 1.0 - smoothstep(0.0, 0.9, nearest)
            color = tuple(c * (0.72 + 0.40 * dome * dome) for c in base)
            crease = smoothstep(0.34, 0.0, second - nearest)
            color = mix(color, CREASE, crease * crease * 0.55)
            # Pores sit in roughly a fifth of the knobs.
            if nearest_seed < 0.22:
                pore = smoothstep(0.13, 0.05, nearest)
                color = mix(color, PORE, pore * 0.8)
            grain = hash_unit(x, y, 11) - 0.5
            color = tuple(c * (1.0 + grain * 0.08) for c in color)

            index = y * stride + x * 4
            for channel in range(3):
                pixels[index + channel] = int(max(0.0, min(255.0, color[channel])) + 0.5)
            pixels[index + 3] = 255

    def chunk(kind, data):
        payload = kind + data
        return (struct.pack('>I', len(data)) + payload +
                struct.pack('>I', zlib.crc32(payload) & 0xFFFFFFFF))

    raw = b''.join(b'\x00' + bytes(pixels[row * stride:(row + 1) * stride])
                   for row in range(SIZE))
    png = (b'\x89PNG\r\n\x1a\n' +
           chunk(b'IHDR', struct.pack('>IIBBBBB', SIZE, SIZE, 8, 6, 0, 0, 0)) +
           chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

    png_path = ROOT / 'assets' / PNG_NAME
    png_path.parent.mkdir(parents=True, exist_ok=True)
    png_path.write_bytes(png)
    packed = b'FGET' + bytes([FGET_KEY]) + bytes(b ^ FGET_KEY for b in png)
    fget_path = ROOT / 'datasets' / FGET_NAME
    fget_path.write_bytes(packed)
    print(f'{png_path.relative_to(ROOT)}: {len(png)} bytes')
    print(f'{fget_path.relative_to(ROOT)}: {len(packed)} bytes')


if __name__ == '__main__':
    main()

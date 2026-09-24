#!/usr/bin/env python3
"""Generate the World 11 reef butterflyfish texture (asset 0x1C).

The image is a side view with the head on the left, like world11_fish_01.png:
u = 0 is the snout, v = 0 is the top. The body outline comes from the same
knot tables as cpp/render/gl33/World11ReefFishGeometry.cpp, so the painted
body lines up with the procedural mesh. Keep both tables identical.

Alpha: 255 on the body, partial on fins (cutout at 0.30 in the shader),
0 elsewhere. The lower-left corner holds the pectoral-fin atlas.

Writes assets/world11_fish_02.png and packs datasets/0x0000001C.fget.
Deterministic and dependency-free, like scripts/assets.py.
"""
import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WIDTH = 1024
HEIGHT = 512
FGET_KEY = 0x6B
PNG_NAME = 'world11_fish_02.png'
FGET_NAME = '0x0000001C.fget'

# (u, top v, bottom v) of the body outline; mirrored in World11ReefFishGeometry.cpp.
BODY_KNOTS = [
    (0.030, 0.505, 0.535),
    (0.060, 0.455, 0.575),
    (0.120, 0.360, 0.650),
    (0.200, 0.255, 0.740),
    (0.300, 0.180, 0.805),
    (0.420, 0.150, 0.830),
    (0.540, 0.175, 0.800),
    (0.640, 0.245, 0.735),
    (0.720, 0.345, 0.640),
    (0.790, 0.425, 0.570),
    (0.840, 0.440, 0.560),
]
DORSAL_KNOTS = [
    (0.250, 0.215), (0.320, 0.100), (0.420, 0.055), (0.550, 0.060),
    (0.660, 0.110), (0.740, 0.200), (0.800, 0.330), (0.840, 0.430),
]
VENTRAL_KNOTS = [
    (0.460, 0.820), (0.520, 0.900), (0.620, 0.925), (0.720, 0.860),
    (0.790, 0.720), (0.840, 0.575),
]
PELVIC = (0.255, 0.365, 0.905)  # start u, end u, lowest v of the pelvic spine
TAIL_START, TAIL_END = 0.820, 0.975
PECTORAL_ATLAS = (0.020, 0.180, 0.840, 0.980)  # u0, u1, v0, v1


def hermite(knots, u):
    """Piecewise cubic Hermite with Catmull-Rom tangents (clamped ends)."""
    us = [k[0] for k in knots]
    vs = [k[1] for k in knots]
    if u <= us[0]:
        return vs[0]
    if u >= us[-1]:
        return vs[-1]
    i = 0
    while us[i + 1] < u:
        i += 1

    def slope(j):
        lo = max(j - 1, 0)
        hi = min(j + 1, len(us) - 1)
        return (vs[hi] - vs[lo]) / (us[hi] - us[lo])

    h = us[i + 1] - us[i]
    t = (u - us[i]) / h
    t2, t3 = t * t, t * t * t
    return ((2 * t3 - 3 * t2 + 1) * vs[i] + (t3 - 2 * t2 + t) * h * slope(i) +
            (-2 * t3 + 3 * t2) * vs[i + 1] + (t3 - t2) * h * slope(i + 1))


def body_edges(u):
    top = hermite([(k[0], k[1]) for k in BODY_KNOTS], u)
    bottom = hermite([(k[0], k[2]) for k in BODY_KNOTS], u)
    return top, bottom


def clamp(value, low=0.0, high=1.0):
    return low if value < low else high if value > high else value


def smoothstep(edge0, edge1, value):
    t = clamp((value - edge0) / (edge1 - edge0))
    return t * t * (3.0 - 2.0 * t)


def mix(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def noise(x, y):
    """Integer hash in [0, 1) for fine scale speckle."""
    h = (x * 374761393 + y * 668265263) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 65536.0


YELLOW = (250, 206, 38)
GOLD = (238, 172, 24)
BELLY = (252, 238, 176)
INK = (18, 16, 22)
WHITE = (246, 244, 236)
EDGE_BLUE = (70, 120, 190)
FIN_CLEAR = (236, 222, 150)


def paint():
    pixels = bytearray(WIDTH * HEIGHT * 4)
    columns = []
    for x in range(WIDTH):
        u = (x + 0.5) / WIDTH
        top, bottom = body_edges(u)
        body = 0.030 <= u <= 0.840
        dorsal = hermite(DORSAL_KNOTS, u) if 0.25 <= u <= 0.84 else None
        ventral = hermite(VENTRAL_KNOTS, u) if 0.46 <= u <= 0.84 else None
        columns.append((u, top, bottom, body, dorsal, ventral))

    pu0, pu1, pv0, pv1 = PECTORAL_ATLAS
    eye_x, eye_y, eye_r = 0.125 * WIDTH, 0.430 * HEIGHT, 13.0
    spot_x, spot_y = 0.640 * WIDTH, 0.300 * HEIGHT

    for y in range(HEIGHT):
        v = (y + 0.5) / HEIGHT
        row = y * WIDTH * 4
        for x in range(WIDTH):
            u, top, bottom, in_span, dorsal, ventral = columns[x]
            speckle = noise(x, y) - 0.5
            color = FIN_CLEAR
            alpha = 0.0

            # Body with vertical anti-aliasing in pixels.
            coverage = 0.0
            if in_span:
                coverage = clamp(min(v - top, bottom - v) * HEIGHT + 0.5)
                coverage *= clamp((u - 0.030) * WIDTH + 0.5)
            if coverage > 0.0:
                depth = (v - top) / max(bottom - top, 1e-4)
                base = mix(GOLD, YELLOW, smoothstep(0.0, 0.35, depth))
                base = mix(base, BELLY, smoothstep(0.62, 0.98, depth) * 0.75)
                # Diagonal chevrons of darker gold across the flank.
                stripe = (u * 38.0 + v * 21.0) % 1.0
                chevron = smoothstep(0.42, 0.47, abs(stripe - 0.5))
                chevron *= smoothstep(0.20, 0.30, u) * smoothstep(0.76, 0.66, u)
                base = mix(base, (206, 138, 18), chevron * 0.55)
                # Dark snout and a slanted eye bar with a white trailing band.
                base = mix(base, (176, 132, 44), smoothstep(0.07, 0.035, u) * 0.8)
                bar_center = 0.125 + (v - 0.43) * 0.10
                bar = abs(u - bar_center) * WIDTH
                base = mix(base, WHITE, smoothstep(30.0, 26.0, bar) *
                           smoothstep(15.0, 19.0, bar))
                base = mix(base, INK, smoothstep(19.0, 16.0, bar))
                # Eye: white iris ring around a black pupil inside the bar.
                eye = math.hypot(x + 0.5 - eye_x, y + 0.5 - eye_y)
                base = mix(base, (224, 218, 196), smoothstep(eye_r + 1.0, eye_r, eye) *
                           smoothstep(eye_r * 0.55, eye_r * 0.7, eye))
                # Ocellus: dark false eye with a pale ring near the dorsal base.
                spot = math.hypot(x + 0.5 - spot_x, (y + 0.5 - spot_y) * 1.15)
                base = mix(base, WHITE, smoothstep(24.0, 21.0, spot))
                base = mix(base, INK, smoothstep(19.0, 16.5, spot))
                shade = 1.0 + speckle * 0.07
                color = tuple(c * shade for c in base)
                alpha = coverage

            if alpha < 1.0:
                fin_color, fin_alpha = None, 0.0
                # Dorsal fin: gold rays, dark submarginal line, blue rim.
                if dorsal is not None and dorsal <= v <= top + 0.02:
                    edge = (v - dorsal) * HEIGHT
                    rays = 0.88 + 0.12 * abs(math.sin(u * 150.0))
                    fin_color = mix(YELLOW, GOLD, smoothstep(top, dorsal, v))
                    fin_color = mix(fin_color, INK, smoothstep(9.0, 6.0, edge) *
                                    smoothstep(2.5, 4.5, edge))
                    fin_color = mix(fin_color, EDGE_BLUE, smoothstep(3.0, 1.0, edge))
                    fin_alpha = 0.92 * rays * clamp(edge + 0.5)
                # Anal fin continues the dorsal pattern below the body.
                elif ventral is not None and bottom - 0.02 <= v <= ventral:
                    edge = (ventral - v) * HEIGHT
                    rays = 0.88 + 0.12 * abs(math.sin(u * 150.0))
                    fin_color = mix(YELLOW, GOLD, smoothstep(bottom, ventral, v))
                    fin_color = mix(fin_color, INK, smoothstep(9.0, 6.0, edge) *
                                    smoothstep(2.5, 4.5, edge))
                    fin_color = mix(fin_color, EDGE_BLUE, smoothstep(3.0, 1.0, edge))
                    fin_alpha = 0.92 * rays * clamp(edge + 0.5)
                # Pelvic spine: a narrow white-tipped blade under the chest.
                elif PELVIC[0] <= u <= PELVIC[1] and v >= bottom - 0.02:
                    t = (u - PELVIC[0]) / (PELVIC[1] - PELVIC[0])
                    reach = bottom + (PELVIC[2] - bottom) * math.sin(t * math.pi * 0.85)
                    if v <= reach:
                        fin_color = mix(YELLOW, WHITE, smoothstep(bottom, reach, v))
                        fin_alpha = 0.85 * clamp((reach - v) * HEIGHT + 0.5)
                # Caudal fin: rounded fan, clear edge, one dusky band.
                elif TAIL_START <= u:
                    spread = 0.06 + 0.19 * smoothstep(TAIL_START, TAIL_END - 0.02, u)
                    offset = abs(v - 0.5)
                    end = TAIL_END - 0.02 * (offset / 0.25) ** 2
                    if offset <= spread and u <= end:
                        edge = min((spread - offset) * HEIGHT, (end - u) * WIDTH)
                        rays = 0.72 + 0.28 * abs(math.sin(math.atan2(v - 0.5, u - 0.80) * 18.0))
                        fin_color = mix(YELLOW, FIN_CLEAR, smoothstep(0.84, 0.95, u))
                        band = smoothstep(0.012, 0.004, abs(u - 0.875))
                        fin_color = mix(fin_color, (120, 96, 40), band * 0.6)
                        fin_alpha = rays * (0.95 - 0.35 * smoothstep(0.90, 0.97, u))
                        fin_alpha *= clamp(edge + 0.5)
                # Pectoral fin atlas (drawn on flared quads by the mesh).
                elif pu0 <= u <= pu1 and pv0 <= v <= pv1:
                    s = (u - pu0) / (pu1 - pu0)
                    t = (v - 0.5 * (pv0 + pv1)) / (0.5 * (pv1 - pv0))
                    width = 0.35 + 0.60 * math.sin(clamp(s) * math.pi * 0.75)
                    tip = 1.0 - 0.18 * t * t
                    if abs(t) <= width and s <= tip:
                        edge = min((width - abs(t)) * 0.5 * (pv1 - pv0) * HEIGHT,
                                   (tip - s) * (pu1 - pu0) * WIDTH)
                        rays = 0.70 + 0.30 * abs(math.sin(math.atan2(t, s + 0.2) * 9.0))
                        fin_color = mix(YELLOW, FIN_CLEAR, s)
                        fin_alpha = 0.85 * rays * clamp(edge + 0.5)

                if fin_color is not None and fin_alpha > 0.0:
                    shade = 1.0 + speckle * 0.05
                    fin_color = tuple(c * shade for c in fin_color)
                    keep = alpha
                    color = mix(fin_color, color, keep)
                    alpha = keep + fin_alpha * (1.0 - keep)

            index = row + x * 4
            pixels[index] = int(clamp(color[0], 0, 255) + 0.5)
            pixels[index + 1] = int(clamp(color[1], 0, 255) + 0.5)
            pixels[index + 2] = int(clamp(color[2], 0, 255) + 0.5)
            pixels[index + 3] = int(clamp(alpha) * 255.0 + 0.5)
    return pixels


def encode_png(pixels):
    def chunk(kind, data):
        payload = kind + data
        return (struct.pack('>I', len(data)) + payload +
                struct.pack('>I', zlib.crc32(payload) & 0xFFFFFFFF))

    stride = WIDTH * 4
    raw = b''.join(b'\x00' + bytes(pixels[y * stride:(y + 1) * stride])
                   for y in range(HEIGHT))
    header = struct.pack('>IIBBBBB', WIDTH, HEIGHT, 8, 6, 0, 0, 0)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) +
            chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def main():
    png = encode_png(paint())
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

#!/usr/bin/env python3
"""Unpack/edit/repack World11 PNG assets without third-party dependencies."""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = {
    '17': 'day_ocean.png',
    '18': 'world11_coral_tissue.png',
    '19': 'world11_fish_01.png',
    '1A': 'world11_fish_01_body_mask.png',
    '1B': 'sand_bottom_ocean.png',
}
PNG_MAGIC = b'\x89PNG\r\n\x1a\n'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['unpack', 'pack'])
    args = parser.parse_args()
    pending = []
    for asset_id, name in ASSETS.items():
        packed_path = ROOT / 'datasets' / f'0x000000{asset_id}.fget'
        png_path = ROOT / 'assets' / name
        packed = packed_path.read_bytes()
        if len(packed) <= 5 or packed[:4] != b'FGET':
            raise ValueError(f'Invalid FGET: {packed_path.name}')
        key = packed[4]
        if args.action == 'unpack':
            png = bytes(value ^ key for value in packed[5:])
            target, result = png_path, png
        else:
            png = png_path.read_bytes()
            target = packed_path
            result = packed[:5] + bytes(value ^ key for value in png)
        if not png.startswith(PNG_MAGIC):
            raise ValueError(f'Invalid PNG: {name}')
        pending.append((target, result))
    for target, result in pending:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(result)
        print(f'{target.relative_to(ROOT)}: {len(result)} bytes')

if __name__ == '__main__':
    main()

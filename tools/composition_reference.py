"""Prepare a photo for the Z 7II SD composition reference menu."""

import argparse
from pathlib import Path
import struct

from PIL import Image, ImageOps


WIDTH, HEIGHT, STRIDE = 360, 240, 368


def encode(source):
    with Image.open(source) as image:
        image = ImageOps.exif_transpose(image).convert('RGBA')
        scale = min(WIDTH / image.width, HEIGHT / image.height)
        size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
        fitted = image.resize(size, Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', (STRIDE, HEIGHT))
    canvas.paste(fitted, ((WIDTH - fitted.width) // 2, (HEIGHT - fitted.height) // 2))
    return struct.pack('<4s3I112x', b'ZRF1', WIDTH, HEIGHT, STRIDE) + canvas.tobytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path, help='Input JPEG, PNG or another Pillow-readable image')
    parser.add_argument('--slot', type=int, choices=range(1, 7), default=1)
    parser.add_argument('--output-dir', type=Path, default=Path.cwd())
    args = parser.parse_args()
    output = args.output_dir / f'Z72REF{args.slot}.RFG'
    try:
        data = encode(args.image)
        with output.open('xb') as handle:
            handle.write(data)
    except (OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    print(output.resolve())


if __name__ == '__main__':
    main()

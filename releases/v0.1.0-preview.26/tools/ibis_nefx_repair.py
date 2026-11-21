# SPDX-License-Identifier: MIT
"""Create NX Studio-compatible copies of merged Z7 II NEFX files."""

import argparse
from pathlib import Path
from shutil import copyfileobj
import struct
from tempfile import TemporaryDirectory

ENTRY = struct.Struct("<HHII")
NIKON_NOTE = b"Nikon\0\x02\x11\0\0II*\0\x08\0\0\0"


def read_at(file, offset, size):
    file.seek(offset)
    value = file.read(size)
    if len(value) != size:
        raise ValueError("Truncated NEF metadata")
    return value


def file_ifd(file, offset):
    count, = struct.unpack("<H", read_at(file, offset, 2))
    return {tag: (kind, count, value) for tag, kind, count, value in
            ENTRY.iter_unpack(read_at(file, offset + 2, count * ENTRY.size))}


def compression_offset(file):
    header = read_at(file, 0, 8)
    if header[:4] != b"II*\0":
        raise ValueError("Expected a little-endian Nikon NEFX")
    root = file_ifd(file, struct.unpack_from("<I", header, 4)[0])
    kind, size, offset = root[0x110]
    if (kind, size) != (2, 12) or read_at(file, offset, size) != b"NIKON Z 7_2\0":
        raise ValueError("Expected a Nikon Z7 II file")

    kind, count, offset = root[0x14a]
    if kind != 4 or not 2 <= count <= 8:
        raise ValueError("Unexpected NEFX image directory layout")
    sensor = file_ifd(file, struct.unpack("<I", read_at(file, offset + 4, 4))[0])
    width, height = sensor[0x100][2], sensor[0x101][2]
    if (width, height) not in ((8256, 5504), (16512, 11008)):
        raise ValueError("Unexpected Z7 II NEFX dimensions")
    if any(sensor.get(tag) != entry for tag, entry in {
        0xfe: (4, 1, 0), 0x103: (3, 1, 34713), 0x106: (3, 1, 2),
        0x115: (3, 1, 3), 0x116: (4, 1, height),
        0x117: (4, 1, width * height * 3 * 14 // 8), 0x11c: (3, 1, 1),
    }.items()):
        raise ValueError("Expected a single packed RGB strip")
    kind, count, offset = sensor[0x102]
    if (kind, count) != (3, 3) or read_at(file, offset, 6) != b"\x0e\0" * 3:
        raise ValueError("Expected three 14-bit channels")
    kind, count, strip = sensor[0x111]
    file_size = file.seek(0, 2)
    if (kind, count) != (4, 1) or strip < 8 or strip + sensor[0x117][2] > file_size:
        raise ValueError("Incomplete NEFX pixel data")

    exif = file_ifd(file, root[0x8769][2])
    kind, note_size, note = exif[0x927c]
    if (kind != 7 or not len(NIKON_NOTE) + 6 <= note_size <= strip - note
            or read_at(file, note, len(NIKON_NOTE)) != NIKON_NOTE):
        raise ValueError("Unexpected Nikon MakerNote")
    ifd = note + len(NIKON_NOTE)
    count, = struct.unpack("<H", read_at(file, ifd, 2))
    directory_size = 2 + count * ENTRY.size + 4
    if len(NIKON_NOTE) + directory_size > note_size:
        raise ValueError("Incomplete Nikon MakerNote directory")
    entries = list(ENTRY.iter_unpack(read_at(file, ifd + 2, count * ENTRY.size)))
    fields = {tag: (kind, count, value) for tag, kind, count, value in entries}

    def note_field(tag, size):
        kind, count, offset = fields[tag]
        offset += 10
        if (kind, count) != (7, size) or not len(NIKON_NOTE) + directory_size <= offset <= note_size - size:
            raise ValueError("Unexpected Nikon pixel shift metadata")
        return read_at(file, note + offset, size)

    pixel_shift = note_field(0x0e2d, 16)
    if pixel_shift[:5] != b"0100\x02" or pixel_shift[6] not in (1, 2, 3, 4):
        raise ValueError("Expected a merged pixel shift NEFX")
    compression = note_field(0x51, 24)
    if compression[:4] not in (b"0101", b"0102") or compression[10:12] != b"\x0a\0":
        raise ValueError("Missing packed 14-bit compression marker")

    for index, (tag, kind, count, value) in enumerate(entries):
        if tag == 0x93:
            if (kind, count) != (3, 1) or value not in (3, 10):
                raise ValueError("Unexpected legacy compression marker")
            return ifd + 2 + index * ENTRY.size + 8, value
    raise ValueError("Missing legacy compression marker")


def repair(source):
    if source.suffix.lower() != ".nefx":
        raise ValueError("Select a merged .NEFX, not the source photos")
    target = source.with_name(f"{source.stem}_NX_compatible.NEFX")
    with source.open("rb") as raw:
        offset, compression = compression_offset(raw)
        if compression == 10:
            return None
        if target.exists():
            raise FileExistsError(f"Output already exists: {target.name}")
        with TemporaryDirectory(prefix="z72-nefx-", dir=target.parent) as temp:
            copy = Path(temp) / target.name
            with copy.open("w+b") as edited:
                raw.seek(0)
                copyfileobj(raw, edited, length=1024 * 1024)
                if read_at(edited, offset, 2) != b"\x03\0":
                    raise ValueError("Legacy compression marker changed during copy")
                edited.seek(offset)
                edited.write(b"\x0a")
            if target.exists():
                raise FileExistsError(f"Output already exists: {target.name}")
            copy.rename(target)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", type=Path, help="merged NEFX files or folders containing them")
    args = parser.parse_args()
    failed = False
    for path in args.inputs:
        sources = [path]
        if path.is_dir():
            sources = sorted(item for item in path.iterdir()
                             if item.is_file() and item.suffix.lower() == ".nefx"
                             and not item.stem.lower().endswith("_nx_compatible"))
        if not sources:
            print(f"{path}: no merged NEFX files")
            failed = True
        for source in sources:
            try:
                output = repair(source)
                print(f"Created {output}" if output else f"{source}: already compatible")
            except (KeyError, OSError, ValueError, struct.error) as error:
                print(f"{source}: {error}")
                failed = True
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())

"""Build a relocatable native Thumb C module using LLVM, without vendor bytes."""
import argparse
import binascii
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TARGET = 0x01700072
HEADER = 64
MAX_FILE = 65536
MAX_MEMORY = 131072


def seal(data):
    data = bytearray(data)
    data[32:36] = bytes(4)
    struct.pack_into('<I', data, 32, binascii.crc32(data))
    return bytes(data)


def from_elf(data):
    if data[:7] != b'\x7fELF\x01\x01\x01' or struct.unpack_from('<H', data, 18)[0] != 40:
        raise ValueError('Expected little-endian ELF32 ARM')
    entry, _, shoff = struct.unpack_from('<III', data, 24)
    shsize, count = struct.unpack_from('<HH', data, 46)
    if shsize != 40:
        raise ValueError('Unexpected section format')
    sections = [struct.unpack_from('<10I', data, shoff + i * 40) for i in range(count)]
    symbols = {}
    for section in sections:
        if section[1] != 2:
            continue
        strings = sections[section[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        for off in range(section[4], section[4] + section[5], 16):
            name, value, _, _, _, index = struct.unpack_from('<IIIBBH', data, off)
            label = names[name:].split(b'\0', 1)[0].decode()
            if index == 0 and label:
                raise ValueError('Undefined module symbol: ' + label)
            symbols[label] = value
    image_size, memory_size = symbols['__image_end'], symbols['__memory_end']
    if not 0 < image_size <= memory_size <= MAX_MEMORY or not entry & 1 or entry & ~1 >= image_size:
        raise ValueError('Invalid module image or Thumb entry')
    image = bytearray(image_size)
    for _, kind, flags, addr, off, size, *_ in sections:
        if flags & 2 and kind != 8 and size:
            if addr + size > image_size:
                raise ValueError('Allocated section outside image')
            image[addr:addr + size] = data[off:off + size]
    relocations = []
    # Relative code/data references are already resolved by the linker.
    relative = {0, 3, 10, 25, 26, 28, 29, 30, 42, 45, 46, 49, 50, 51}
    for section in sections:
        if section[1] != 9 or not sections[section[7]][2] & 2:
            continue
        for off in range(section[4], section[4] + section[5], 8):
            address, info = struct.unpack_from('<II', data, off)
            kind = info & 255
            if kind == 2:  # R_ARM_ABS32: link-time image offset becomes runtime pointer.
                if address % 4 or address + 4 > image_size:
                    raise ValueError('Unaligned or out-of-bounds pointer relocation')
                value, = struct.unpack_from('<I', image, address)
                if value >= memory_size:
                    raise ValueError('Absolute pointer outside module')
                relocations.append(address)
            elif kind not in relative:
                raise ValueError(f'Unsupported ARM relocation {kind} at {address:#x}')
    relocations.sort()
    if len(set(relocations)) != len(relocations):
        raise ValueError('Duplicate relocation')
    file_size = HEADER + image_size + 4 * len(relocations)
    if file_size > MAX_FILE:
        raise ValueError('Module exceeds 64 KiB file limit')
    header = struct.pack('<4s15I', b'Z72M', 1, TARGET, file_size, image_size,
                         memory_size - image_size, entry & ~1, len(relocations), 0, *([0] * 7))
    packed = seal(header + image + b''.join(struct.pack('<I', off) for off in relocations))
    return packed, dict(image_bytes=image_size, bss_bytes=memory_size - image_size,
                        entry_offset=entry & ~1, relocations=relocations,
                        symbols={k: v for k, v in symbols.items() if k and not k.startswith('$')})


def build(source):
    with tempfile.TemporaryDirectory(prefix='z72-native-module-') as folder:
        obj, elf = Path(folder) / 'module.o', Path(folder) / 'module.elf'
        subprocess.run(['clang', '--target=arm-none-eabi', '-mcpu=cortex-a9', '-mthumb',
                        '-mfloat-abi=soft', '-Os', '-fPIC', '-fvisibility=hidden',
                        '-ffreestanding', '-fno-builtin', '-fno-unwind-tables',
                        '-fno-asynchronous-unwind-tables', '-fno-stack-protector',
                        '-Wall', '-Wextra', '-Werror', '-I', ROOT / 'sdk',
                        '-c', source, '-o', obj], check=True)
        subprocess.run(['ld.lld.exe' if os.name == 'nt' else 'ld.lld', '-T', ROOT / 'sdk/module.ld', '--emit-relocs',
                        '--no-undefined', obj, '-o', elf], check=True)
        return from_elf(elf.read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    module, report = build(args.source)
    with args.output.open('xb') as handle:
        handle.write(module)
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

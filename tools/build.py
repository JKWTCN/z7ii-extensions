"""Reproduce the verified complete Z7II C 1.70 SD-host update package."""
import argparse
import binascii
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x10020000
APPEND = 0x12497B78
HOOKS = ((0x1044E010, 10, 'extension_init_hook'),
         (0x105AFC44, 8, 'extension_grid_source'),
         (0x105B010E, 10, 'extension_grid_enable'))
STOCK_SHA = '478caf02dba41fa9144c0c4b6e1a051dc556d40e51a79a308bdd1169efb9dbbe'
STOCK_PACKAGE_SHA = 'fc0e94f57500a1f0e287c6b9a036c81a5ffceccb8432a377d645b3fa35a19b6a'
VERIFIED_MASTER_SHA = '7ef71ba6dfb89b44a3580049d89357c5e940e14368fc4807934413c6e93537cc'
VERIFIED_PACKAGE_SHA = 'a0ea6e97f42022668510f635293b196d19c7e06bb20852e8dcad6fe6cebf3657'
# Public header digest from the verified update, not private signing material.
# Valid only for the exact package bound by VERIFIED_PACKAGE_SHA.
VERIFIED_HEADER = bytes.fromhex('91dd99e931c94b6c1f66c85597f6b505dec8d2fe')

# Static container encoding tables for C 1.70; not private signing material.
ENCODING_TABLES = bytes.fromhex(
    'a8be06a33deb1cf9e5138487728ddcadd4270e1b087a12f674b7ce52d0d8c27c'
    '496b28e8649550ddda99e93f100b9dde225c2a2442519a0a2e75fde0f1979ec3'
    '1da51e8c9bec58a2652947fc79d754abf23376cad9f85e6eafd3faaeb5853b34'
    'cda1830f01022ca6e2462fc56a6f5b7848ef04407323d162c730f003f526a0bf'
    '70edcf81b4aab9b61196947e44327d1438803e66ba1618891588e65663c4f78b'
    '053c983a676d8a437f574b17fb7bb0095f91c6c0d5602b86ffea3992358fee90'
    '1fb121b3bb8e69254f716cbd364c0c0d20b8c82de4acdbd207e1a461bcc1e782'
    '4ee3004dcb19d677a7b2375d684acc1a55df41f393fef45ac9a953459f9c3159'
    'bdc488121017508285897c54279ed2da93e8a26973a323e05a38d1ff26c1cd42'
    '79b2e10325687e1b36dc3205bff28c0e0a964db4a1602e3e025dbad7c2ee1847'
    'c7c97a83b78df5c3b0ca9fa07284063a9a9b926f6ca7f47d56778755ddfc6a0b'
    'a52911f6e9002c90539859fd5b67faefea0da8fe7f97d4bc812fc6d8460c52b5'
    '4af7abe33b7449e7b3b194584fbb21514519ec2b04a6db579dccd931ed5c4b28'
    '2aaa41be629115d59586adc57180eb8b634499b6142d64a475375f6bf1f88edf'
    'a9cb4c6578401f013422cf43078a1c1d705ef9d333b9aef0e2668fd00913d66e'
    '16ac1efb6d24e43948e6c876dee51ab89c61304e3fafc00f3c20f3083dce357b'
    'a4ae8d306809fb450d708bb17334fd7e487d16159b6177175699cd18c42d26e3'
    '1f33a96ae9d044ec836cc387f5f63a22b5904e819a2131395bf22e504de1285f'
    '08de0c10d7ac20fcbdc9eaa63fc69114ef045ec88eaff989ca9ee6e5ccadee98'
    'c21e40fa64e4a7367627ba55a1d5a22937a34cdd439dfe6f927935bc1b63b019'
    'bec0595c5385da003b67c732d2a5eddf3e5a6e9584d14f4660c5f7ff05710a69'
    'f41123d4b902138cb24bb3948a6b0696781ddc2b80668293077f86aa52a8f32a'
    '4a75b7bbf8d6cb123de7f09f41496d016225f15d7ab62fbfa051e80f3824033c'
    'ce1c57ab9cb4c1d988b854588fe21a7b7247dbcf740bd37c42ebe0d82c0e6597'
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def thumb_jump(site, length, target):
    padding = (-site) % 4
    code = struct.pack('<HH', 0xF8DF, 0xF000 | (4 if padding else 0))
    code += b'\x00\xbf' * (padding // 2) + struct.pack('<I', target | 1)
    if len(code) > length or (length-len(code)) % 2:
        raise ValueError('Hook does not fit instruction window')
    return code + b'\x00\xbf' * ((length-len(code)) // 2)


def build_host(stock):
    if sha(stock) != STOCK_SHA or BASE + len(stock) - 2 != APPEND or binascii.crc_hqx(stock, 0):
        raise ValueError('Requires the exact original Z7II C 1.70 master')
    with tempfile.TemporaryDirectory(prefix='z72-native-host-') as folder:
        obj, elf, binary = (Path(folder) / name for name in ('host.o', 'host.elf', 'host.bin'))
        entry = Path(folder) / 'entry.o'
        subprocess.run(['clang', '--target=arm-none-eabi', '-mcpu=cortex-a9', '-mthumb',
                        '-c', ROOT / 'host/entry.S', '-o', entry], check=True)
        subprocess.run(['clang', '--target=arm-none-eabi', '-mcpu=cortex-a9', '-mthumb',
                        '-mfloat-abi=soft', '-Os', '-ffreestanding', '-fno-builtin',
                        '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
                        '-fno-stack-protector', '-Wall', '-Wextra', '-Werror',
                        '-I', ROOT / 'sdk', '-c', ROOT / 'host/host.c', '-o', obj], check=True)
        subprocess.run(['ld.lld.exe' if os.name == 'nt' else 'ld.lld',
                        '-T', ROOT / 'host/host.ld', '--no-undefined', entry, obj, '-o', elf], check=True)
        subprocess.run(['llvm-objcopy', '-O', 'binary', '--only-section=.extension', elf, binary], check=True)
        symbols = {name: int(value, 16) for value, name in re.findall(
            r'^([0-9a-fA-F]+) [tT] (\w+)$', subprocess.check_output(['llvm-nm', elf], text=True), re.M)}
        code = binary.read_bytes()
        if not 0 < len(code) < 8192:
            raise ValueError('Unexpected host size')
    patched = bytearray(stock[:-2])
    edits = []
    for site, size, name in HOOKS:
        offset=site-BASE
        patched[offset:offset+size]=thumb_jump(site,size,symbols[name])
        edits.append(dict(address=hex(site), size=size, original_sha256=sha(stock[offset:offset+size])))
    # Own the eight alignment bytes between the initialized data and BSS.
    if struct.unpack_from('<II', stock, 0x140C4) != (0xB18AEDC0, 0x12AF750):
        raise ValueError('Original BSS boundary mismatch')
    struct.pack_into('<II', patched, 0x140C4, 0xB18AEDB8, 0x12AF758)
    patched.extend(code)
    patched.extend(binascii.crc_hqx(patched, 0).to_bytes(2, 'big'))
    report = dict(stock_sha256=sha(stock), master_sha256=sha(patched), host_bytes=len(code),
                  hooks=edits, anchor='0xb18aedb8', append=hex(APPEND), symbols=symbols,
                  camera_verified=False)
    return bytes(patched), report


def transform_payload(payload):
    """Apply the stock container's symmetric position-dependent encoding."""
    tables = ENCODING_TABLES
    low, middle, high = tables[:256], tables[256:512], tables[512:]
    result = bytearray()
    for start in range(0, len(payload), 256):
        block = payload[start:start + 256]
        position = start + 32
        mask = bytes(low[(position + i) & 255] ^
                     middle[((position + i) >> 8) & 255] ^
                     high[((position + i) >> 16) & 255] for i in range(len(block)))
        result.extend((int.from_bytes(block, 'little') ^
                       int.from_bytes(mask, 'little')).to_bytes(len(block), 'little'))
    return bytes(result)


def container_members(body):
    """Validate directory boundaries and CRCs; return ordered raw members."""
    if len(body) < 48 or body[:32] != b' ' * 32:
        raise ValueError('Invalid container marker or length')
    count, offset, reserved1, reserved2 = struct.unpack_from('>4I', body, 32)
    if not 0 < count <= 64 or offset != 48 + count * 32 or reserved1 or reserved2:
        raise ValueError('Invalid container directory')
    if offset > len(body):
        raise ValueError('Truncated container directory')
    members = []
    for index in range(count):
        name, start, size, r1, r2 = struct.unpack_from('>16s4I', body, 48 + index * 32)
        if start != offset or size < 2 or start + size > len(body) - 16 or r1 or r2:
            raise ValueError('Invalid container member bounds')
        value = body[start:start + size]
        if binascii.crc_hqx(value, 0):
            raise ValueError('Container member CRC mismatch')
        members.append((name, value))
        offset += size
    if (len(body) != offset + 16 or body[offset + 2:] != bytes(14) or
            binascii.crc_hqx(body[:offset + 2], 0)):
        raise ValueError('Container trailer CRC or padding mismatch')
    return members



def extract_master(stock_package):
    if sha(stock_package) != STOCK_PACKAGE_SHA:
        raise ValueError('Requires the exact original C 1.70 update package')
    members = container_members(b' ' * 32 + transform_payload(stock_package[64:]))
    masters = [value for name, value in members if name.startswith(b'eg1840_mas_')]
    if len(masters) != 1 or sha(masters[0]) != STOCK_SHA:
        raise ValueError('Extracted original master mismatch')
    return masters[0]


def build_package(stock_package, stock_master, candidate):
    # Fixed hashes bind both inputs and the result to a hardware-tested release.
    # A changed host must never inherit the previously computed header digest.
    if sha(stock_package) != STOCK_PACKAGE_SHA or sha(stock_master) != STOCK_SHA:
        raise ValueError('Requires the exact original C 1.70 package and master')
    if sha(candidate) != VERIFIED_MASTER_SHA:
        raise ValueError('Host differs from the verified release; refusing to reuse its header')
    original = container_members(b' ' * 32 + transform_payload(stock_package[64:]))
    targets = [i for i, (name, _) in enumerate(original) if name.startswith(b'eg1840_mas_')]
    if len(targets) != 1 or original[targets[0]][1] != stock_master:
        raise ValueError('Original package master mismatch')
    members = list(original)
    members[targets[0]] = (original[targets[0]][0], candidate)
    offset = 48 + 32 * len(members)
    directory = bytearray(struct.pack('>4I', len(members), offset, 0, 0))
    for name, value in members:
        directory.extend(struct.pack('>16s4I', name, offset, len(value), 0, 0))
        offset += len(value)
    body = b' ' * 32 + directory + b''.join(value for _, value in members)
    body += binascii.crc_hqx(body, 0).to_bytes(2, 'big') + bytes(14)
    packed = VERIFIED_HEADER + bytes(12) + b' ' * 32 + transform_payload(body[32:])
    if sha(packed) != VERIFIED_PACKAGE_SHA:
        raise ValueError('Complete package differs from the verified release')
    decoded = b' ' * 32 + transform_payload(packed[64:])
    if container_members(decoded) != members:
        raise ValueError('Package roundtrip member mismatch')
    return packed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-package', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True,
                        help='Complete update file, e.g. artifacts/Z7_2_0170.bin')
    args = parser.parse_args()
    manifest = args.output.with_suffix(args.output.suffix + '.json')
    if args.output.exists() or manifest.exists():
        raise ValueError('Output or manifest already exists')
    stock_package = args.stock_package.read_bytes()
    if sha(stock_package) != STOCK_PACKAGE_SHA:
        raise ValueError('Requires the exact original C 1.70 update package')
    stock_master = extract_master(stock_package)
    candidate, report = build_host(stock_master)
    packed = build_package(stock_package, stock_master, candidate)
    report.update(stock_package_sha256=sha(stock_package), package_sha256=sha(packed),
                  package_bytes=len(packed), only_master_changed=True,
                  roundtrip_verified=True, matches_verified_release=True,
                  camera_verified=True)
    with args.output.open('xb') as output:
        output.write(packed)
    with manifest.open('x', encoding='utf-8') as output:
        output.write(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

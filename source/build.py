#!/usr/bin/env python3
"""Build a Skyline NRO with Zig 0.13.0 and Python 3; no devkitPro needed.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parent.parent

def align(value):
    return (value + 4095) & ~4095

def convert(elf_path, nro_path):
    raw = Path(elf_path).read_bytes()
    if raw[:6] != b'\x7fELF\x02\x01':
        raise ValueError('Expected ELF64 little endian')
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', raw)
    _, kind, machine, _, _, phoff, shoff, _, _, phsize, phnum, shsize, shnum, _ = header
    if (kind, machine, phsize, shsize) != (3, 183, 56, 64):
        raise ValueError('Expected AArch64 shared ELF with standard headers')
    loads = []
    for i in range(phnum):
        ph = struct.unpack_from('<IIQQQQQQ', raw, phoff + i * phsize)
        if ph[0] == 1:
            loads.append(ph)
    if len(loads) != 3 or [p[1] for p in loads] != [5, 4, 6]:
        raise ValueError('Expected exactly text, rodata, and data load segments')
    cursor = 0
    for p in loads:
        if p[3] != cursor or p[3] % 4096 or p[5] > p[6] or p[2] + p[5] > len(raw):
            raise ValueError('Invalid or noncontiguous load segments')
        cursor += align(p[5])
    out = bytearray(cursor)
    for p in loads:
        out[p[3]:p[3]+p[5]] = raw[p[2]:p[2]+p[5]]
    # Nintendo NRO header; layout follows switchbrew/switch-tools elf2nro.
    out[0x10:0x80] = bytes(0x70)
    struct.pack_into('<4sIII', out, 0x10, b'NRO0', 0, len(out), 0)
    for i, p in enumerate(loads):
        struct.pack_into('<II', out, 0x20 + i * 8, p[3], align(p[5]))
    data = loads[2]
    bss = align(max(0, data[6] - align(data[5])))
    struct.pack_into('<I', out, 0x38, bss)
    for i in range(shnum):
        sh = struct.unpack_from('<IIQQQQIIQQ', raw, shoff + i * shsize)
        if sh[1] != 7 or sh[5] < 12:
            continue
        namesz, descsz, note_type = struct.unpack_from('<III', raw, sh[4])
        name = raw[sh[4]+12:sh[4]+12+namesz]
        if name == b'GNU\0' and note_type == 3:
            start = sh[4]+12+((namesz+3)&~3)
            out[0x40:0x40+min(descsz,32)] = raw[start:start+min(descsz,32)]
    mod = struct.unpack_from('<I', out, 4)[0]
    if mod < 0x80 or out[mod:mod+4] != b'MOD0':
        raise ValueError('Missing or overwritten MOD0 header')
    dynamic, bss_start, bss_end, _, _, runtime = struct.unpack_from('<6i', out, mod+4)
    if (mod+bss_start != len(out) or mod+bss_end != len(out)+bss or
        not (data[3] <= mod+dynamic < len(out)) or
        not (len(out) <= mod+runtime <= len(out)+bss-0xd0)):
        raise ValueError('Invalid MOD0 dynamic/BSS/runtime offsets')
    nro_path = Path(nro_path)
    nro_path.parent.mkdir(parents=True, exist_ok=True)
    nro_path.write_bytes(out)
    return {'file':str(nro_path.relative_to(ROOT)), 'bytes':len(out),
            'bss_bytes':bss, 'sha256':hashlib.sha256(out).hexdigest()}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--zig', default=shutil.which('zig'))
    args = parser.parse_args()
    if not args.zig:
        parser.error('Zig 0.13.0 is required; pass --zig /path/to/zig')
    version = subprocess.check_output([args.zig, 'version'], text=True).strip()
    if version != '0.13.0':
        parser.error('Use Zig 0.13.0 for this reproducible build')
    output = ROOT / 'build'
    output.mkdir(exist_ok=True)
    elf = output / 'cpu_rematch.elf'
    cmd = [args.zig, 'c++', '-target', 'aarch64-linux-gnu', '-mcpu=cortex_a57',
        '-std=c++17', '-O2', '-fPIC', '-ffreestanding', '-fno-exceptions', '-fno-rtti',
        '-fno-stack-protector', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
        '-fvisibility=hidden', '-nostdlib', '-shared', '-Wall', '-Wextra', '-Werror',
        '-Wl,-T,source/plugin.ld', '-Wl,--version-script=source/exports.map',
        '-Wl,--hash-style=sysv', '-Wl,--build-id=sha1', '-Wl,-Bsymbolic',
        '-Wl,-z,max-page-size=4096', 'source/plugin.cpp', 'source/module.S', '-o', str(elf)]
    subprocess.run(cmd, cwd=ROOT, check=True)
    nro = ROOT / 'sdmc/atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libcpu_rematch.nro'
    result = convert(elf, nro)
    (output/'manifest.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()

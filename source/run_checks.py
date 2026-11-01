#!/usr/bin/env python3
"""Build the plugin and run host/compiled-ARM checks. AGPL-3.0-or-later."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--zig')
    args = parser.parse_args()
    zig = args.zig or shutil.which('zig')
    if not zig:
        try:
            import ziglang
            zig = str(Path(ziglang.__file__).parent / 'zig')
        except ImportError:
            parser.error('Install requirements-dev.txt or pass --zig /path/to/zig')
    subprocess.run([sys.executable, 'source/build.py', '--zig', zig], cwd=ROOT, check=True)
    compiler = shutil.which('g++') or shutil.which('clang++')
    if not compiler:
        parser.error('A C++17 host compiler is required')
    with tempfile.TemporaryDirectory(prefix='cpu-rematch-checks-') as tmp:
        for name in ['gate', 'startup', 'diagnostics', 'mode']:
            output = str(Path(tmp) / ('test_' + name))
            command = [compiler, '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror']
            env = os.environ.copy()
            if name == 'mode':
                command += ['-g', '-fsanitize=address', '-fno-omit-frame-pointer']
                env['ASAN_OPTIONS'] = 'detect_leaks=0'
            command += [str(ROOT / 'source' / ('test_' + name + '.cpp')), '-o', output]
            subprocess.run(command, cwd=ROOT, check=True)
            subprocess.run([output], cwd=ROOT, env=env, check=True)
    nro = str(ROOT / 'sdmc/atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libcpu_rematch.nro')
    configurations = [
        [], ['--vtable-data'],
        ['--vtable-data', '--pad-style', '4', '--controller-id', '1', '--sub-width', '4',
         '--game-base', '0x7100000000', '--plugin-base', '0x5000000000'],
        ['--pad-style', '2', '--controller-id', '32'],
    ]
    for options in configurations:
        subprocess.run([sys.executable, 'source/test_arm.py', nro, *options], cwd=ROOT, check=True)

if __name__ == '__main__':
    main()

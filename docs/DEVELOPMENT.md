# Building and testing

Installing the mod does not require any of these tools. Download the ZIP linked
in [README.md](../README.md) if you just want to play.

## Setup

Use **Python 3.10+**, **Git** and a **C++17 compiler** (`g++` or `clang++`). The full
test runner targets Linux; Ubuntu 24.04 is used in CI.

From the repository root:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-dev.txt
```

The requirements include Zig **0.13.0** for cross-compilation, Unicorn for ARM
simulation and PyYAML for repository checks. No devkitPro installation is needed.

## Run the checks

```sh
.venv/bin/python scripts/check_repository.py
.venv/bin/python source/run_checks.py
sha256sum --check SHA256SUMS.txt
```

The repository check reads tracked files. Stage new files with `git add` first.
The plugin runner rebuilds the NRO, runs four host tests (including an
AddressSanitizer test) and executes the compiled ARM plugin in four simulated
runtime configurations. It finds the Zig executable supplied by `ziglang`
automatically; alternatively pass `--zig /path/to/zig`.

The checksum compares the rebuilt NRO with the released v0.5.0 binary. An
intentional source change may require a new binary, checksum and install ZIP.
Review [validation notes](VALIDATION.md) before claiming game compatibility:
the automated checks use mocked game/SDK behavior and do not run Eden or Smash.

## Build only

With a separate Zig 0.13.0 installation:

```sh
python3 source/build.py --zig /path/to/zig
```

The plugin is written to
`sdmc/atmosphere/contents/01006A800016E000/romfs/skyline/plugins/libcpu_rematch.nro`.
Intermediate ELF files and a build manifest go in ignored `build/`.

## Contributing

Open a pull request describing the problem, the resulting behavior and the
checks you ran. Preserve the version and memory checks and keep changes scoped
to regular local 1v1 CPU matches. Include regression coverage for behavior changes.
Keep source, release files and documentation consistent when changing the mod.

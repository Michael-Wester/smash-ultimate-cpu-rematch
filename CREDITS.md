# Source provenance

This is a standalone C++ implementation first prepared on 2026-10-31.
It is not an official release by Eden, Skyline, or the authors cited below.

The plugin and its build/test source are distributed under AGPL-3.0-or-later.
Full corresponding source is included in `source/`; the license text is in
`LICENSE`.

## HewDraw Remix contributors

The game-state pointer walk, retry vtable slot, executable signatures, and regular
Smash mode identification are based on the public research/code in HewDraw Remix:

- https://github.com/HDR-Development/HewDraw-Remix/blob/73eceada689d434fa151cee140eff7ef08419641/utils/src/offsets.rs
- https://github.com/HDR-Development/HewDraw-Remix/blob/a56075404c96853b260689a341d63fcb90a33f93/dynamic/src/util.rs
- https://github.com/HDR-Development/HewDraw-Remix/blob/a56075404c96853b260689a341d63fcb90a33f93/utils/src/game_modes.rs
- https://github.com/HDR-Development/HewDraw-Remix/blob/a56075404c96853b260689a341d63fcb90a33f93/dynamic/src/singletons.rs

Copyright remains with the respective HDR contributors. Modifications in this
package include C++ translation, exact-version gating, unique-match scanning,
instruction validation, CPU match restriction, input latching, optional logs,
standalone binary packaging and host tests.

## Other references

- jugeeya's Salty Runback established the initial direction of research. Its code
  was inspected, but this package uses the HDR research above as its restart basis:
  https://github.com/jugeeya/ultimate-salty-runback
- Game function ABI declarations were checked against skyline-smash's `cpp.rs`,
  blob `adfa10cb4b92474e16c350a027bd72e6c7a8930b`:
  https://github.com/ultimate-research/skyline-smash
- Controller ABI and style constants were checked against ninput:
  https://github.com/blu-dev/ninput/blob/master/src/lib.rs
- Skyline loading, hook imports, SDK version queries and MOD0 conventions:
  https://github.com/skyline-dev/skyline
  https://github.com/ultimate-research/skyline-rs
- Linker layout and SDK ABI references: Genwald's MIT-licensed template.
  Its MIT notice is included in `licenses/SkylinePluginTemplate-MIT.txt`:
  https://github.com/Genwald/SkylinePluginTemplate
- NRO layout/ELF conversion semantics were checked against switchbrew's
  `elf2nro.c`. The small Python converter is written for this package and does
  not bundle switch-tools or its dependencies:
  https://github.com/switchbrew/switch-tools/blob/master/src/elf2nro.c

No Nintendo game code or assets are included.

## Implementation references

The following notes record the research used during development. See
[validation notes](docs/VALIDATION.md) for the current test results and the
project owner's manual confirmation of v0.5.0.

### Native filesystem diagnostics

Native filesystem symbol names and ABI checked against:
https://github.com/ultimate-research/nnsdk-rs/blob/master/src/nn/fs.rs

The filesystem backend is tested using SDK mocks. Bounded diagnostics remain
included to help troubleshoot installations.

### State lookup

The v03 fallback uses the 13.0.3-era HDR map at commit
`73eceada689d434fa151cee140eff7ef08419641`, specifically state slot `0x52c3760`,
frame hook `0x135b810`, and mode function `0x1743870`:
https://github.com/HDR-Development/HewDraw-Remix/blob/73eceada689d434fa151cee140eff7ef08419641/utils/src/offsets.rs

QueryMemory calling convention was checked against Skyline's syscall wrapper:
https://github.com/skyline-dev/skyline/blob/master/source/skyline/nx/kernel/svc.s

The correction and regression harness are included in source.

### Mode output storage

The plugin allocates aligned 8-byte storage for both mode outputs. Research on
output widths included native caller analysis in the 13.0.5 xref database; this
is a different game version and is supporting research rather than runtime proof:
https://github.com/Atamol/ssbu-xref-db/blob/main/decompiled/FUN_7101746780__7101746780.c

No decompiled game source is bundled. The AddressSanitizer regression uses a mock
callee that writes an 8-byte submode result into the actual plugin call site.

### Vtable validation and ARM tests

Version 0.5.0 expands readable vtable validation to the main image's data memory, with a
QueryMemory-derived bound. The executable retry target is still restricted to
main-game text. Main-module writable vtables are illustrated in the published
analysis table; this does not prove the actual battle-state vtable's location:
https://github.com/Atamol/ssbu-xref-db/blob/main/tables/vtables.csv

The included ARM harness is original test code. Unicorn 2.1.4 is an external test
dependency and is not bundled. No proprietary game code/assets are included in the
harness. All game and SDK behavior used in it is a simulation; limitations are
listed in docs/VALIDATION.md.

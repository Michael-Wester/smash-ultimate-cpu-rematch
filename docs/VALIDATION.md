# Validation record — CPU Rematch 0.5.0

## Manual runtime confirmation

On 2026-11-01 the project owner confirmed v0.5.0 working on their Windows/Eden
setup with Smash Ultimate 13.0.3, both for mid-match restarts and during GAME!.
This is a reported manual test on one setup, not an automated emulator test.
Other emulator versions, rule combinations and platforms remain unverified.


## Compiled artifact

- Zig 0.13.0, Cortex-A57/AArch64, warnings treated as errors.
- NRO: 16,384 bytes; BSS: 4,096 bytes.
- SHA-256: `2822666a1527dd25e1e4b3e25ff25c64757ac80469329bf258122e7fa78d00a1`.
- NRO/MOD0 headers and bounds: checked by build script.
- Imports: Skyline A64HookFunction, getRegionAddress, SDK LookupSymbol.
- No C++/Linux runtime dependencies; 3 AArch64 JUMP_SLOT relocations.

## Regression evidence

- Original 4-byte mode storage: AddressSanitizer detects overflow when mock callee
  writes an 8-byte submode result. Corrected storage passes.
- Original v03 compiled ARM binary: wide submode write makes mode read back as zero.
- V04 compiled ARM binary: readable data-section vtable is rejected.
- V05 compiled ARM binary: both read-only and module-data vtables reach mock retry.

## ARM runtime matrix

| Vtable | Controller | ID | Mode write | Addresses | Result |
|---|---|---|---|---|---|
| RO | Pro | 0 | 8 bytes | User-log layout | Pass |
| Data | Pro | 0 | 8 bytes | User-log layout | Pass |
| Data | Paired Joy-Con | 1 | 4 bytes | Game 0x7100000000; plugin 0x5000000000 | Pass |
| RO | Handheld | 32 | 8 bytes | User-log layout | Pass |

The harness executes the delivered NRO, not a host recompilation of the plugin.
It models game data and SDK functions and verifies that the native ARM callback
receives the correct state pointer. It checks stack and callee-saved registers.

Positive and negative scenarios: initialization, eligible 1v1 CPU match, inputs,
repeated requests, held/released keys, cooldown, simulated post-GAME ready-flag
transition, invalid mode/count/CPU mix, results, disconnected input, invalid pointer
chain, invalid target, and matching file/debug diagnostics.

All four host tests pass: mode/ASan, startup, native-filesystem mocks, input gate.

## What remains unverified

Broad compatibility with other Eden versions, rule combinations and platforms;
detailed retry side effects and result/stat recording; repeatability beyond the
manual confirmation on one setup. The simulated post-GAME case assumes the battle
remains loaded and the results-mode flag has not changed. It does not establish
that those conditions hold at the desired instant in the real game.

The automated tests here do not run Eden or the real game. The manual confirmation
above supplies the live runtime evidence. No empirical success probability is claimed.

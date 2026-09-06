# Camera elapsed dispatch recovery

`game_camera_elapsed_dispatch.c` owns the complete GAMEONLY routine `0x800798B4..0x800799CB`: 280 bytes and 70 instructions. The fresh Ghidra listing `game_800798b4.txt` has instruction SHA-256 `731f6e8cdd8970250f106b51aaa7cdb2382737c64e28ea4fea1825a65ab13fbf`. Known callers are `0x8002DD9C`, `0x80037AB0`, `0x80078F9C`, `0x80077E34`, `0x80079C2C`, `0x80079C8C`, `0x80079DDC`, `0x8007A2E4`, and `0x800385D8`.

The source allocates an `0x18`-byte frame and spills RA in the initial argument branch delay. A0=`0xFFFFFFFF` reloads the signed lower threshold from `0x800BC1F8`; every other value adds to `0x80106074` with 32-bit wrap. The candidate is published and freshly reread, capped when signed upper `<` elapsed, then freshly reread again. Signed elapsed below the lower threshold returns immediately with the second SLT Boolean in V0 and leaves the elapsed word intact.

At or above the lower threshold, nonzero `0x800BC200` skips the indirect service. Zero follows `0x800FC9D0` to its word at offset `0x5C` and executes the dynamic JALR after setting RA and executing its NOP. Unknown, zero and unaligned dynamic targets retain that exact prefix. The routine then rereads `0x800BC1F4`. Negative state calls `0x8007A410`; nonnegative state calls `0x8007A468`, masks its raw result to the low byte, and calls `0x8007A410` only when that byte is zero. A nonzero byte freshly reloads callback-mutable `0x800BC1F4`.

The refresh return remains raw and is stored back to `0x800BC1F4`. Source order then stores V0 to `0x800D8EEC` before resetting `0x80106074` to zero. The epilogue reloads RA through callback-live SP, advances SP by `0x18`, executes the JR delay NOP, and only then refuses unknown or misaligned RA.

The strict C99 owner retains all 32 GPRs, HI/LO and per-byte knownness. Exact signed bounds preserve known low bytes even when the sign byte is unknown. Addition retains invariant result bytes through carry enumeration, and ANDI makes its upper three bytes known zero. Validated little-endian mapped memory provides atomic loads, partial stores, known-null checks, wrapping guest addresses, access journals, and a budget at every mapped access or child call. Progress exposes all three child prefixes, instruction counts, elapsed and publication state, the dynamic target, frame SP and restored RA.

`game_camera_elapsed_dispatch_adapter.cpp` binds both actual recovered camera-selector sites: `0x80079C2C`/delay `0x80079C30`/RA `0x80079C34` and `0x80079C8C`/delay `0x80079C90`/RA `0x80079C94`. Both carry one argument with A0=`0xFFFFFFFF`. The parent event contract has no invocation field, so cumulative binding counts remain telemetry and never restrict binding reuse. Any identifying PC, delay, entry, kind or known RA claims the assigned boundary before exact guards.

The recovered parent exposes only GPRs, so the bridge explicitly starts HI/LO unknown. Typed full-machine callbacks preserve valid GPR mutation prefixes when only HI/LO metadata is invalid; an invalid zero register or GPR mask cannot poison the narrow parent. The dynamic target, `0x8007A468`, and `0x8007A410` remain typed services because no complete compatible owner exists in this boundary.

The focused executable performs 180 always-active checks. It covers the exact sentinel and non-sentinel paths; negative and overflowing deltas; equal, signed-extreme and inverted bounds; upper clamping and lower exit; all indirect-gate paths; dynamic unknown, zero, unaligned, unmapped and wrapping targets; negative and nonnegative cached state; probe results 0, `0x100`, `0xFF`, and all 16 knownness masks; partial raw refresh returns; every child refusal; callback-live GPRs, SP, RA, HI/LO and memory; every operation-budget prefix; all 16 restored-RA masks; malformed atomic loads; unavailable knownness stores; stack/global aliases and wrapping; mapping, overlap, alignment and `SIZE_MAX`; exact publication order; deterministic RAM, knownness and machine state; and null arguments.

The natural executable performs 38 always-active checks through the actual recovered camera selector. Mode 8 with selector 1 reaches `0x80079C2C`; selector 0 reaches `0x80079C8C`. Both prove the corrected NOP delay PC, A0=`0xFFFFFFFF`, RA, entry, argument count, unknown HI/LO boundary, typed refresh, malformed parent guards, nested refusal, invalid HI/LO prefix retention, invalid GPR isolation, operation limits, missing mapping, direct binding reuse, wrapper reuse, and null arguments. The test whitelists each synthetic prerequisite parent service with its exact PC, entry, delay, argument count, live arguments and RA, assigns explicit V0 fixture results, checks their order around CH, and proves a prerequisite refusal prevents CH. Both suites use heap-backed runtime-generated 2 MiB fixtures and no runtime assets. They compile with strict C99/C++17 `-Wall -Wextra -Werror -pedantic-errors`.

The independent original-instruction differential passed 8,064 cases across all 70 PCs, all 32 GPRs plus HI/LO and their masks, the full 2 MiB RAM fixture, callback entries, six live stack words, callback-relocated SP, callback-mutated cached state, and operation-budget prefixes.`r`n`r`nProduction dependencies are the shared retained-memory/full-machine types and the existing recovered `game_camera_select` owner. The other seven source callers and all three typed children remain outside CH.

Gameplay shown: **NO - no direct visual effect**. This routine updates retained elapsed/camera state and invokes camera services. Visible motion requires downstream camera and renderer composition in an advancing native match.

The native self-driving capture composes this owner at both actual selector
sites: the startup mode-12 path at `0x80079C8C` and the nested phase-selection
path at `0x80079C2C`. Both share their caller's retained memory, use explicit
synthetic thresholds and a typed refresh return of 42, publish that value, and
reset elapsed time. Each executes 48 instructions, eight reads, five stores,
and one typed refresh call. The parent subsequently clears its cached selector
state in source order. This adds no live match-loop or renderer claim.
The native screen remains User Setup; CPU diagnostic frame hashes both equal
SHA-256 `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
All 363 asset-free CTests and progress/metadata freshness checks pass.

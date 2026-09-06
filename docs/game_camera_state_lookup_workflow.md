# Camera state lookup recovery

`game_camera_state_lookup.c` owns the complete GAMEONLY routine
`0x8007A410..0x8007A467`: 88 bytes and 22 instructions. The fresh Ghidra
listing `game_8007a410.txt` has instruction SHA-256
`7b65160301ecc0720aa9d4f29a7c22f014a5eaea0623be65217514bccd262ee6`.
Its sole caller is the recovered camera elapsed dispatcher at `0x8007999C`,
with NOP delay `0x800799A0`, return address `0x800799A4`, and no arguments.
The lookup is a leaf and allocates no stack frame.

The owner loads the source word at `0x800FC9AC`, shifts it left four bits with
32-bit wrap, and masks it with `0xFFFFF000`. Thus source bits 0..7 and 28..31
do not affect the table lookup. The BLTZ delay always replaces V0 with
`0x8000`, including when source bit 27 is unknown. Nonnegative values use an
arithmetic-right-shifted index and the table displacement that begins at
`0x800BC204`. Negative values first add the delay value with 32-bit wrap and
use the separately biased table beginning at `0x800BC224`. The computed index
is unchecked, so the mapped guest address may leave either nominal table.

The strict C99 implementation retains all 32 GPRs, HI/LO, and per-byte
knownness. Exact completion over the two source bytes consumed by the index
preserves every invariant byte through the bias, arithmetic shift, offset,
base-register addition, and load address. AT retains the source-visible
`0x800C0000 + offset` result while the memory access separately applies its
signed load displacement. Mapped little-endian reads are atomic for malformed
knownness, accept a partial table word as the raw return, journal exact access
PCs and addresses, and consume one operation per load. JR executes its NOP
before unknown or unaligned RA is refused. The routine never writes memory.

`game_camera_state_lookup_adapter.cpp` binds the actual recovered parent event:
refresh kind `0x8007A410`, call PC `0x8007999C`, delay `0x800799A0`, entry
`0x8007A410`, invocation 1, argument count 0, and known RA `0x800799A4`.
Any identifying field claims that boundary before exact validation. Direct
binding reuse remains valid, while the wrapper resets per-parent-run telemetry.
The actual camera elapsed dispatcher and lookup share one retained memory and
full machine. Its other indirect and `0x8007A468` child events remain exact,
whitelisted typed fixture services because this assignment does not own them.

The focused executable performs 165 always-active checks. It covers zero,
positive, negative, bias-wrap, sign-transition, high-nibble and dropped-low-byte
inputs; checked AT and effective addresses on both sign paths; unmapped extreme
indices; partial source and table knownness; the BLTZ delay prefix; every load
budget cutoff; malformed later bytes; all 16 RA masks and unaligned RA; mapping
overlap, wrapping, `SIZE_MAX`, null journals and null arguments; access order;
all untouched registers and HI/LO; and deterministic unchanged RAM and known
planes.

The natural executable performs 27 always-active checks through the actual
camera elapsed dispatcher. Negative cached state calls the lookup directly.
The nonnegative path invokes the exact probe fixture first, whose explicit
result and source mutation then drive a different lookup entry; the zero gate
also proves the indirect-service, probe, and lookup order. The suite checks all
parent identifiers, direct and wrapper reuse, prerequisite refusal, lookup
budget exhaustion, missing mappings, malformed source knownness, valid machine
prefix propagation, publication order, and null arguments. Both suites use
heap-backed synthetic 2 MiB runtime fixtures and no runtime assets. They pass
strict C99/C++17 Clang `-Wall -Wextra -Werror -pedantic-errors` and MSVC
`/W4 /WX` builds.

Production dependencies are the shared mapped-memory/full-machine types and
the existing recovered `game_camera_elapsed_dispatch` owner. The indirect
service and the `0x8007A468` probe remain typed dependencies outside CK.

Manager differential validation passed 12,288 cases covering all 22 source PCs,
all 34 machine words and masks, full 2 MiB RAM, generated signed tables,
discarded input bits, and budget prefixes. The full asset-free suite passes
369 tests. The native capture composes actual selector, elapsed, and lookup
owners on the same memory: input zero reads `0x800BC204`; a typed adjustment
fixture supplies `0xFFFFFF00`, selecting index 7 at `0x800BC240`. Both generated
table cells return 42 and the actual elapsed owner publishes that value.
The paths execute 16 and 15 instructions, each with two reads. Comparison
frames share SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`;
the visible frontend remains User Setup.

Gameplay shown: **NO - no direct visual effect**. The routine only selects and
returns retained camera-state data. Any visible camera change requires its
caller and downstream camera/rendering work in a running match.

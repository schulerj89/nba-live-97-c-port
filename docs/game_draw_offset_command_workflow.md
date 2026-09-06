# GAMEONLY draw-offset command recovery

## Evidence and ownership

`game_8009a7dc.txt` is the fresh Ghidra source for the 18 instructions at
`0x8009A7DC..0x8009A823`. Its decoded instruction-byte SHA-256 is
`f1cfbd67f0585f8b4ff68a86906883752ae4cf73940cfe19b4df8b7116ce2ce7`.
The recovered owner covers exactly those 72 bytes. Known callers are
`0x8009A2A0` and the recovered draw-packet call at `0x8009A3B0`; the routine
has no callees. The ownership audit found only typed references and no prior
full owner.

The owner retains all 32 GPRs, HI, LO, and their byte masks. It performs the
single type-byte read from `0x800C55C0`, preserves the subtract and unsigned
comparison, and selects 12-bit fields only for types one and two. Every other
type uses 11-bit fields. The `BNE` delay always masks y to twelve bits, the
11-bit path performs its a0 mask in the unconditional jump delay, and the
final coordinate OR executes in the JR delay slot before unknown-RA refusal.
The owner performs no guest write.

The manager's independent original-instruction differential passed 8,192
cases across all 18 instruction prefixes. It compared all 34 machine words and
masks, unchanged mapped memory, every one of the 256 type bytes, raw coordinate
high bits, and budgets zero and one. The receipt is
`.local/evidence/tipoff-recovery/draw_offset_command_differential.json` in the
main repository.

## Native composition

`Nba97GameDrawOffsetCommandPacketBinding` accepts the exact source event at
`0x8009A3B0`, delay `0x8009A3B4`, entry `0x8009A7DC`, return address
`0x8009A3B8`, and two arguments. It validates mapped memory, every machine
mask, and its optional journal before entering the owner. The complete or
stopped full machine is copied back to the packet owner, and nested result and
progress remain explicit. Unrelated packet events can use the typed fallback.

The natural integration runs the recovered packet owner with the committed E3
start and E4 end owners followed by this E5 offset owner. For synthetic origin
`(100,200)`, size `(64,32)`, and offset `(3,4)`, it stores `0xE30C8064` at
packet offset four, `0xE40E70A3` at offset eight, and `0xE5004003` at offset
twelve. The final two packet children remain typed fixture services. A zero E5
budget leaves the preceding E3/E4 stores committed, blocks the E5 store, and
publishes live `RA=0x8009A3B8` with the allocated packet frame.

## Verification

The focused executable reports 22,294 passing checks. It covers every type
byte; raw zero, boundary, signed-bit, and high-bit coordinates; distinct
11-bit and 12-bit commands; every combination of A0/A1 per-byte knownness;
masked known-zero prefixes; unknown type and RA; the branch and JR delay
prefixes; missing and unaligned byte mappings; atomic malformed-byte failure;
budgets zero and one; all 34 machine words and masks; unchanged memory;
journal truncation; invalid, overlapping, and overflowing mappings; malformed
zero/HI/LO/GPR state; and deterministic repetition.

The natural integration executable reports 45 passing checks. It covers the
actual third packet event, all three recovered command stores, frame
restoration, full-machine propagation, nested budget failure, exact event and
RA guards, malformed machine/memory/journal rejection, and unrelated-event
fallback.

The owner builds with Clang in strict C99 mode using
`-pedantic -Wall -Wextra -Werror`. The adapter and tests build as C++17 with
the same warning policy. All build outputs remain beneath `%TEMP%`.

Gameplay shown: NO - no direct visual effect. This routine only packs a GPU
command word; visibility requires later packet submission and GPU consumption.

## Files and dependencies

The seven owned files are the recovered C source/header, packet adapter
source/header, focused test, natural integration test, and this workflow. The
production owner depends only on the shared retained-memory/full-machine model;
the adapter depends on the recovered packet event API. The natural test also
uses the committed draw-packet, draw-area-start, and draw-area-end owners. No
runtime asset, shared build file, capture, renderer path, or second PS1 routine
is added.
Manager native integration composes the actual scene, display, draw, packet,
draw-area start/end, offset, video query, GPU command, BIOS trampoline,
graphics submission and packet-DMA owners. Both offsets (2,3) pack E5001802
with type zero and one read; the packet stores them at 80021F14 and 80021F70
before submission and environment copying. Two packet helpers and device,
scheduler, critical-section and BIOS transfer services remain synthetic.
The CPU frames remain identical, and the displayed frontend is User Setup.

Manager verification passed 22,294 focused checks, 45 natural-caller checks,
all 319 asset-free CTests, strict C99/C++17 compilation, and progress/recovery,
instruction-semantics and roster freshness checks. Native receipt:
`.local/verification/team_select/game-entry-20260906-040914-cbc19063/frames/draw_offset_command_verified.json`.
Both CPU frame hashes are
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The local User Setup screenshot is ignored and is not gameplay.

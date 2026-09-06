# GAMEONLY draw-area end recovery

## Evidence and ownership

`game_8009a710.txt` is the fresh Ghidra source for the 51 instructions at
`0x8009A710..0x8009A7DB`. The decoded instruction-byte SHA-256 is
`0f5135aa43b161924d022ff28c214458f897f9df0228b62b4aaba8f7971fff5b`.
The recovered owner covers exactly those 204 bytes. Known callers are
`0x8009A25C` and the recovered draw-packet call at `0x8009A39C`; the routine
has no child calls. The ownership audit found only typed references and no
prior full owner.

The implementation retains all 32 GPRs, HI, LO, and their byte masks. It keeps
the conditional signed-limit reads at `0x800C55C4` and `0x800C55C6`, the
unconditional type read at `0x800C55C0`, wrapping limit-minus-one clamps, both
packing layouts, and the final JR delay-slot OR. It performs no guest write.
Signed comparisons use bounds after unsigned sign-bit biasing; each unknown
byte, including byte three, spans `00..FF`, so an unknown sign never decides a
clamp early.

The manager's independent original-instruction differential passed 16,384
cases across all 51 instruction prefixes. It compared all 34 machine words and
masks, mapped memory, all 256 GPU type bytes, signed coordinate and limit
extrema, high-word garbage, and budgets zero through three. The receipt is
`.local/evidence/tipoff-recovery/draw_area_end_differential.json` in the main
repository.

## Native composition

`Nba97GameDrawAreaEndPacketBinding` accepts the exact source event at
`0x8009A39C`, delay `0x8009A3A0`, entry `0x8009A710`, return address
`0x8009A3A4`, and two arguments. It validates memory, all machine masks, and
the optional journal before invoking the owner. It copies the complete or
stopped 32-GPR/HI-LO machine back to the packet owner and retains the nested
status and progress. Unrelated packet events can use its typed fallback.

The natural test runs the recovered packet owner and composes both recovered
coordinate commands. The committed draw-area-start owner handles child one.
The new end owner handles child two after the packet routine computes
`x + width - 1` and `y + height - 1` through live registers. For synthetic
origin `(100,200)` and size `(64,32)`, packet offset four receives
`0xE30C8064` and offset eight receives `0xE40E70A3`. The remaining three packet
children stay typed fixture services. With a zero end-owner budget, the start
store remains committed, the end store remains untouched, and the parent
publishes live `RA=0x8009A3A4` and its allocated frame.

## Verification

The focused executable reports 521 passing checks. It covers negative, normal,
and clamped axes; signed limit values `0`, `1`, `0x7FFF`, `0x8000`, and
`0xFFFF`; coordinate low-half extrema with high-word garbage; types `0`, `1`,
`2`, `3`, and `255`; distinct 10-bit and 12-bit command bits; all 34 machine
words and masks; partial coordinate, limit, type, and RA knownness; unknown
branch delay prefixes; skipped limit mappings; every read failure; budgets zero
through three; atomic malformed-byte rejection; final OR before unknown-RA
refusal; unchanged memory; journal truncation; invalid, overlapping,
overflowing, and unaligned mappings; and deterministic repetition.

The natural integration executable reports 43 passing checks. It covers the
actual second packet event and inclusive-coordinate calculation, both real
E3/E4 stores, frame restoration, full-machine propagation, the nested budget
failure prefix, exact event/RA guards, malformed machine/memory/journal
rejection, and unrelated-event fallback.

The recovered owner builds with Clang in strict C99 mode using
`-pedantic -Wall -Wextra -Werror`. The adapter and tests build as C++17 with
the same warning policy. All build outputs were written beneath `%TEMP%`.

Gameplay shown: NO - no direct visual effect. This owner packs a GPU command;
a visible change requires later packet submission and GPU consumption.

## Files and dependencies

The seven owned files are the recovered C source/header, packet adapter
source/header, focused test, natural integration test, and this workflow. The
production owner depends only on the shared retained-memory and full-machine
model. Its adapter uses the recovered packet event API. The natural test also
uses the committed draw-packet and draw-area-start owners. No runtime asset,
shared build file, capture, renderer path, or additional PS1 routine is added.

The native scripted scene composes the scene, display, draw, packet, draw-area
start and end, video query, GPU control and BIOS trampoline owners on shared mapped
memory. Both calls pack inclusive x=127/y=95 as E4017C7F with type zero and three reads;
the actual packet stores the command and the draw caller copies its environment.
Three later packet helpers and submission/MMIO/BIOS behavior remain synthetic
services. CPU before/after frames must match; the displayed frontend remains
User Setup and is not an advancing native match.

Manager verification passed 521 focused checks, 43 natural checks, all 313
asset-free CTests and progress/recovery/instruction/roster freshness checks.
The independent original comparison passed 16,384 cases across all 51
instructions and all 34 machine words. Native capture
game-entry-20260906-034713-a3ca18d8 proves both E4017C7F writes and the copied
environment. CPU frames retain SHA-256
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d. The
ignored User Setup screenshot is local proof, with no advancing match claim.

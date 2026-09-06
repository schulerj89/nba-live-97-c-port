# GAMEONLY draw-area start recovery

## Evidence and ownership

`game_8009a644.txt` is the fresh Ghidra source for the 51 instructions at
`0x8009A644..0x8009A70F`. The decoded instruction-byte SHA-256 is
`3c7734bec5a5abe7be9a069a0ed5a4cc70bcf291f6122580303e0cea36164f34`.
The recovered owner covers exactly those 204 bytes. Known callers are
`0x8009A224` and `0x8009A364`; the routine has no child calls. The repository
audit found no prior full owner.

The implementation uses full 32-GPR, HI, and LO state with one knownness bit
per byte. It keeps the original conditional reads from `0x800C55C4` and
`0x800C55C6`, the unconditional type read from `0x800C55C0`, signed wrapping
clamps, both packing layouts, and the final JR delay-slot OR. It performs no
guest write. Signed comparisons use bounds after unsigned sign-bit biasing;
unknown byte 3 spans the full `00..FF` range, so an unknown sign cannot decide
a clamp accidentally.

The manager's independent original-instruction differential passed 16,384
cases across all 51 instruction prefixes. It compared all 34 machine words
and masks, all eight mapped global bytes, all 256 GPU type bytes, signed
coordinate and limit extrema, high-word garbage, budgets zero through three,
and unchanged memory. The receipt is
`.local/evidence/tipoff-recovery/draw_area_start_differential.json` in the main
repository.

## Native composition

`Nba97GameDrawAreaStartPacketBinding` accepts only the source-proven packet
event at `0x8009A364`, delay `0x8009A368`, entry `0x8009A644`, return address
`0x8009A36C`, and two arguments. It copies all GPRs and HI/LO into the owner
and publishes the complete or stopped machine back to the parent. Other event
kinds can use the typed fallback. The binding retains the nested result and
progress rather than inventing a child completion.

The natural integration compiles the recovered draw-packet owner. The real first callback returns the E3 command, which
BG stores at packet offset four. The other four unresolved packet children
remain typed fixture callbacks. A zero owner budget proves nested refusal
occurs before the packet store while the allocated BG frame and JAL return
address remain live.

## Verification

The focused executable reports 521 passing checks. It covers negative,
normal, and clamped coordinates; signed limit values `0`, `1`, `0x7FFF`,
`0x8000`, and `0xFFFF`; coordinate low-half extrema with high-word garbage;
types `0`, `1`, `2`, `3`, and `255`; distinct 10-bit and 12-bit command bits;
all 34 machine words and masks; partial coordinate, limit, and type knownness;
branch delay prefixes; skipped limit mappings; each read failure; budgets zero
through three; atomic malformed-byte rejection; unknown RA after the final OR;
no memory writes; journal truncation; invalid, overlapping, overflowing, and
unaligned mappings; and deterministic repetition.

The natural integration executable reports 36 passing checks. It covers the
actual BG event, packet store, frame restoration, full-machine propagation,
budget refusal prefix, adapter event and RA guards, malformed machine and
memory rejection, malformed journal rejection, and unrelated-event fallback.
The C owner builds in strict C99 mode with `-pedantic -Wall -Wextra -Werror`;
the adapter and both tests build as C++17 with the same warning policy. A
second MSVC pass used `/W4 /WX /permissive-` where applicable. Build artifacts
were written under `%TEMP%`, outside the worktree.

Gameplay shown: NO - no direct visual effect. This routine only creates a GPU
command word; a visible result requires the later packet submission and GPU
consumption path.

## Files and dependencies

The seven owned files are the recovered C source/header, packet adapter
source/header, focused test, natural integration test, and this workflow. The
only production dependency is the shared match-clock machine and retained
memory model. The natural test additionally depends read-only on BG's
`game_draw_packet.c` and `game_draw_packet.h`. No runtime asset, BIOS body,
other PS1 routine, shared build file, capture, or renderer path is included.

The native scripted scene composes the scene, display, draw, packet, draw-area
start, video query, GPU control and BIOS trampoline owners on shared mapped
memory. Both calls pack x=64/y=32 as E3008040 with type zero and three reads;
the actual packet stores the command and the draw caller copies its environment.
Four later packet helpers and submission/MMIO/BIOS behavior remain synthetic
services. CPU before/after frames must match; the displayed frontend remains
User Setup and is not an advancing native match.

Manager verification passed 521 focused checks, 36 natural-caller checks, all
311 asset-free CTests, and progress/recovery/instruction/roster freshness checks.
The native capture game-entry-20260906-033606-6d6a46db retained identical CPU
frame SHA-256 391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d
and proved both E3008040 packet writes and the copied environment. Its local
User Setup capture is ignored; no gameplay is claimed.

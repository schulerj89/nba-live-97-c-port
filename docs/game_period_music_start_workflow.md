# GAMEONLY period music start recovery

`nba97_game_period_music_start` owns the complete GAMEONLY routine at
`0x800295D0..0x8002968B` (188 bytes, 47 instructions). A repository ownership
search found only the function inventory and the unresolved call in the
already-recovered first-period startup owner; no prior complete owner existed.
The source is the fresh Ghidra listing `game_800295d0.txt`, SHA-256
`ddd6f1b71a8e8d0bd5bcb770afb224b5c3dc4b60c084478b53f9cf3555c3072e`.

The owner creates the original `0x18`-byte frame, tests the unsigned enable
byte, optionally loads music through `0x800AAE7C`, rereads volume through the
callback-live `s0`, multiplies it by nine, clamps values above 126 to 127, and
dispatches the four playback children in source order. The typed events expose
each JAL PC, delay PC, entry, operation, invocation, argument count, and full
machine at callback entry. Reads, stores, alignment traps, aliases, unknown
bytes, callback mutations, and operation-budget prefixes remain observable.

The narrow adapter composes the owner at the actual first-period startup call
at `0x800673F8`. It claims any event or machine that names the assigned kind,
entry, call PC, delay PC, or return address before validating all fields, so a
malformed assigned event cannot escape through the fallback. The legacy parent
contains only GPRs. The adapter therefore supplies explicit unknown `{0,0}`
HI/LO words and does not claim knowledge absent from that caller. It returns a
valid GPR prefix even when a nested callback corrupts only HI/LO, while an
invalid GPR file is never copied into the parent.

Asset-free focused coverage exercises all 256 volume bytes, both load-flag
states, saturation at 14/15, disabled exit, exact call order and delay
arguments, live `s0`/`sp`/HI/LO mutations, every operation cutoff, each callback
refusal, partial and malformed knownness, null-known store rejection, mapping,
alignment, aliases, wrapped stack regions, unknown and misaligned JR, unchanged
registers, and deterministic failure prefixes. The natural test runs the
existing complete `0x800673F0` first-period caller through normal, disabled,
nested-failure, reuse, invalid-machine, and adapter-guard cases.

Manager-owned independent comparison against the original 47 instructions
passed 6,912 cases across every source PC, all 34 CPU words and masks, full
2 MiB RAM, callback entry machines, all 256 volume values under both flag
states, operation cutoffs, live pointer/memory mutation, relocated frames, and
RA/HI/LO effects. The receipt is
`.local/evidence/tipoff-recovery/period_music_start_differential.json` and stays
ignored.

This CPU routine starts and configures audio but renders no pixels. Gameplay
shown: **NO - no direct visual effect**. Manager-owned native capture supplies
the required CPU/audio-state evidence and pixel-identical frame hashes after
shared integration.

Manager integration passed all 345 asset-free Debug CTests (5.37 seconds),
progress and recovery validation, instruction-semantics freshness, and roster
configuration checks. Native input run `game-entry-20260906-063305-e29b37a0`
composes the actual first-period caller with this owner on shared diagnostic
RAM. Explicit volume fixtures 14 and 15 produce 126 and 127; unloaded and loaded
paths respectively perform 16/12 operations, 7/5 reads, 4/3 stores, and 5/4
typed audio calls. Both publish loaded and playing flags equal to one. The
native receipt records every call PC and argument, restored RA 0x80067400,
SP 0x801FFED0, and explicitly unknown HI/LO. No audio driver is claimed.
CPU-only frames share SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The displayed frontend is still User Setup; no advancing match is shown.

# GAMEONLY camera startup recovery

`nba97_game_camera_startup` owns the complete GAMEONLY routine at
`0x80079664..0x80079757` (244 bytes, 61 instructions). Fresh ignored Ghidra
evidence identifies the routine with SHA-256
`dce17f8d8df1a0f17fbe9e2b728274a7cceb1d1760633a75827575a3adc0c2cb`;
the private evidence text has SHA-256
`0d10447bcfe642a5fb2233cb449939824eeaf701fd8f8289590dc6f5be567fcd`.
The three source callers are the recovered match tick at call PC
`0x80068C2C`, `FUN_800786C0` at `0x800789C4`, and `FUN_80038108` at
`0x80038624`. The tracked source, documentation and function catalog had no
complete prior owner.

The owner reads unsigned bytes at `0x80021ED9` and `0x80021EDA` before it
forms the 0x18-byte stack frame. It publishes the source constants and bytes,
saves live `ra`, then compares entry `a0` with one. The branch delay always
sets `a0=12`: the non-one arm calls `0x800799CC(12,0)` and writes the two
extra globals, while the one arm reloads the unsigned byte at `0x80021ED7`
and passes that value to `0x800799CC(byte,0)`. Both child sites set `a1=0` in
their JAL delay slots. The owner then reloads three live source words, clears
five camera-state words, publishes the three sources, reloads `ra` through
the child-mutable live `sp`, advances `sp`, and returns.

All 32 GPRs use the existing per-byte-knownness register representation.
Pure 32-bit arithmetic preserves raw wrap and propagates known bytes through
carry dependencies. An unresolved entry branch reports unknown only after its
unconditional `a0=12` delay write. Partial bytes can be published without
becoming known, and unknown control-flow or `JR` inputs stop at their consuming
instruction. Every retained access is little-endian, validated for mapping and
alignment, journaled in source order, and remains visible after later failure.
Guest addresses are never converted to host pointers.

`0x800799CC` remains one full-GPR typed callback. It receives the exact call
PC, delay PC, target, argument count, operation index, retained memory and all
32 live registers. Child changes to memory, `sp`, saved-stack words and any GPR
remain visible. Refusal does not claim that the child returned, and a malformed
accepted register file is rejected.

`nba97_game_camera_startup_from_match_tick` is the narrow production adapter
for the existing tick call at `0x80068C2C`. The legacy tick service API records
only a narrow argument list and exposes neither retained memory nor the full
entry register file. The binding therefore requires explicit retained-memory
and full-GPR entry fixtures. It does not manufacture registers from the tick
call's absent state. It requires the explicit entry `a0` to be fully known,
equal to the call record, and exactly zero as assigned in source at
`0x80068C30`; contradictory or unknown fixtures are rejected without invoking
the owner. The adapter forwards the earlier hot-start and later tick services
to an explicit fallback, retains the exact nested camera result, and reports
success only after the whole camera owner returns.

The focused tests create all memory at runtime. They cover both branch arms,
entry `a0` values zero, one and high-bit non-one, mode bytes zero through 255 at
the boundary values, exact call arguments and delay registers, exact memory
publication/load order, callback-mutated globals/GPRs/stack/`ra`, callback
refusal and malformed state, partial byte/word knownness, exact carry-settled
noncontiguous stack knownness, unknown branch and return inputs,
mapping/alignment/overlap/wrap/alias cases, and every operation
budget prefix on both paths. The integration test enters through the existing
match tick with its source-proven `a0=0` and an explicit full-GPR fixture, uses
an explicit prior hot-start fixture, completes this production owner through
the `0x800796B8` call with branch-delay `a0=12` and JAL-delay `a1=0`, and proves the next stop is the
unresolved period-startup boundary.

Visual classification: `Gameplay shown: NO - no direct visual effect`. This
routine changes retained CPU camera state and invokes a typed camera service;
it contains no renderer or frame submission. Advancing court/player rendering
still depends on `0x800799CC`, the following period and simulation services,
and the match frame providers.

Manager verification passed 1,206 direct checks, 18 natural integration checks,
217/217 asset-free CTests, and 912 private original-instruction differential
cases covering all 61 instructions, full memory/GPRs, child-entry registers,
every camera byte and budget prefixes across both source branches.
The native input-driven run `game-entry-20260905-185916-82047520` records
`camera_startup_verified.json`. It composes the preceding recovered hot-start
owner, then projects only the evidenced `0x80068C2C` JAL and `0x80068C30`
A0-zero delay into that owner's returned full GPR state. The root entry and
`0x800799CC` service are explicit synthetic fixtures; the legacy tick's missing
prologue/GPR interface is not inferred complete. The chain reaches the next
period-startup call at `0x80068C4C`, with zero simulation steps/frame pumps.
The camera receipt proves 23 operations, six reads, sixteen stores, bytes
`E7/91`, and the child-mutated vector `FFFF1234,12345678,87654321`.
Before/after scanout SHA-256 is identical:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.

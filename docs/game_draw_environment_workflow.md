# GAMEONLY draw-environment submission recovery

## Boundary and evidence

This owner translates only GAMEONLY `0x80099ACC..0x80099B8F`, 196 bytes and 49 instructions. The fresh Ghidra listing is `.local/evidence/tipoff-recovery/game_80099acc.txt`; its instruction SHA-256 is `e3a8b21fbffb78d4de7709eabc4d916d87a024eb8092b9472d95627f1b8e40f4`. Known callers are `0x8002A048`, `0x8002A058`, `0x80048F4C`, `0x80048FA0`, `0x80047D94`, `0x80047DE8`, `0x80047E2C`, `0x80049264`, `0x800361C0`, `0x8002B8D8`, `0x8002B258`, `0x800A6874`, `0x800AA150`, `0x8002CFD0`, `0x8002D024`, `0x800A6608`, `0x8002B398`, and `0x800542C8`.

The ownership audit found typed call references but no complete source-faithful owner. The display-environment routine at `0x80099CA4` is a separate boundary.

## Source behavior

The routine allocates a wrapping 0x20-byte stack frame and saves `s2`, `ra`, `s1`, and `s0` in source order. It constructs `s2=0x800C55C2`, reads the debug byte, computes the unsigned `<2` predicate, and captures incoming `a0` in the branch delay slot. Debug values 0 and 1 skip diagnostics. Other values load the indirect target from `0x800C55BC`, pass the fixed format pointer `0x8002836C` and live `s1`, and retain every callback mutation.

The packet-builder call at `0x80099B20` receives `a0=live s1+0x1C` and delay-slot `a1=live s1`. After it returns, the owner prepares `a0=0x00FFFFFF`, `a1=live s0`, and `a2=0x40`; loads the packet and graphics table in source order; ORs the packet tag; and stores it before dereferencing the table's user and function words. The dynamic submission at `0x80099B58` receives the table user, live packet pointer, size 0x40, and delay-slot `a3=0`.

The final copy call receives `a0=callback-live s2+0xE`, `a1=callback-live s1`, and delay-slot `a2=0x5C`. Its `v0` is deliberately ignored: source `v0` becomes callback-live `s1`. The epilogue reloads `ra`, `s2`, `s1`, and `s0` through callback-live `sp`, advances `sp` by 0x20, and only then validates the `JR ra` target.

All guest additions wrap as `uint32_t`. Loads and stores preserve little-endian per-byte knownness, aliases, exact source order, alignment traps, and reached failure prefixes. OR `0x00FFFFFF` makes the low three output bytes known while preserving both the upper value and its knownness. A store to a region with `known=NULL` refuses before changing bytes if any consumed source byte is unknown.

## Typed children

| Call PC | Target | Arguments | Delay slot |
|---|---|---:|---|
| `0x80099B10` | indirect `[0x800C55BC]` | 2 | `a1=live s1` |
| `0x80099B20` | `0x8009A344` | 2 | `a1=live s1` |
| `0x80099B58` | indirect `[[0x800C55B8]+8]` | 4 | `a3=0` |
| `0x80099B68` | `0x8009CB0C` | 3 | `a2=0x5C` |

These original callees remain typed dependencies in this owner. The callback receives the full CPU machine and mapped memory and may synchronously mutate every GPR, HI/LO, and retained byte. A successful callback with malformed machine state produces `NBA97_TEXT_ARGUMENT` after preserving its reached prefix.

## Natural scene composition

`nba97_game_draw_environment_from_scene` composes the recovered scene owner at its two exact DRAW events: `0x80048F4C/0x80048F50` with `RA=0x80048F54`, and `0x80048FA0/0x80048FA4` with `RA=0x80048FA8`. Both carry one argument and target `0x80099ACC`. The adapter claims a malformed event whenever either the assigned kind or entry matches, so an accepting fallback cannot handle a malformed assigned boundary.

The scene owner has no HI/LO payload. The binding therefore accepts an explicit optional per-call provider. Without one, the child machine carries unknown HI and LO rather than invented values. Invalid incoming memory/register metadata and invalid provider masks are rejected before the parent register file changes. Nested owner failures remain available in `binding.result`, including `NBA97_TEXT_ARGUMENT` when the scene callback can report only refusal.

The natural fixture runs the actual `nba97_game_scene_startup` owner through both draw calls. Synthetic packet, submission, and copy services perform observable work; the fixture verifies the two tagged buffers, per-site HI/LO, both submissions, and the final copied 0x5C-byte environment.

## Validation

Asset-free always-active tests cover debug bytes 0, 1, 2, and 255; all four child PCs, targets, argument registers, return addresses, and delay slots; all 16 packet knownness masks; the preserved upper tag byte; packet/table aliases and store-before-dereference behavior; callback-live `s0/s1/s2/sp/HI/LO`; ignored copy return; every child refusal and malformed output; unknown `a0`, debug, dispatch table/function, `sp`, and final `ra`; misalignment, wrapping stack, unmapped memory, overlapping regions, late malformed knownness, unknown stores to `known=NULL`, truncated journals, deterministic replay, and every normal-path operation-budget cutoff.

## Classification

Gameplay shown: **NO - no direct visual effect**. The isolated callback records CPU packet submission and retained-memory changes. Native rendering requires the graphics submission backend and the adjacent display-environment integration.

Manager natural-caller integration replaces the typed copy with the recovered
BIOS trampoline, retaining BIOS byte transfer as a synthetic host service. It
checks the copy-budget failure after submission and preserves delay-slot T1/T2.
The native fixture composes scene, both display/draw calls, video query, GPU
control writes and the BIOS trampoline on the same mapped memory. Packet
construction, submission, MMIO consumption and BIOS transfer remain explicit
synthetic services. Original dispatch entries 0x8009B298 and 0x8009B1F8 were
checked in the private runtime table. Independent original comparison passed
3,856 cases across all 49 instructions and was rerun after the unknown-SLTIU
mask fix, covering full RAM/machine state, aliases and mutable live stacks.

Manager verification passed 7,869 focused checks, 30 natural-caller checks and
all 307 asset-free CTests. Strict C99 and progress/recovery/instruction/roster
freshness checks passed. Native run
`.local/verification/team_select/game-entry-20260906-031045-f8e92027`
recorded 98 scripted states and two complete draw submissions, each with
17 operations, nine reads, five stores and three callbacks. Packet tags became
0x12FFFFFF and 0x34FFFFFF; the final 92-byte cache matches the last environment.
Both CPU-only hashes were
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
User Setup remained displayed; no native match frame advanced.

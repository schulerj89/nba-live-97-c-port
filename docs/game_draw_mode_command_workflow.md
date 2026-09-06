# GAMEONLY draw-mode command recovery

## Boundary and evidence

`nba97_game_draw_mode_command` owns only GAMEONLY `0x8009A5E8..0x8009A643`, 92 bytes and 23 instructions. The fresh Ghidra listing is `.local/evidence/tipoff-recovery/game_8009a5e8.txt`; its instruction SHA-256 is `0b3c69b40bf5a16d3c424e16bb05db4eef1927a92e78d6d7764f6686a384cf20`. Callers are `0x8009A3C8` in the recovered draw-packet owner and `0x8009A314`. The routine has no callees. The ownership audit found caller references and the decomp inventory entry, but no earlier complete owner.

The manager's independent original-instruction differential passed 8,192 cases across all 23 PCs, all 34 machine words and masks, unchanged mapped memory, all 256 type bytes, full-word `a0/a1` branch combinations, random raw `a2`, and operation budgets zero and one. The receipt is `.local/evidence/tipoff-recovery/draw_mode_command_differential.json` in the main repository.

## Source behavior

The owner reads the display type byte from `0x800C55C0`, subtracts one with 32-bit wrapping, and uses unsigned `<2` to recognize only types one and two. Those two types produce:

`0xE1000000 | (a1 != 0 ? 0x800 : 0) | (a2 & 0x27FF) | (a0 != 0 ? 0x1000 : 0)`

Every other byte, including zero and 255, produces:

`0xE1000000 | (a1 != 0 ? 0x200 : 0) | (a2 & 0x09FF) | (a0 != 0 ? 0x400 : 0)`

The `a0` and `a1` predicates test their complete 32-bit words. A nonzero high byte therefore sets the flag even when the low byte is zero. Partial knownness can decide nonzero when any known byte is nonzero; a partially known all-zero representation remains unknown.

The `a1` branches execute `LUI v1,0xE100` in their delay slots. The `a0` branches execute the selected `ANDI v0,a2` in theirs. Unknown predicates retain those register effects before refusing. The final `OR v0,v1,v0` executes in the `JR ra` delay slot, so an unknown `ra` refuses only after publishing the complete command. `a0`, `a1`, and `a2` are unchanged; only `v0` and `v1` change; every other GPR and HI/LO passes through.

## Memory and adapter mapping

The fixed type address is resolved through validated `uint32_t` guest mappings. The single LBU preserves per-byte knownness and performs no store. A zero operation budget or unmapped byte retains the preceding `v0=0x800C55C0` address calculation. Malformed knownness is validated before replacing `v0`. The access journal records the exact one-byte read at `0x8009A5F0`.

`nba97_game_draw_mode_command_from_packet` claims the exact draw-packet child boundary: kind `CHILD_8009A5E8`, call PC `0x8009A3C8`, delay `0x8009A3CC`, entry `0x8009A5E8`, full-known `ra=0x8009A3D0`, and three arguments. Matching either assigned kind or entry requires complete metadata validation, preventing malformed assigned events from reaching an accepting fallback. Valid completion and every nested refusal prefix copy the full 32-GPR/HI-LO machine back to the packet owner.

## Validation and natural composition

The asset-free always-active focused suite covers all 256 display types; zero and nonzero high-word `a0/a1`; raw `a2` flag and mask boundaries; all 16 `a2` byte-knownness masks; all partially known-zero and known-nonzero branch decisions; unknown type, `a0`, `a1`, and `ra` prefixes; budgets zero and one; exact one-byte journal behavior; all 34 machine words and masks; malformed maps, knownness, zero register, and journals; overlap rejection; truncation; and deterministic replay.

The natural suite runs the actual draw-packet owner with the recovered draw-area-start, draw-area-end, recovered draw-offset, and draw-mode owners chained through their production adapters. Only the final texture-window child remains typed. It verifies all four recovered packet words, exact BM call metadata, the following JAL delay store, the six-word packet count, and HI/LO preservation. A zero BM budget proves the first three words remain stored, the BM word and texture child are not reached, and BG retains its allocated frame and `ra=0x8009A3D0` at the failed call.

## Classification

Gameplay shown: NO - no direct visual effect. The routine creates one CPU-side GPU command word. Visible output requires later packet submission and GPU consumption.

Manager validation passed 1,938 focused checks, 35 natural-caller checks,
all 321 asset-free CTests, strict C99/C++17 builds and all metadata/progress
freshness checks. Native scene composition uses actual scene, display, draw,
packet, E3/E4/E5/E1 helpers, video query, GPU command, BIOS trampoline,
graphics submission and packet-DMA owners on shared mapped memory. Both
E1 calls pack flags (1,1) and raw payload 1234 as E1000634, stored at 80021F18
and 80021F74, then submitted and copied with the environment. One texture
helper and scheduler/critical/device/BIOS services remain synthetic.
Native receipt: `.local/verification/team_select/game-entry-20260906-041734-8c3fb855/frames/draw_mode_command_verified.json`.
Both CPU hashes are
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The ignored local screenshot still shows User Setup, not gameplay.

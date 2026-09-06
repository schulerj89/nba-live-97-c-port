# GAMEONLY GPU control command recovery

`nba97_game_gpu_control_command` owns exactly GAMEONLY
`0x8009B16C..0x8009B193` inclusive: 40 bytes and 10 instructions. The boundary
comes from the fresh Ghidra listing
`.local/evidence/tipoff-recovery/game_8009b16c.txt`; its instruction-byte
SHA-256 is
`43224c6b6612d2c3440ea6e3a16f7e74f9b0d9711b10aba909f1d09cd97a73f2`.
The known computed callers are `0x80099D6C`, `0x80099F78`, `0x80099FA4`,
`0x8009A114`, and `0x800994D4`. There are no callees.

The C99 owner retains all 32 GPRs, HI, LO, and per-byte knownness. It first
loads the runtime GPU control-port address from `0x800C5694`, writes the complete
`a0` command to that mapped address, extracts the command high byte into `v0`,
sets `at` to `0x800E0000 + v0`, and writes the command low byte at
`0x800D8D94 + v0`. The port write therefore remains visible when an unknown
command class prevents the later cache address from being used. `at` retains
the pre-offset value from the source, and `a0`, the other GPRs, and HI/LO remain
unchanged.

The retained-memory mapper preserves little-endian loads and stores, alignment,
32-bit guest addresses, atomic refusal for partial stores into regions without
knownness storage, late-byte validation, aliases, and journal truncation. The
three mapped accesses form the complete operation budget. This leaf does not
decode GP1 commands, cast a guest address to a host pointer, or control a native
GPU.

The production adapter intercepts the four actual display-environment GPU
events at `0x80099D6C`, `0x80099F78`, `0x80099FA4`, and `0x8009A114`. It checks
the exact call PC, delay PC, `0x8009B16C` entry, kind, argument count, JALR
return address, full machine, and memory structure. Every other BA service is
forwarded through its typed callback. A stopped leaf publishes its complete
machine and memory prefix, and the composed wrapper returns the leaf result
rather than replacing it with a generic callback refusal.

The focused synthetic test covers all 256 command high bytes, low-byte and
signed extrema, all 16 `a0` known masks, all 34 machine words and masks,
budgets zero through three, unknown return address, partial stores, late-byte
invalid loads, alignment and mapping failures, pointer/cache aliases, runtime
pointer reload, truncated journals, malformed inputs, and deterministic replay.
The integration test runs the actual BA owner through the unchanged origin-only
path and the changed path containing all four GPU calls, checks mapped port and
cache effects, retains raw video and copy services as typed fixtures, and
verifies exact guards and failure prefixes.

Gameplay shown: **NO - no direct visual effect**. The evidence is the mapped
CPU/MMIO receipt and byte cache; scanout consumption is outside this leaf.

Manager integration also composes SetDispMask's source call at 0x800994D4.
Fresh Ghidra evidence for its 39-instruction caller has instruction SHA
28520655216df722415935c464f41c3d5921787e341e163afe573dcc44bd54f0.
That caller consumes only child V0 and the preserved SP/S0/S1 frame; omitted
GPRs and HI/LO are explicitly unknown in the adapter. The inline restricted
startup callback now invokes the complete owner, then applies host scanout
state to the completed GP1 write. The mapped port and cache writes belong only
to the new C owner. Main contains ownership and dispatch, no copied algorithm.
Natural tests exercise zero and nonzero mask values, exact return/frame state,
unknown omitted words and partial failure. The native scene composition also
runs the display, video-query and BIOS-trampoline owners through all five GPU
commands with mapped synthetic MMIO. Independent original comparison passed
8,192 cases across all ten instructions, full machine/masks, mapped RAM/port,
all command masks, aliases, budgets and unknown return PCs.

Manager verification passed 44,865 focused checks, 58 natural-caller checks
and all 305 asset-free CTests. Strict C99/C++17 and progress/recovery/
instruction/roster freshness checks passed. Native run
`.local/verification/team_select/game-entry-20260906-030554-d9447e3e`
recorded 98 scripted states and five complete GPU writes, each with one read
and two ordered stores. Final mapped port was 0x0800002E; cached command
bytes were 64/28/31/2E (hex). Both CPU-only hashes were
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
User Setup remained displayed; this is not an advancing match.

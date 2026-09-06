# First-period presentation finish recovery

`nba97_game_period_presentation_finish` owns GAMEONLY
`0x8002DDCC..0x8002DE33` (104 bytes, 26 instructions). The fresh Ghidra
listing is `game_8002ddcc.txt`; its instruction bytes have SHA-256
`eacc7dbe8c2324ac5d4cd30b204d9e41b92260d9fd8e01d32ffc33db27a6712b`.

The routine loads the word at `0x8001EDE8`, and its load delay allocates a
0x18-byte frame. It saves `ra`, clears presentation byte `0x800EB680`, writes
active marker 1 at `0x80109AFC`, and publishes the captured word at
`0x80109AE4`. It calls typed service `0x80044550`, then rereads live byte
`0x800FDB78`. A zero byte calls typed service `0x80046C2C`; any known nonzero
byte skips it. The routine finally clears `0x80109AFC`, reloads `ra` through
callback-live `sp`, applies the load-delay stack restoration, and executes
JR/NOP.

Both child boundaries receive the complete 32-GPR and HI/LO machine after JAL
publishes `ra`; their NOP delay slots add no mutation. A child may change every
register, the gate byte, live `sp`, and the saved return word. The first child
therefore controls the later gate even when its entry value differed. The
nonzero path returns the zero-extended gate in `v0`, while the zero path leaves
the second child's raw `v0` live. The final active-marker clear precedes all
epilogue failures.

Guest addresses remain validated `uint32_t` values over mapped retained-memory
regions. Loads and stores preserve little-endian per-byte knownness, partial
source words, access order, alignment traps, and exact budget or failure
prefixes. Stack and fixed globals may alias, and 32-bit stack arithmetic wraps.

The natural adapter claims first-period startup's assigned kind, entry, call
PC `0x80067424`, delay PC `0x80067428`, or JAL return `0x8006742C`, then
requires the full exact boundary before invoking this owner. Other recovered
first-period children use typed fallback. The parent exposes only GPRs, so the
adapter marks unavailable HI/LO explicitly unknown. It copies back any valid
GPR prefix even when an accepted child made HI/LO invalid; an invalid child
GPR file is never published into the parent.

Focused runtime-generated tests cover all 256 gate values, callback-mutated
gate decisions, exact calls and access order, every operation cutoff, both
child refusals and invalid machines, all source-word known masks, load and
store atomicity, raw memory without a knownness plane, mutable SP/RA/HI/LO,
stack/global aliases, wrapping and aligned access, unknown or unaligned JR,
untouched registers, malformed metadata, and deterministic full state.
Natural tests execute the actual `0x800673F0` owner for both its skipped and
nonzero presentation paths, verify real source order, nested failures, valid
GPR transport after invalid HI/LO, repeated binding use, and every assigned
identifier guard.

The manager-owned independent original-instruction differential passed 5,376
cases across all 26 source PCs, all 34 machine words and masks, full 2 MiB RAM,
every callback-entry machine, all 14 access/call cutoffs in the first 256
fixtures, every gate byte, callback-mutated gates, stack-frame copies, and
RA/HI/LO effects. The ignored receipt is
`period_presentation_finish_differential.json`.

Gameplay shown: NO - no direct visual effect. The renderer services at
`0x80044550` and `0x80046C2C` remain typed boundaries, so this integration
proves CPU state and frame identity rather than an advancing gameplay frame.
The owner intentionally clears `0x800EB680`; later announcement logic must
observe the resulting zero instead of a retained fixture value.

The focused suite passed 1,194 checks and the natural caller suite passed 448.
Manager integration passed all 347 asset-free Debug CTests (5.52 seconds),
progress/recovery validation, instruction-semantics freshness, and roster checks.
Native input run `game-entry-20260906-063759-d9dbcb7e` takes the actual first-period
presentation branch on shared diagnostic RAM. It clears the original 255 flag,
publishes explicit source-word fixture 0x80170000, observes active marker one
inside both typed presentation children, then clears that marker. The receipt
records 10 operations, 3 reads, 5 stores, two exact child calls, restored RA
0x8006742C and SP 0x801FFED0, with HI/LO explicitly unknown. The later real
announcement now sees the cleared flag and takes mode 2 in both period cases.
CPU-only before/after frames share SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The visible frontend remains User Setup; no advancing match is claimed.

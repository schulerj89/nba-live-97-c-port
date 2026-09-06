# GAMEONLY match-buffer rewind recovery

`nba97_game_match_buffer_rewind` owns GAMEONLY
`0x80076AD0..0x80076B27` inclusive: 88 bytes and 22 instructions. Fresh Ghidra
evidence is recorded in `game_80076ad0.txt`, with instruction SHA-256
`520b041e803da61c6611b2b0a99b3da28da49315300930bd305b92ad7a4f54f6`.
Known callers are `0x80065AE8`, `0x80064370`, `0x8002E1B8`, `0x80076B50`,
`0x80078668`, `0x800792A8`, and `0x8002E078`. Its only callee is the recovered
zero-fill entry `0x800A3A74`, called at `0x80076AF8`.

The C99 owner receives the complete live GPR/HI/LO machine, retained mapped
memory with per-byte knownness, a typed zero callback, an operation budget, and
an access journal. It performs eight direct accesses and one callback in source
order. The routine reads `0x800FA004`, applies the load-delay stack decrement,
saves `ra`, and copies the loaded word to `0x800FA00C` and `0x800FA010`. The JAL
assigns `ra=0x80076B00` before its delay slot assigns `a1=4`. After the child
returns, the owner clears `0x800FE860`, `0x8002148C`, and `0x800FE864`, reloads
`ra` through callback-live `sp+0x10`, restores `sp`, and validates the indirect
return target before the JR NOP.

The production adapter routes the exact length-four child event through the
existing `nba97_game_memory_zero` owner. Entry `0x800A3A74` forces `a2=0`, and
length four takes the optimized path rather than the small-byte loop. With the
aligned fixed destination `0x800F1918`, the original path issues an SWR over
all four bytes, computes `t0=4`, `t1=0`, `a1=-4`, and returns `a0` to the same
address before an overlapping four-byte SWL. Zero budgets 0, 1, and 2 therefore
expose the pre-store, first-store, and completed two-store prefixes. The bridge
retains the incoming `v0` word and exact byte mask because the zero routine
never assigns it. It also transports all source-proved `at/a2/t0/t1/t2/a0/a1`
states without copying the zero algorithm.

`nba97_game_match_buffer_rewind_from_match_state_reset` accepts only the
mode-98 reset event at `0x80065AE8`, including its delay PC, entry, invocation,
argument count, and assigned return address. The wrapper executes the real
recovered match-state reset, composes this owner for mode 98, and keeps every
other reset child behind the parent's typed callback. The isolated BN wrapper leaves non-98 mode at its original `0x8006432C`
boundary. Manager composition additionally routes the actual buffer initializer
call at `0x80064370` through the same rewind/zero owners, with exact event
guards and full machine conversion between the two typed interfaces. Bindings can
be reused, with cumulative owner and zero invocation telemetry.

The focused synthetic test covers the exact access and callback sequence,
budgets 0 through 8 with source stop PCs, all sixteen loaded-pointer masks, all
sixteen saved-return masks, partial stores, stores without a knownness map,
malformed first and final loads, callback refusal, malformed GPR/HI/LO callback
state, full callback-live machine changes, stack and pointer aliases,
alignment, mapping, 32-bit stack wrap, JR alignment, atomic failure prefixes,
and deterministic machine/memory/knownness repeatability. The natural test
executes the real reset owner in mode 98 and non-98 mode, validates exact event
guards and repeated bindings, and runs the actual zero owner through its three
meaningful budgets. All fixtures are generated in memory and contain no retail
assets.

Visual classification: **Gameplay shown: NO - no direct visual effect**. The
routine updates retained CPU-side buffer pointers and flags. The manager owns
same-memory native reset composition and pixel-identity evidence; this module
does not render or advance gameplay.

Manager verification: 423 focused checks, 412 natural BN and BS integration
checks, all 335 asset-free CTests, strict C99, and freshness checks passed.
Private original-instruction comparison passed 4,352 cases across all 22
instructions, full 34-word machine/masks, callback entries, aliases and 2 MB
RAM. A separate 64-case comparison proved the exact length-four zero adapter
through all 27 reachable PCs with incoming masks and all store budgets.

Native run `game-entry-20260906-053327-9e3629af` drove 98 frames through the
port input API and displayed User Setup. The actual reset/buffer initializer
invoked rewind with the preceding pointer `0x800CCC00`; both cursor words
match it and all four flag/zero destinations are cleared. Runtime flag
sentinels are explicitly diagnostic. The owner logged nine operations, two
reads, six stores and one callback. Its zero child logged two stores totaling
eight written bytes over the same four-byte range. The ignored receipt is
`frames/match_buffer_rewind_verified.json` under that run.
Before/after CPU frame hashes both equal
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
No advancing native match or gameplay is claimed.

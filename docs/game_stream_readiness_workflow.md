# GAMEONLY stream readiness predicate recovery

`nba97_game_stream_readiness` owns the exact inclusive GAMEONLY span
`0x80088D0C..0x80088D7B`, 112 bytes and 28 words. Ghidra identifies a
104-byte, 26-instruction function body with SHA-256
`abb0ef9b5d11169191ecf6b120604cfd96fa6b60c37c2dcc69a3c87931a7545c`.
The complete raw span has SHA-256
`e23900b4a5d3149a9da1e4d35da1cc34ec44377f1fd4fba1a3539c70d8660e76`.
The two-word difference is the unreachable `J 0x80088D64` / `NOP` pair at
`0x80088D50/0x80088D54`; it is accounted for as inert source data inside the
owned span. Known callers are `0x8002A2EC`, `0x800448B8`, `0x80078B18`, and
`0x800805D8`. The sole child is `0x80084448` at `0x80088D30`.

The owner allocates a 0x18-byte frame, saves `ra` and `s8`, and sets `s8` to
the frame. It reads the signed halfword at `0x800F0FDC`. Zero returns zero
without calling the child. A nonzero flag invokes the no-argument typed child;
its signed result is compared with two. Results `INT_MIN` through one return
one, while two through `INT_MAX` return zero. Negative child results therefore
mean ready and are not treated as native failures.

The epilogue first moves callback-live `s8` into `sp`, then reloads `ra` and
`s8`, advances `sp` by 0x18, and consumes restored `ra` at `JR`. The callback
may mutate all 32 GPRs, HI/LO, memory, the frame words, `sp`, and `s8`.
Consequently the restored frame can differ from the entry frame. Stack/global
aliases, wrapping ADDIU addresses, little-endian access order, alignment traps,
and callback mutations remain observable.

Retained values carry one knownness bit per byte. `LH` sign-extension derives
known upper bytes from the source high byte. `SLTI` always makes `v1`'s upper
three bytes known zero, even when its low Boolean byte is undecidable. Unknown
branches stop only after these preceding and delay-slot effects. Unknown saved
values can be stored when a knownness bitmap exists; an all-known backing
without such a bitmap refuses an unknown store rather than inventing bytes.
Mapped guest addresses remain `uint32_t` values and are never host pointers.

`nba97_game_stream_readiness_from_match_audio_service` accepts only AB's exact
full-machine `0x8002A2EC -> 0x80088D0C` event with NOP delay at `0x8002A2F0`,
no arguments, and JAL `ra=0x8002A2F4`. The composed adapter runs the actual AB
owner with the existing AC clock-read and X stream-status owners, and replaces
only that readiness boundary. AB's remaining services and AD's `0x80084448`
child stay explicit typed callbacks.

The runtime-generated focused tests cover flag `0/1/FFFF/8000`, child signed
extremes and the `1/2` boundary, exact call metadata and access order, all
operation-budget prefixes, callback refusal and malformed machines, mutable
GPR/HI/LO/sp/s8/frame state, stack/global aliasing, partial knownness in both
branches, unknown return address, alignment, mapping, wrapping address space,
and invalid metadata. The integration test executes the actual recovered AB
`0x8002A2EC` call through production AC/X/AD adapters. It uses synthetic memory
only; no retail assets or binary fixtures are checked in.

Visual classification: **no direct visual effect**. This CPU predicate reads
retained stream state and a typed readiness service. It draws nothing and does
not establish audible playback or advancing gameplay. Identical native frames
are expected; state and event receipts are the evidence.

Manager acceptance passes 220 focused checks, 17 natural integration checks,
strict C99 and all 251 asset-free CTests. The private original-instruction
comparison passes 1,728 cases across all 26 reachable PCs, full memory, all
GPRs and HI/LO, callback entry state, every budget prefix, signed cases,
aliases and mutable frame selectors. Progress and metadata checks pass.

Gameplay shown: NO - no direct visual effect. Native input-driven capture
'game-entry-20260905-214114-f2834f09' reaches the actual AB call at 0x8002A2EC.
With explicit enabled flag 1 and typed child response 1, the new owner returns
1 after six operations, three reads, two stores and one child call. Its frame
is 0x801FFEB0 and restored caller return is 0x8002A2F4. Diagnostic frame hashes
match 391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d.
The separately captured frontend still shows Boston/Chicago User Setup.
All media/logs remain ignored; no advancing match or audible playback is claimed.

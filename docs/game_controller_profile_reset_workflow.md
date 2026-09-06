# GAMEONLY controller-profile reset recovery

## Source boundary

This module owns the complete GAMEONLY routine at
`0x80083490..0x800835C3` (308 bytes, 77 instructions). The fresh Ghidra
listing is `game_80083490.txt`, with instruction SHA-256
`8ef146f2e6f42e6a8790aadee0043a6dd9c736133f98a635b8dba5d9c9c76c8d`.
The sole source caller is the recovered match-state reset at `0x80065A38`.
The sole direct child is the existing memory-zero entry `0x800A3A74`, called
at `0x800834D8` after the delay slot assigns `a1=0x24`.

The similarly named controller initializer at `0x80065328` owns another
routine and does not overlap this source boundary.

## Preserved execution

The C99 owner transports all 32 GPRs, HI/LO, and one knownness bit per source
byte. Guest addresses are wrapping 32-bit values and every access is validated
against retained mappings. The source's six frame stores and six live-SP
restores remain explicit.

Each iteration sign-extends the callback-live controller index, derives the
120-byte record, and invokes the zero child for its 36-byte header. The owner
then reads `0x80021DDE + s0` even when the entry override is nonzero. A negative
signed selection skips the tail copy. Otherwise, a nonzero signed profile flag
chooses the selected profile's `+0x22` data; a zero flag or entry override uses
`0x800BC94C`. The copy remains 59 ordered byte loads and stores, so forward and
backward overlap retain the original alias behavior.

The loop condition uses callback-live `s1` as a signed 16-bit value. A child
can therefore relocate the frame, change later addresses, skip or repeat
records, or create a runaway loop. `operation_budget` bounds such execution
while returning the exact completed prefix. All branch and JAL delay effects,
including the final JR NOP before an unknown-ra refusal, remain visible.

## Native composition

`Nba97GameControllerProfileResetBinding` accepts only BN's exact full-machine
event at `0x80065A38/0x80065A3C`, entry `0x80083490`, return address
`0x80065A40`, and one argument. The zero callback composes the existing
`nba97_game_memory_zero` owner. For the source-proven fixed count of `0x24`, it
publishes the exact `at/a2/t2` zero prefix, retains partial `v0` knownness and
HI/LO, updates `t0/t1` only after the first successful SWR, and transports the
zero owner's working `a0/a1` on every return or bounded stop.

`nba97_game_match_state_reset_with_controller_profile_reset` routes only that
one BN child through BP and leaves every other BN child behind the parent's
existing typed callback. A nested status is promoted at the natural boundary
so a zero-child budget or mapping failure is not hidden as generic refusal.

## Asset-free verification

The focused executable generates all memory and profiles at runtime. It covers
the override and selected-profile paths, flag zero/nonzero, signed selection
bytes 127/128/255, negative and wrapped live indices, eight canonical records,
all 59 copied bytes and sentinels, access order, both overlap directions,
full-machine callback mutation, every callback refusal, malformed callback
state, unknown branch/load/store prefixes, unavailable-knownness atomic stores,
later-byte metadata failure, all operation-budget prefixes, runaway bounding,
invalid/overlapping/missing mappings, alignment, SP wrap, partial masks, and the
unknown-ra delay prefix. It passes 941,357 assertions under strict C99/C++17
warning-as-error builds.

The natural integration executable compiles the frozen BN owner and exercises
its real `0x80065A38` event. It proves eight actual zero-owner compositions,
record contents, BN continuation and return state, plus owner-budget and nested
zero-budget failure prefixes before BN's post-call stores. It passes 834
assertions. Both tests are deterministic and use no retail payload or asset.

The private original-instruction differential passes 3,136 cases covering all
77 source instructions, all 34 machine words and masks, the full 2 MiB mapped
RAM fixture, every callback-entry machine, all profile branches, callback-live
`s0` through `s4`, `sp`, HI/LO, and access/call budget prefixes. Its receipt is
`controller_profile_reset_differential.json` under ignored local evidence.

Gameplay shown: NO - no direct visual effect. This CPU-only reset changes
retained input-profile state; no renderer or capture path is part of the
assigned seven-file boundary.

Manager verification also compared the production zero bridge against the
original zero entry in 768 cases for the fixed 36-byte count, all destination
alignments, all V0 byte masks, partial T0/T1/HI/LO masks, full machine and 2 MiB
memory, and every first/middle/tail store-budget prefix (34 reachable PCs).
The original controller-profile comparison passed 3,136 cases across all
77 PCs, all 34 words and masks, full memory, callback-entry state, all profile
branches, live S0-S4/SP/HI/LO mutations and operation prefixes.

The native initializer/reset capture replaces its 83490 typed response with
the actual owner and zero adapter on the same retained memory. Runtime-generated
profiles exercise selected, default and negative selections: eight headers
clear, six tails copy 354 bytes, and two tails retain their sentinels. The
receipt records 742 operations, 374 reads, 360 stores and eight zero calls.
Other team/period services remain typed fixtures; the match does not advance.

Manager checks passed: 941,357 focused checks, 834 natural-caller checks,
327 asset-free CTests, strict C99/C++17 and all metadata freshness checks.
Large synthetic fixture buffers use heap storage, so the default MSVC Debug
stack size works without a linker override. Native evidence is ignored under
.local/verification/team_select/game-entry-20260906-045535-dbdfe7ae.
Both CPU scanout hashes equal
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d.
The separate frontend screenshot shows User Setup only.

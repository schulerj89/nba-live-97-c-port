# GAMEONLY period startup recovery

`src/recovered/game_period_startup.c` owns the complete GAMEONLY routine at
`0x80067468..0x8006754F` (232 bytes, 58 instructions). The boundary comes from
the fresh Ghidra listing `game_80067468.txt`, whose recorded routine SHA-256 is
`fc4dfb230d9560bd6c0b0fbd88158f007b77333f0f4ab43f651a75ea3bb1d426`.
The only source caller is the recovered match tick at call PC `0x80068C4C`.

The owner saves `ra` and `s0`, calls the period and attribute services, and uses
the signed `LH` at `0x800FDB68` to choose exactly one of `0x800673F0` and
`0x80067194`. Its JAL delay slots remain visible: the first saves `s0`, the
nonzero branch assigns `a0=1`, and later calls assign `s0=1`, `a1=-1`, and
`a0=15`. After the last setup call it reloads the live pointer at `0x80020C14`,
stores live `s0` at `0x800FDB92`, publishes the pointer at `0x800FDC48`, pumps
`0x8002DD84`, and only then stores the pump's live `s0` at `0x800FDB6C`. The
four `0x80076B28/0x80076B3C` calls and two `0x800A584C` calls remain separate.
Every nonzero unsigned `LHU` value at `0x8001EDEC`, including nonboolean values,
reaches `0x80035678`.

All children use one typed full-GPR callback. Existing owners for `0x80065DB0`,
`0x80063EDC`, and the frame-pump work reached from `0x8002DD84` expose state or
service APIs that cannot preserve this caller's complete GPR file and mutable
guest stack. The period-startup owner therefore does not guess ABI-preserved
registers or duplicate those algorithms. A later compatible adapter can bind
them through the same callback.

`src/game_period_startup_adapter.cpp` checks the natural match-tick event and
requires a complete, independently source-proven `Nba97GamePeriodStartupContext`.
The legacy tick service event carries only two scalar argument slots and has no
SP or full register output, so the adapter rejects an unproven context instead
of constructing one from absent fields. It maps owner failures into the match
tick result vocabulary while retaining the exact owner progress.

The focused synthetic test covers both signed selector paths, negative values,
the optional nonboolean flag, exact call/argument/delay order, live pointer and
`s0` mutation around the frame pump, all-GPR propagation, mutable/unknown stack
state, restored unknown `ra`, mapping/alignment/wrap/alias failures, malformed
callbacks, every child refusal, and all 24 operation-budget prefixes. The
integration test supplies an explicit child entry register context, reaches the
owner from `nba97_game_match_tick`, completes it, and stops at the next missing
tick service. All fixtures are generated in memory and use no retail asset.

Visual classification: `Gameplay shown: NO - no direct visual effect`. This
routine changes retained CPU period/frame state under typed synthetic services;
it does not itself establish an advancing rendered court or player frame.

Manager verification passed 164 direct checks, 14 natural integration checks,
213/213 asset-free CTests and 832 private original-instruction differential
cases covering all 58 instructions, full memory/GPRs and every budget prefix.
The native input-driven capture `game-entry-20260905-184239-440381ac` records
`period_startup_verified.json`: signed selector `0xFFFF8000`, 13 child calls,
23 operations, five reads/five stores, ball pointer `0x80123400`, and child-live
halfwords `0x4321` then `0x8765`. The production tick adapter uses an explicitly
synthetic full entry context and preceding services. It stops before simulation
service `0x80068CEC -> 0x80067550`; no simulation or frame pump is completed.
The before/after scanout SHA-256 is identical:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.

# GAMEONLY clock violations recovery

`nba97_game_clock_violations` owns GAMEONLY `0x80067D38..0x8006801B`
(740 bytes, 185 instructions). The boundary comes from the fresh read-only
Ghidra listing `game_80067d38.txt`; its instruction-byte SHA-256 is
`6f91de08516306fa3d3a827c48ed8460185386a6909d3f627b89f0378fde50ae`.
The sole caller is the recovered match tick at `0x80068D64`.

The strict C99 owner retains all 32 GPRs with per-byte knownness and carries HI
and LO as independently mutable state. It reads the main clock before allocating
the 0x18-byte frame, saves old `s0`, captures signed delta from `a0`, and spills
`ra` in the main-clock branch delay slot. Thus even a zero or unknown main-clock
exit exposes the exact completed stack prefix. The epilogue reloads `ra` and
`s0` through live, child-mutable `sp`, advances `sp`, and refuses an unknown JR
target only after preserving that prefix.

The first independent sequence requires a zero shot clock and nonzero unsigned
byte at `0x80021D92`. A nonnegative owner at `0x800FDBCC` triggers directly. A
negative owner instead follows the unchecked live ball pointer at `0x800FDC48`,
arithmetic-shifts the signed word at `ball+0x10` by eight, requires a result below
49, and requires signed velocity at `ball+0x18` to be nonpositive. Team zero
requests `0x80029590(11)` and selects 5000; any nonzero signed team halfword
requests event 12 and selects 20000. The sequence then calls `0x800295C8(a0)`,
`0x80062300(10)`, and `0x80062660`, before storing state 3.

The phase-82 sequence reloads every gate: nonnegative owner, zero actor halfword
at unchecked `0x800FDC34+0xA0`, state 2 at `0x800FE884`, and zero block at
`0x800FE88E`. It subtracts live `s0` from unsigned timer `0x800FDBA8`, stores the
wrapped low half, then tests that half as signed via the source shift. Underflow
always clears the timer. If unsigned enable byte `0x80021D90` is nonzero, the
team branch delay first clears phase through the live `a0` phase pointer, then
the same event/duration pair runs with `0x80062300(11)`. The final state becomes
1 when the live old state is signed below 2, otherwise 3.

The third sequence independently reloads signed phase and runs only below
`0x80` with zero `0x800FE8E0`. It applies the same wrapped halfword subtraction
to `0x800FDBAA`. Underflow always clears that timer; nonnegative owner plus
nonzero unsigned byte `0x80021D91` dispatches the event/duration pair,
`0x80062300(12)`, and `0x80062660`, then stores state 4. Callback mutations of
`s0`, all other GPRs, HI/LO, stack saves, pointers, flags, and globals remain live
for every later instruction.

All four children remain typed full-machine boundaries. The existing
`nba97_game_gameplay_audio` owner covers `0x80029590` request routing through a
narrow access/result API, but it cannot expose the complete caller GPR, stack,
HI/LO, refusal, or instruction-prefix state required here, so the adapter does
not fabricate a bridge or copy that algorithm. `0x800295C8` is a separate
unowned two-instruction no-op and deliberately remains a callback. `0x80062300`
and `0x80062660` are also unresolved. A successful fixture callback proves only
CPU orchestration, never audible output.

`nba97_game_clock_violations_from_match_tick` binds only the natural
`0x80068D64 -> 0x80067D38` event. The legacy tick service carries scalar
arguments but no machine state, so production callers must provide an
independently established entry machine and the adapter checks exact `a0` and
JAL-produced `ra`. The integration fixture composes the existing complete match
clocks owner, explicitly models the evidenced intervening `0x80068D60` move and
`0x80068D64` JAL state, and does not infer missing GPRs from the legacy tick API.

The runtime-generated focused tests cover the zero-main prefix, shot and ball
gates, signed height/velocity boundaries, both team selections, every phase-82
gate, halfword wrap/sign boundaries, enable bytes 0/255, signed state selection,
final phase/owner/block gates, all three effect sequences, child mutation of
live `s0`/`sp`/saved words/GPRs/HI/LO, stack/global aliasing, unknown branches
and pointers, alignment, callback refusal, malformed inputs, and every operation
budget prefix. The natural integration test reaches the production adapter at
the actual tick call and checks nested refusal propagation. All fixtures are
synthetic and asset-free.

Visual classification: no direct visual effect. This routine changes retained
CPU timer, phase, and violation state and reaches typed effect services. It does
not render pixels, prove audible output, or establish an advancing match.

Manager integration uses the production tick event and carries the complete
match-clocks output through the actual 68D60 MOVE and 68D64 JAL/NOP. The initial
machine and additional rule fields remain explicitly synthetic. Three native
cases cover phases 0/81/82, including the phase-82 timer changing phase to zero
and enabling the final independent timer sequence. The next missing call is
0x80068D6C -> 0x80067664. No live match continuation is claimed.

Manager validation: 326 focused checks, 15 natural integration checks, strict
C99 compilation, and all 239 asset-free CTests passed. Private original-byte
comparison passed 3,696 cases across all 185 instructions, comparing full memory,
all 32 GPRs, HI/LO, child-entry state, and bounded prefixes. The native verifier
captured 98 input-driven states and matching before/after diagnostic SHA-256
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d;
CPU receipts prove timer/state changes. Separate User Setup menu proof remains
ignored local evidence. Gameplay shown: NO - no direct visual effect.

# GAMEONLY first-period startup recovery

`src/recovered/game_first_period_startup.c` owns the complete GAMEONLY routine
at `0x800673F0..0x80067467` (120 bytes, 30 instructions). The boundary comes
from the fresh Ghidra listing `game_800673f0.txt`, whose recorded routine
SHA-256 is
`bb4e8f79b42f234c61f2e6f1529ca92ded800e35cfdbeb4f2ce96dea0d0e5285`.
The sole observed caller is the recovered period-startup owner at call PC
`0x80067494`, reached when its signed period selector is zero.

The owner creates a `0x18`-byte stack frame, saves `ra`, calls `0x800295D0` and
`0x8002A244`, then reads the live unsigned byte at `0x800EB680`. Any nonzero
byte calls the frame pump at `0x8002DD84`, calls `0x8002DDCC`, and clears the
halfword at `0x800FDB4E`; zero skips all three effects and leaves the halfword
unchanged. It then calls `0x8002A254` with `a0=1` assigned in the JAL delay,
forms `v0=-1`, forms `at=0x80100000`, stores the low halfword at `0x800FDB94`,
and calls `0x80065DB0` followed by `0x8007EF4C`. The final `ra` load uses the
child-mutable `sp`; `sp+=0x18` still executes before an unknown restored `ra`
is consumed by `JR`.

All seven children use one typed callback carrying the complete 32-register
file and retained-memory view. Existing recovered interfaces associated with
`0x80065DB0` and `0x8002DD84` are narrower: they do not report this caller's
complete live GPR file, mutable `sp`, or exact failed prefixes. Binding them
would require guessing unreported ABI effects, so this owner leaves those
events explicit instead of duplicating either algorithm. The same compatibility
limit applies to any future child adapter that cannot return all GPR and stack
effects.

`src/game_first_period_startup_adapter.cpp` accepts only the natural
period-startup event (`0x80067494 -> 0x800673F0`). That parent callback already
carries shared retained memory and the full live GPR file, so the adapter can
run the production owner directly and return its complete or failed prefix to
the parent. No entry registers are reconstructed from a narrow event payload.

The focused synthetic test covers flags `0`, `1`, `255`, an unknown flag byte,
the exact optional store behavior and access order, every call and JAL delay,
all-register mutations, raw final-child `v0`, mutable and unknown stack state,
unknown restored `ra`, mapping/alignment/wrap/alias failures, malformed inputs
and callbacks, every child refusal, and all operation-budget prefixes on both
branches. The integration test drives the recovered period-startup owner
through its actual zero-selector `0x80067494` event, composes this owner through
the production adapter, and keeps every other child as an explicit synthetic
fixture. All fixtures are generated at runtime and contain no retail asset.

Visual classification: `Gameplay shown: NO - no direct visual effect`. Under
typed frame and presentation services this routine changes retained CPU state;
it does not itself advance or render court and player state.
# Native capture integration

The existing input-driven game-entry verifier now runs the production period
startup and first-period adapter with selector zero and presentation bytes 0
and 255. Separate runtime-generated full-GPR fixtures preserve the previous
negative-selector diagnostic. The new cases verify 9/12 operations, 5/7 child
calls, the optional halfword clear, marker `0xFFFF`, and mutable stack return.
Remaining child services are explicit fixtures, including presentation and
tip-off setup; neither case establishes an advancing match. The verifier writes
`first_period_startup_verified.json` and identical before/after scanout hashes
only to its ignored native capture directory.

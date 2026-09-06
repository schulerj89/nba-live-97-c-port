# GAMEONLY audio stream pump recovery

`nba97_game_audio_stream_pump` owns GAMEONLY
`0x80083EEC..0x800840EF` (inclusive), 516 bytes and 129 instructions. The
private source boundary is the fresh Ghidra listing `game_80083eec.txt`, with
routine SHA-256
`9ba0ebb07d46228b3571c86cbf3bb9de39e23cb0af0e97ab80dabee1b8c2e4ab`.
The observed callers are `0x8002A320`, `0x80036EC4`, `0x80036F08`,
`0x80044898`, `0x80046CCC`, `0x8006764C`, `0x800444B8`, `0x8004575C`,
`0x80078B4C`, `0x800801E4`, `0x8008021C`, `0x80080388`, `0x80080430`,
`0x80080468`, `0x80088994`, and `0x800805FC`.

The owner creates a 0x20-byte frame, saves `ra` and `s8`, initializes its
return slot, and calls the `0x8008472C` gate. A signed-negative gate result
returns zero without reading that slot. Other results load the live flags byte
at `0x800C43B0` and preserve the source's two `ANDI` operations and complete
0/1/4/5 dispatch tree.

Mode 5 repeatedly calls `0x80086190`, reloads the handle at `0x800C438C`, and
queries `0x80088018`. The raw status is stored at live `s8+0x10`. The handler
test reloads this word once for signed `> 0` and again for signed `< -9`; an
eligible status is passed to `0x800840F0`, whose result is stored at live
`s8+0x14`. The loop predicate then performs its own two independent reloads.
A terminal status from -9 through zero writes zero, so the normal terminal pass
overwrites an earlier handler result.

Mode 4 reloads the flags byte and requires bit 1 before entering its loop. Its
handler condition is preserved exactly: the first status load must be positive
and the second must be less than -9. This conjunction is contradictory when
retained memory is stable. The `0x80088288` call and its live handle/status
loads remain represented in the translated source, but tests do not claim that
the original unreachable arm executed. The later loop condition still uses
the source's signed `> 0 || < -9` behavior.

All five child boundaries receive all 32 live GPRs and may mutate registers and
retained memory. Every status and return-slot access therefore recomputes its
address from the current live `s8`. The epilogue moves live `s8` into `sp`,
reloads `ra` and saved `s8` from that frame, then advances `sp`. Guest memory is
validated, little-endian, 32-bit addressed, and tracked with one knownness bit
per byte. Unknown signed predicates stop at the consuming branch after its NOP
delay slot; `ANDI` and `SLTI` retain known-zero upper bytes.

`game_audio_stream_pump_adapter` composes both natural full-GPR speech-startup
events at `0x800801E4` and `0x8008021C` with this owner. All other speech
children remain explicit typed services. A nested failure returns the exact
pump prefix in adapter progress and causes the parent callback boundary to
refuse rather than fabricating a successful audio update. The separately
assigned controller-reset caller at `0x8006764C` is not present in this base and
is not duplicated here.

The focused test uses runtime-generated retained-memory fixtures. It covers
negative/zero/positive gate results, all 256 raw flag bytes, signed status
values -10, -9, -1, 0, 1, `INT_MIN`, and `INT_MAX` in both modes, terminal and
bounded repeat behavior, handler return-slot and live-frame mutation, saved
register reloads, full-GPR prefixes, byte knownness, unknown branches,
alignment/mapping/overlap failures, reachable child refusals, malformed child
registers, and every operation-budget prefix. The integration test executes
both real speech call PCs through the production adapter and checks nested
limit and refusal prefixes without retail assets.

Visual classification: **no direct visual effect**. This routine changes CPU
audio-service state and retained words. Typed fixtures do not establish audible
playback, and the routine does not render or advance gameplay. Native capture
and shared build registration remain part of manager-owned integration.

Manager integration also binds the already recovered controller-reset call
`0x8006764C` through the same production adapter module. Its asset-free test
executes the complete controller parent, verifies nested return state and all
eight halfword clears, and checks pump budget/refusal prefixes.

The native input driver composes two speech pump calls (raw flags 5 and 6)
and three controller pump calls. Flag 5 returns handler value `0x12345678`
on a positive status, then overwrites it with zero on the terminal pass;
flag 6 repeats on -10 and terminates on zero without entering the contradictory
handler. The CPU receipts contain 26/23 operations respectively. Lower stream
services and initial fixture state remain explicit synthetic dependencies.
Diagnostic before/after scanouts have identical hashes; separate frontend
frames show User Setup. No audible playback or advancing gameplay is claimed.

Manager private differential compares 3,896 cases against original instruction
bytes: complete memory, all 32 GPRs, child-entry registers and every bounded
prefix. It dynamically visits 122 of 129 instructions. The remaining seven
(`0x80083F70/74` and `0x80084074..0x80084084`) are statically retained but
unreachable under stable mapped memory; they are not counted as executed.

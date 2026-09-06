# GAMEONLY speech startup recovery

`nba97_game_speech_startup` owns GAMEONLY `0x800800F8..0x80080247`
(inclusive), 336 bytes and 84 instructions. The private source boundary is the
fresh Ghidra listing `game_800800f8.txt`, routine SHA-256
`de945f95906f20d9632ddff0b12472b08b7296fc34c510edcb7c169727839628`.
Its three observed call sites are `0x800802B4`, `0x8002D2A8`, and
`0x8002E1A8`; the first is the recovered `0x800802AC` scene random warm-up.

The owner saves `ra` and `s0`, clears the two speech globals, allocates a
speech handle, selects `z0xspchf.bin`, `z0xspchg.bin`, or `z0xspch.bin` from
the live language word, and starts playback. It preserves the live handle
reload at `0x800801A8`, including callback changes, and writes the fifth
playback argument to live guest `sp+0x10` in the `0x800801BC` JAL delay slot.
The raw voice result is published before the next child call.

After one service pump, the routine forms a wrapping clock deadline with
`v0+240`. Readiness is checked before a strict signed `deadline < clock`
comparison. Equality pumps and retries. Both exit branches clear `a0` in their
delay slots. All eleven child boundaries receive all 32 live GPRs and may
replace `s0`, `sp`, saved stack bytes, and any other register. The final
`ra`/`s0` loads therefore use the cleanup child's live `sp` before advancing
that same value by `0x20`.

`game_speech_startup_adapter` composes the natural full-GPR warm-up event at
`0x800802B4` with this owner. The same adapter routes the warm-up's proven
`0x80093694` event through the existing random-seed owner. The random and
step services remain explicit synthetic callbacks; it does not translate the
adjacent `0x800935C4` routine.

The focused unit test uses runtime-generated retained memory and covers all
three language routes, exact calls and delay slots, the guest fifth argument,
callback-updated handles, immediate/later readiness, signed equality and
overflow/negative deadlines, child-mutated `s0`/`sp`/saved words, partial
knownness and unknown branch delays, callback refusal, bounded runaway,
mapping/alignment/overlap errors, and every operation-budget prefix. The
integration test proves the natural warm-up's first event, all-GPR handoff,
existing seed composition, and nested failure prefixes without retail assets.

Visual classification: **no direct visual effect**. This CPU owner changes
speech service state and retained memory; it does not render pixels or prove
audible playback. The native input-driven scene diagnostic now invokes this
owner through the production warm-up adapter. Synthetic clock samples 1000,
1240 and 1241 prove that equality repeats the pump and the next sample exits.
The receipt verifies 26 operations, four reads, seven stores, 15 calls, the
mapped fifth argument, and the handle/voice publications. Its before/after
diagnostic scanouts are identical. The separate frontend capture shows User
Setup; neither the diagnostic color pattern nor the menu establishes gameplay.
All native logs and frames remain ignored local evidence.

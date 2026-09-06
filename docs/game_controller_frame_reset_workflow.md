# GAMEONLY controller frame reset recovery

`src/recovered/game_controller_frame_reset.c` owns the complete GAMEONLY
routine at `0x800675E4..0x80067663` (128 bytes, 32 instructions). The boundary
comes from the fresh Ghidra listing `game_800675e4.txt`, whose recorded routine
SHA-256 is
`d4efbc7854c91bafdea88b2df235ea00f5140bdb039c9369b464fec2e363262e`.
Observed callers are `0x80068CF4`, `0x8007A040`, and `0x8007C1D8`; the first is
the natural call in the recovered match-tick owner.

The owner creates a `0x20`-byte stack frame and saves `ra`, then loads the timer
at `0x800FE90E` with signed `LH`. Zero skips the delta access entirely. A
nonzero timer subtracts the unsigned `LHU` delta at `0x800FDB6C` with wrapping
`SUBU`, stores the low halfword, shifts that low half into the sign position,
and stores zero only when the wrapped halfword is negative. This preserves the
source distinction between a signed timer load, an unsigned delta load, and a
clamp based on the already-stored low 16 bits.

The following eight iterations reload each live word at
`0x800FDC50..0x800FDC6F`, increment `a0`, and then clear the halfword at the
unchecked pointer plus `0x28`. The pointer-table cursor advances in the `BNE`
delay slot, including the final fall-through, so aliases between a target and a
future table word change the later pointer load exactly as they do on the PS1.
Null pointers and wrapped pointer additions are retained guest addresses rather
than host pointers. The sole child, `0x80083EEC` at `0x8006764C`, remains a
typed full-GPR callback. After it returns, `ra` is loaded through its mutable
`sp`, `sp` advances by `0x20`, and `JR` consumes the restored value.

`src/game_controller_frame_reset_adapter.cpp` accepts only the natural match
tick event `0x80068CF4 -> 0x800675E4`. The legacy tick event contains no full
GPR or stack state, so the binding requires an independent complete entry
fixture marked source-proven. It does not derive or invent `sp`, `ra`, `AT`, or
any other register from the tick's empty argument record. Earlier tick services,
including the adjacent `0x80067550` boundary, stay explicit fallback services;
the asset-free integration test keeps these services explicit. The native CPU capture instead composes the actual preceding limit leaf output, applies the adjacent JAL return address and NOP, and supplies the same explicit root S6 value to the legacy tick. It clears eight generated controller fields, clamps timer 1 minus delta 2 to zero, and reaches the unresolved 0x80068D58 -> 0x80067A60 clock call. This does not establish the actual tick prologue or advancing simulation.

The focused runtime-generated test covers timer values `0`, `1`, `0x7FFF`,
`0x8000`, and `0xFFFF` against deltas `0`, `1`, `2`, and `0xFFFF`; both wrapped
positive and negative results; the zero-timer no-read path; exact access and
delay ordering; all 32 GPRs; per-byte unknown timer, pointer, stored value, and
return-address state; callback mutation and refusal; all operation-budget
prefixes; mapping, alignment, overlap, alias, null-pointer, and address-wrap
cases; and mutable saved `ra`/`sp`. The integration test drives the actual
recovered match tick through call PC `0x80068CF4` using the production adapter,
an explicit independent full-GPR fixture, and typed fixtures for earlier
services, then stops at the next tick access. No retail asset or binary fixture
is used.

Visual classification: `Gameplay shown: NO - no direct visual effect`. The
routine changes retained CPU timer and controller-object fields and invokes a
typed service; it does not render or advance visible court/player state by
itself.

Manager validation: 3,276 private original-instruction differential cases cover all 32 source PCs. The native scripted driver records 23 operations (11 reads, 11 stores, one typed stream call) in three period cases. Its diagnostic before/after scanouts are pixel-identical synthetic color patterns; the separate frontend frame shows User Setup. No gameplay or audible playback is claimed.

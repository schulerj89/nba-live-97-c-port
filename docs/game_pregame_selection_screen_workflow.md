# GAMEONLY pregame selection screen recovery

`nba97_game_pregame_selection_screen` owns the complete GAMEONLY routine at
`0x80046C2C..0x80046F67` (828 bytes, 207 instructions). The source is the fresh
Ghidra listing `game_80046c2c.txt`, SHA-256
`4f301a8398a9415f58686b7bd861a1c0d85207f3784538f2025047a7cc820bb1`.
An ownership audit found only the function inventory and the unresolved call in
the recovered period-presentation finish owner, with no earlier complete owner.

The routine saves `ra` and `s8..s0`, initializes coupled selection indices zero
and twelve, and asks typed services to populate its two stack halfwords. Each
redraw computes the original coupling delta, draws the selection, snapshots
demo state and time, and enters the input loop. Direction masks move either the
coupled pair or one index within the source bounds. Mask `0x20` temporarily
installs stack local `sp+0x12` in the controller halfword while invoking the
menu service, then restores callback-live `s0` through callback-live `s6`.

With no input before the first latch, only positive signed wrapping timer deltas
accumulate. At 360 the routine advances and redraws while `s1 < 4`, then exits.
Any nonzero input latches `s7`; in demo mode even unrelated input bits instead
store the skip flag and value 99 before exit. Masks `0x180` request the regular
exit after a frame service. All 25 JAL sites expose the exact call PC, delay PC,
entry, operation, per-kind invocation and argument count with the full mutable
GPR/HI/LO machine.

The narrow adapter composes the owner at recovered period-presentation finish's
actual `0x8002DE14` call. It claims any event or machine naming the assigned
kind, entry, call PC, delay PC, or return address before validating invocation
one, zero arguments, the complete machine and mapped memory. The parent's
unresolved `0x80044550` child remains on a typed fallback. Once invoked, the
adapter always returns the owner's full machine prefix to the same-machine
parent, including an accepted child's malformed HI/LO, GPR, or zero register;
the parent can therefore preserve and diagnose that exact failing prefix.

Asset-free focused tests cover the normal source call order, every input mask
and bound, coupled redraw state, timer threshold/exit/zero/negative/wrap cases,
first-input latching, demo skip, controller aliases, operation cutoffs, callback
refusals, callback machine validation, unknown and malformed loads, partial and
misaligned return addresses, and semantic deterministic comparisons without
struct-padding `memcmp`. Natural tests run the actual recovered
`0x8002DDCC..0x8002DE33` parent through its nonzero gate, real input exit,
timeout, demo, failure, reuse, typed fallback, and malformed boundary cases on
the same retained RAM.

This routine controls a pregame UI/menu, but its drawing, input, timing and audio
children remain typed services until manager-owned shared integration connects
them to rendering. Gameplay shown: **BLOCKED**. The exact missing boundary is
the production composition of the typed draw/input/frame services
`0x8003081C`, `0x80035678`, `0x80046738`, `0x80049018`, `0x80083EEC`, and
`0x80036478` with the native renderer and input path.

Manager validation: strict Clang C99/C++17 and VS2022 passed 223 focused and 29 natural BZ checks. The full asset-free CTest suite passed 355/355. Independent original comparison passed 10,656 cases across all 207 instructions, including independent input, timer and selection cases and a late demo transition caused by a callback. It compared all 34 machine words and masks, 2 MiB RAM, callback entry machines and 16 stack words, mutable SP, and exact budget prefixes. FP register 30 and the 0x80046E7C delay-slot ANDI have regression tests. The adapter retains invalid child mutation prefixes.

Native run game-entry-20260906-071939-adb675df executes the actual presentation caller after the actual pregame card on the same synthetic retained RAM. Existing owners provide clock and stream pump/status behavior. Explicit input 4, 32 and 128 produces two selection requests, (0,12) then (1,13), one temporary controller/menu binding, and three polls before exit. The original controller halfword 4660 is restored. Drawing, menu and frame services remain typed dependencies.

Gameplay shown: BLOCKED at 0x80046738, 0x80036BE4 and 0x80049018. The visible native screen remains User Setup; ignored local PNG proof was inspected and is never committed. CPU before/after SHA-256 is 391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d. No advancing match is claimed.

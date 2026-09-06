# GAMEONLY match-state reset workflow

`nba97_game_match_state_reset` owns GAMEONLY `0x800659F0..0x80065B17`
(inclusive), 296 bytes and 74 instructions. The fresh
`game_800659f0.txt` listing has instruction SHA-256
`bae52046d74f276be2f00c026cb35b54ac140f1d8b3a07a44bcc987086107522`.
Repository review found only selected reset effects in `prepareMatchRuntime`;
that API remains a partial native preparation step and is not a prior owner of
this complete source boundary.

The C99 owner accepts all 32 GPRs, HI, and LO with byte-level knownness. It
creates the original 0x20-byte frame, issues four typed zero-fill calls, resets
the controller-profile state, publishes two fixed halfwords, rebuilds roster
bindings, executes the 24-evaluation decrement spin, initializes the two team
structures, runs the remaining reset services, and chooses the final child
from the retained mode at `0x8001EDEC`. The mode-98 path calls `0x80076AD0`;
every other proved mode calls `0x8006432C`.

All 14 source calls expose their exact call PC, delay-slot PC, entry, argument
count, operation number, invocation number, and complete machine. In
particular, the first zero call assigns `ra` before saving live `s1` in its JAL
delay slot. The first `0x800655B0` call stores five through live `v1`, the first
`0x80065820` call stores one through the newly assigned `s0`, and
`0x800646A8` stores zero through callback-live `s0`. The source spin is finite,
is not charged to the host safety budget, runs 24 branch evaluations, and
leaves `v0=0xFFFFFFFE` before later source instructions overwrite it.

Mapped accesses and child calls consume the explicit operation budget. Every
load and store validates alignment, mapping, and all participating knownness
bytes before changing its destination. Completed prefixes, access order, raw
machine words, and masks remain available after limits, traps, unknown
addresses or predicates, callback refusal, and malformed callback metadata.
The epilogue reloads `ra`, `s1`, and `s0` through callback-live `sp`, advances
that live stack by 0x20, and refuses an unknown `ra` only after the full
epilogue.

The native adapter claims the initializer's exact `0x8002DBF8` event with NOP
delay at `0x8002DBFC`, entry `0x800659F0`, return address `0x8002DC00`, assigned
call kind, and zero arguments. The parent exposes no HI/LO, so the binding
accepts an explicit provider; a missing provider supplies two explicit unknown
words. Valid child GPR prefixes copy back even when returned HI or LO metadata
is malformed. Malformed GPR or zero-register metadata stays in the nested
diagnostic without replacing the valid parent register set.

The adapter composes `0x80063D58` through the recovered roster owner while
transporting HI/LO unchanged. Its production zero bridge composes all four
`0x800A3A74` calls through the recovered zero owner. Fresh
`game_800a3a74.txt` and `game_800a3a78.txt` evidence proves the full scratch
mapping: `at`, `a2`, and `t2` are zero before the first SWR; after that store,
`t1` is the original destination modulo four and `t0` is four minus `t1`;
`a0`/`a1` use the narrow owner's exact working outputs; `v0` and every other
GPR, HI, and LO are untouched. Separate budgets retain failures at each of the
four zero calls. All other reset children remain typed full-machine callbacks.

Asset-free focused fixtures cover both mode exits, all calls and delay slots,
the fixed spin, all 16 byte masks, callback-live S0/S1/SP/HI/LO changes, wrapped
arguments, stack/global aliases, every owner budget from 0 through 26,
unknown stores and mode, malformed loads and callbacks, unmapped and unaligned
accesses, journal truncation, and epilogue control flow. Natural initializer
fixtures execute the real parent, all four recovered zero calls, and the
recovered roster child on one retained memory image. They cover all destination
alignments, zero budget prefixes and tail failures, nested roster failure,
partial V0 transport, exact adapter guards, malformed HI/LO and GPR prefixes,
and repeated adapter reuse.

An independent local original-instruction differential compared 5,376 cases
across every one of the 74 source PCs, all 34 machine words and masks, the full
2 MiB memory image, callback entry machines, both mode branches, budgets 0
through 26, and callback-live S0/S1/SP/HI/LO mutation.

The production zero bridge independently passed 5,120 comparisons against the
original 81-instruction zero entry across all 73 reachable large-path PCs, all
four source counts and destination alignments, all 16 V0 masks, partial
T0/T1/HI/LO masks, the full machine and 2 MiB memory image, and first, middle,
and tail budget prefixes.

Gameplay shown: NO - no direct visual effect. This boundary rebuilds CPU
match state; visible gameplay requires the later native frame path.

The native scripted initializer now replaces its former synthetic 659F0 response
with this production reset adapter on the same retained memory. It composes
all four zero-fill calls and the roster owner, and records the source halfwords,
stack return, nine remaining typed child services and both frame hashes. The
initial parent zero-fill proof is captured immediately after that clear, before
subsequent reset stores alter the range. The frontend remains User Setup.

Manager verification: 257 focused checks, 1,386 natural-caller checks, and all
323 asset-free CTests passed. The native scripted run is recorded in ignored
.local/verification/team_select/game-entry-20260906-044055-640f3f59.
Its before/after CPU scanout hashes both equal
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d.
The separate frontend screenshot shows User Setup only.

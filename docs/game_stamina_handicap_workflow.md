# GAMEONLY stamina and score handicap

`nba97_game_stamina_handicap` owns GAMEONLY `0x80068504..0x800686B7`
(436 bytes, 109 instructions). The source is the fresh Ghidra listing
`game_80068504.txt`, with instruction SHA-256
`f0736b214b6a4d3bd08c1ff01790894a88b2cb710f5a81ff15d7cef5ac94fea2`.
The boundary was previously represented only by match tick's unresolved call at
`0x80068E60`; no complete standalone owner was found.

The routine first writes `0xFFFF` to `0x800FDB98` in the feature-test branch
delay. When enabled and the signed clock is below 7201, it compares two
unsigned score halfwords through wrapped `SUBU` and changes the handicap to
zero for differences below -2 or five for differences above 2. The two source
`SLTI` instructions and the `ORI v0,5` delay effect remain observable on
unknown or skipped paths.

Phase zero enables two fixed loops. The first visits 24 signed halfwords at
`0x8001F80C` with stride `0x22`. Only sign-extended values 0..0x7FFE pass the
source unsigned comparison. Each eligible value adds the freshly read signed
halfword at `0x800FDB7E`, caps signed results above 0x7FFF, and stores the low
half. Negative additions may underflow; the owner preserves that behavior.

The second loop follows ten live actor pointers and their linked records.
Negative record stamina and inactive actors skip the reduction, while source
feature and actor flags select either a signed actor value or its wrapped
triple. The reduction rereads `0x800FDB7E` as an unsigned halfword, stores the
wrapped stamina half before the sign test, clamps a negative low half to zero,
and always clears actor `+0xDD`, including inactive paths. Pointer, flag, delta,
and storage aliases remain live because every source access is retained in
order.

The owner carries all 32 GPRs and HI/LO with one knownness bit per byte. Guest
addresses remain 32-bit mapped values. Alignment, resource bounds, overlapping
guest regions, malformed knownness, unknown writes, and operation-budget
prefixes are explicit. The routine is a leaf and leaves SP, RA, HI, and LO
unchanged; live RA is validated only when the final JR executes.

`nba97_game_stamina_handicap_from_match_tick` binds the actual zero-argument
`0x80068E60 -> 0x80068504` event and requires JAL-produced `ra=0x80068E68`.
The legacy match-tick service ABI has no CPU state, so an independently proven
full entry machine is mandatory. The natural test reaches the call through the
actual recovered match-tick and actor-timers owners on shared synthetic RAM.
It uses an exact prerequisite whitelist, proves match tick's countdown store of
59 in the call delay, then refuses the next `0x80068E78` service.

Focused synthetic coverage exercises every handicap gate, clock and score
boundary, phase exit, all score eligibility classes, signed and unsigned delta
extrema, actor feature/flag paths, triple and stamina wrap, store-before-clamp
ordering, partial predicates and delay effects, malformed loads, pointer
failures, every mapped-access budget, all RA masks, JR alignment, untouched
machine state, and deterministic repeatability. The manager's independent
original-instruction differential passed 16,864 cases across all 109 PCs, all
34 machine words and masks, full 2 MiB RAM, score/actor classes
and operation prefixes. Its receipt is
`.local/evidence/tipoff-recovery/stamina_handicap_differential.json`.

Visual classification: **Gameplay shown: NO - no direct visual effect**. This
CPU-only routine changes retained handicap, score, stamina, and flag state. The
synthetic natural caller fixture is state evidence and does not claim rendered
advancing gameplay.

The manager's native capture composes actual crossing-rule, actor-timer, and
stamina owners on the same generated RAM. The explicit caller fixture retains
its source `s3=0x800FDB88`; the 0x80068E40..0x80068E64 bridge projects only the
proven `v0`, `v1`, and JAL return address from the actual actor-timer return.
The countdown's call-delay store is 59. The receipt observes handicap 5,
24 score values `10 -> 11`, ten stamina records `20 -> 16`, and the actor flag
`1 -> 0` after recording the earlier actor-timer checkpoint. Typed prerequisites
and the next recorder boundary still prevent an advancing match.

Focused validation: 341 checks; natural integration: 31 checks. Strict Clang and
MSVC pass. The full 375-test asset-free CTest suite and all progress, recovery,
instruction-semantics and roster freshness checks pass. The native ignored
`stamina_handicap_verified.json` ties these state changes to scripted native
input and identical before/after CPU-frame SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The displayed UI remains User Setup.

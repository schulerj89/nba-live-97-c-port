# GAMEONLY crossing-half rule gate recovery

The recovered owner covers exactly `0x8006817C..0x8006830B`, 400 bytes and
100 instructions. Its source is the fresh Ghidra listing
`game_8006817c.txt`, whose instruction SHA-256 is
`608e555ead6150b53d94c8dcafe77e30cde1ca3d94242c7f39270875e745b19a`.
The functions inventory was checked before implementation; it named the
boundary but contained no complete owner.

The routine first reads the signed phase before allocating its 24-byte frame.
The phase branch always saves `ra` in its delay slot. Actor-block state clears
the crossing blocker, while a negative owner takes an intentionally different
uncleared exit. The two actor pointers are latched in source order and their
fullword fields are XORed. A nonnegative XOR arms the state through ordered
halfword stores of `1`, `0`, and `0x7FFF`. A negative XOR follows the enable
and timer path.

The timer uses unsigned halfword loads and 32-bit `ADDU`, stores the wrapped
low half at `0x800FDBAC`, then sign-extends that low half before comparing it
with 13. This preserves negative and wraparound cases. Once eligible, the
routine checks the live second actor, consumes the complete `0x80062D84`
return, chooses announcement 11/5000 or 12/20000, dispatches rule code 5, and
stores rule code 8. Every clearing path finally stores zero at `0x800FE8E0`.

Five typed child kinds describe six static call sites. The native adapter
composes only the exact `0x800682D0 -> 0x800295C8` event through the existing
complete no-op owner. The semantic or narrow helpers associated with
`0x80062D84` and `0x80062300` do not expose this source's full mutable-machine
contract in this checkout, so those services remain typed callbacks together
with `0x80029590` and `0x80062660`. No child algorithm is copied.

Match tick reaches the routine at `0x80068E30`, but its legacy service API
carries only the event and an optional scalar result. The adapter therefore
requires an independently proven full 32-GPR, HI/LO, and stack entry machine
with `ra=0x80068E38`; missing context is refused. The natural test labels every
synthetic prerequisite response explicitly, checks the exact service sequence
through `0x80068E30`, and rejects the next service so the fixture does not
claim an advancing match.

Focused asset-free coverage exercises normal home and away announcement paths,
all clear and uncleared exits, ordered arm stores, timer boundaries and wraps,
actor aliases, all six static call sites, callback refusal, operation cutoffs,
partial and malformed knownness, atomic loads, unknown stores, mutable
`a0`/`sp`/HI/LO, mapped address wrap, alignment/resource failures, and all 16
saved-`ra` masks. The tests compare semantic word and mask fields directly and
remain active under `NDEBUG`.

The independent original-instruction differential receipt is
`.local/evidence/tipoff-recovery/cross_half_rule_differential.json`. It passed
12,160 cases across all 100 source PCs, full 2 MiB retained RAM, all 32 GPRs
plus HI/LO and their byte masks, callback entry machines and six stack words,
mutable `sp`, callback-mutated team state, timer/sign/flag variants, and the
tested access/call budget prefixes. The committed focused binary passes 172
always-active checks; the actual-match-tick integration binary passes 43.

Gameplay shown: **NO - no direct visual effect**. This CPU routine changes
rule state and dispatches typed audio/rule services. The native self-driving
capture uses the actual match-tick caller with explicit synthetic prerequisites
and a full-machine snapshot over the same retained memory. Its timer changes
12 to 13, blocker changes 1 to 0, and rule code changes 0 to 8. The existing
duration no-op runs once; the parent then refuses the unresolved actor-timer
boundary at `0x80068E38 -> 0x8006830C` before any match frame. The native screen
remains User Setup. The CPU diagnostic frame hashes both equal SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The full asset-free CTest suite passes 361 tests. Native logs, captures, and
private source comparisons remain ignored local evidence.

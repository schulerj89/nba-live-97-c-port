# Period audio no-op recovery

`nba97_game_period_audio_noop` owns GAMEONLY
`0x8002A254..0x8002A25B` (8 bytes, 2 instructions). The fresh Ghidra listing
is `game_8002a254.txt`; its instruction bytes have SHA-256
`6d64edf91449c1b17746c1ef18afa2eb25c70bdf1322ab3df5a2630993b7e2f1`.
Known callers are `0x80067434` and `0x800678F0`; the routine has no children.

The complete body is `JR ra` followed by a NOP delay. It performs no audio
operation, memory access, stack access, call, argument interpretation, or
register assignment. Every incoming GPR and HI/LO word and per-byte known mask
passes through exactly. In particular, it does not invent a `v0` result and it
ignores every `a0` bit pattern. An unknown or fully-known unaligned return
target is reported only after the NOP delay.

The owner therefore needs only a full-machine context. Its progress reports
zero operations, reads, stores, and accesses. No guest-memory mapping or host
pointer is involved.

The natural adapter composes the actual recovered first-period owner at JAL
`0x80067434`, its delay assignment `a0=1` at `0x80067438`, and return
`ra=0x8006743C`. It claims the assigned kind, entry, call PC, delay PC, or
return address before requiring the complete exact boundary, so malformed
assigned events cannot reach fallback. All other first-period children remain
typed fallback services. Because the parent carries only GPRs, the adapter
marks unavailable HI/LO explicitly unknown; the no-op preserves them without
using them and copies the unchanged GPR file back.

Asset-free focused tests cover arbitrary full 34-word state with partial masks,
extreme ignored `a0`, `v0`, and `sp` values, all 16 return-address known masks,
all three misalignments, aligned zero and nonzero returns, invalid zero/GPR/
HI/LO metadata, null arguments, zero observable operations, and deterministic
fieldwise preservation. Natural tests execute the actual `0x800673F0` owner on
both presentation branches with all remaining children typed, verify delay
`a0=1`, unchanged forwarded `v0`, exact source call order, zero memory changes
during the hook, repeated binding use, and every exact and claim-only guard.

Gameplay shown: NO - no direct visual effect. This evidenced hook has no state,
audio, UI, or renderer effect, so the manager-owned native capture should show
identical frames while the call trace proves execution.

Manager review independently checked the original JR/NOP bytes and compared
8,192 arbitrary full-machine cases, every return-address known mask and
alignment, and zero memory operations. The ignored differential receipt is
`period_audio_noop_differential.json`.

Integration passed 632 focused checks, 474 natural-caller checks, all 351
asset-free Debug CTests (7.20 seconds), and progress, recovery, instruction-
semantics, and roster freshness checks. Native input run
`game-entry-20260906-065307-c96ebe83` composes the actual first-period hook in
both presentation paths without adding fixture inputs. It verifies complete
retained-memory and GPR identity, zero operations/accesses, delay A0=1,
RA 0x8006743C, SP 0x801FFED0, and preservation of respective V0 values zero
and 0x80046C2C. Parent-absent HI/LO remain explicitly unknown.
CPU-only before/after frames share SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The displayed frontend remains User Setup; no advancing match is claimed.

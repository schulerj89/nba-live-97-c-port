# GAMEONLY actor resume/reset recovery

`nba97_game_actor_resume` owns GAMEONLY `0x800582DC..0x800583FB`, 288 bytes
and 72 instructions. Fresh Ghidra evidence records instruction SHA-256
`a9ecb213007e80cf4fa52701ed0b7d61a6c462630092492659f1d5eb90612c8d`.

The owner reads signed phase `0x800FDB90` before allocating its `0x18` frame.
Phase 82 compares the actor's unsigned `+0xD9` byte with signed halfword
`0x800FE880`; every other phase uses signed `0x800FDB94`. Equality publishes
actor state byte 1 and inequality publishes 2, including the original branch
and jump delay-slot writes.

It clears actor `+0x4E`. An unsigned `+0x46` or `+0x4A` value below 37 forces
`a1=1`; only two values at least 37 retain the caller's live `a1`. It then calls
typed children `0x80056FFC` and `0x8005703C`. The latter receives `a0` copied
from callback-mutated live `s0` in its delay slot. Low two bits at `+0x60` and
`+0x64` gate a live nested `+0x20` pointer read. Nested byte `+0x0D` selects
actor `+0x9A` as 0 or 3; a nonzero low flag preserves `+0x9A`. Finally it writes
47 to `+0xB8`, copies unsigned halfword `+0xA2` to `+0xA6` in the third child's
JAL delay slot, calls typed `0x800582CC`, and restores `ra/s0` through live `sp`.

The 23 callers are `0x80062560`, `0x800609B4`, `0x800609E0`, `0x80060AB4`,
`0x800676CC`, `0x800598B0`, `0x800585F4`, `0x8006CEF4`, `0x8006BDC8`,
`0x8006BE90`, `0x8006B818`, `0x8006B198`, `0x8005C768`, `0x8005A1B4`,
`0x8005A270`, `0x8005B23C`, `0x8005C504`, `0x80059A3C`, `0x8005D098`,
`0x8005D0E8`, `0x8005D054`, `0x8005DB8C`, and `0x8005B008`.

`game_actor_resume_adapter` composes the actual full-machine period-expiry call
at `0x800676CC`, including live actor `a0`, delay-slot `a1=1`, JAL `ra`, all 32
GPRs, HI/LO, and shared retained memory. The older tipoff-continuation API at
`0x800609B4/0x800609E0` carries values through a narrow callback with no GPR,
SP, or HI/LO channel, so it cannot safely compose this owner or invent a return
machine. Its typed boundary remains explicit.

Focused asset-free tests cover signed phase/team selection, actor bytes 0/255,
animation thresholds 36/37/65535, retained or forced `a1`, every low-two-bit
flag route, nested byte zero/nonzero, exact access and call order, callback
mutation/refusal, all operation-budget prefixes, partial knownness and stores,
stack/actor aliasing, alignment, mapping, overlap, and unknown branch/JR state.
The natural integration runs recovered period expiry through its real
`0x800676CC` event while all three AF children remain explicit full-machine
fixtures.

Gameplay shown: **NO - no direct visual effect**. This routine prepares actor
animation and state in retained CPU memory. A rendered change still requires
the separate rendering bridge; the synthetic actor fixture is not gameplay.

Manager verification: 126 focused checks, 10 actual period-expiry integration
checks, strict C99, and all 255 asset-free CTests passed. Private differential
validation compared 2,408 cases against original instructions, covering all 72
instruction PCs, complete retained memory, all GPRs and HI/LO, callback entry
state, and bounded prefixes.

Native run `game-entry-20260905-220509-e435f3ae` exercised the actual parent
call at `0x800676CC` in an independent zero-clock actor fixture: state 27 became
1, `+0x4E` became 0, `+0x9A` became 3, `+0xB8` became 47, and `+0xA6` became
`0x1234`. It completed 22 operations, 12 reads, seven stores and three explicit
animation-service callbacks. The parent then reset owner/phase and selected
the ball. The ignored `actor_resume_verified.json` records exact state, stack,
return addresses, driver input frames and CPU before/after hashes. Both hashes
are `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The separately captured frontend remains Boston/Chicago User Setup. These
CPU scanouts are diagnostic patterns; no advancing match or tip-off is shown.

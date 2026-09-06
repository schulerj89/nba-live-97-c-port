# GAMEONLY frame interrupt-restore recovery

`nba97_game_frame_interrupt_restore` owns GAMEONLY
`0x8004900C..0x80049017`, exactly 12 bytes and three instructions. The fresh
Ghidra listing `game_8004900c.txt` has instruction-byte SHA-256
`2a0aba4dfdd11aabbe78d6d357c5248773d17b360739d0f3c8567bded0096ee3`.
Its callers are the existing frame owner at `0x8004909C`, `0x800491D8`,
`0x8004926C`, and `0x800492C0`. It has no callees and no guest RAM access.

The source performs `MTC0 a0,Status`, then `JR ra` with a NOP delay. CP0 Status
receives the complete `a0` bit pattern and per-byte knownness without masking,
merging, or sanitization. The previous CP0 Status is ignored even when unknown.
Every GPR, including `a0` and raw `v0`, plus SP and HI/LO, remains unchanged.
An unknown return address is consumed only after Status has been published, so
that write remains visible when execution stops at `0x80049010`.

CP0 Status is explicit emulated machine state. It is neither a guest pointer
nor a request to control host operating-system interrupts. MTC0 is the only
budgeted operation and produces an ordered journal entry at `0x8004900C`.
Budget zero stops before the write; budget one reaches the write and return.

`game_frame_interrupt_restore_adapter` binds all four exact restore call PCs
from the existing `0x80049018` frame owner. The binding carries only persistent
CP0 Status. At each invocation it independently constructs known zero, fully
known `a0=args[0]`, and source-proven `ra=call_pc+8`; all other GPRs and HI/LO
are unknown because the narrow caller does not expose them. The parent has
already rejected an unknown saved Status before placing it in `args[0]`.
Telemetry is indexed by call PC and counts repeated calls separately. A nominal
frame reaches restore thirteen times: once at `0x8004909C`, ten actor-loop
calls at `0x800491D8`, and once each at `0x8004926C` and `0x800492C0`.

Natural integration composes the existing AS disable owner as the forwarded
typed service at all four corresponding disable PCs, transports one explicit
CP0 Status through disable and restore, and leaves every other frame service as
a typed synthetic fixture. It does not duplicate AS or substitute a second
restore algorithm. Focused tests cover all 16 `a0` knownness masks, fixed 0,
1, `0xFFFFFFFF`, and `0x80000001`, 4,096 deterministic random values, full
machine preservation including raw `v0`, ignored unknown prior Status, unknown
RA after the write, exact budget 0/1 prefixes, journal ordering, malformed
metadata, and repeatability.

The production `GameMatchFrame` rendering backend now dispatches both recovered
interrupt owners directly. Its persistent CP0 Status starts unknown and must be
supplied from an established entry state. Failed later services retain a prior
disable's effect, and an unknown saved Status stops the frame after that effect.
The backend test verifies the actual pair before camera/render services.

Gameplay shown: NO - no direct visual effect. This routine changes only
explicit emulated CP0 control state. It does not render pixels, advance the
match, or control native OS interrupts. Shared capture and build registration
remain manager-owned.

Manager validation passed 149,846 focused checks, 154 natural-frame checks,
45 rendering-backend checks, strict C99 compilation, and all 281 asset-free
CTests. Independent execution of the three original instructions matched
8,192 cases across all 35 machine words, byte masks, and operation prefixes.
The native receipt records 13 actual disable/restore pairs across four sites,
with Status `0xABCDEF00` restored to `0xABCDEF01`. Before/after CPU frame
SHA-256 is identical:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The frontend remains User Setup; typed rendering fixtures are not gameplay.

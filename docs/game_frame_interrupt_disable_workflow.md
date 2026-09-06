# GAMEONLY frame interrupt-disable recovery

`nba97_game_frame_interrupt_disable` owns GAMEONLY
`0x80048FF4..0x8004900B`, exactly 24 bytes and six instructions. The fresh
Ghidra listing `game_80048ff4.txt` has instruction-byte SHA-256
`e2b5189bb64b723db34b4a799b587de972cad235d22f3004a0b1d50abea35fb8`.
Its observed callers are the existing frame owner at `0x80049070`,
`0x800491C8`, `0x8004920C`, and `0x8004927C`. It has no callees and performs
no guest RAM access.

The six source instructions read CP0 Status into `v0`, load `v1=-2`, compute
`v1=v1&v0`, write `v1` back to CP0 Status, then execute `JR ra` with a NOP
delay. Thus `v0` retains the old status and `v1` plus CP0 Status contain the
old value with only interrupt-enable bit 0 cleared. Every other GPR, SP, and
HI/LO stays unchanged. An unknown return address is consumed after MTC0, so
the explicit CP0 write remains visible when execution stops at `0x80049004`.

CP0 Status is retained native machine state with one knownness bit per source
byte. It is not a host pointer or a request to mask host OS interrupts. MFC0
and MTC0 are the two budgeted operations and appear in an ordered journal at
`0x80048FF4` and `0x80049000`. Partial Status knowledge propagates to `v0`,
`v1`, and the written CP0 word. Clearing bit 0 does not make an otherwise
unknown low byte known.

`game_frame_interrupt_disable_adapter` binds all four exact call events from
the actual recovered `0x80049018` frame owner. The binding carries only
persistent CP0 Status. At every call site the adapter independently marks all
other GPRs and HI/LO unknown, then supplies the hard-wired zero register and
source-proven `ra=call_pc+8`. It never carries incidental registers across
intervening frame instructions that the narrow callback cannot expose. A
partially known old Status becomes the frame API's canonical unknown return
value after the disable owner has still published the cleared CP0 prefix.
Telemetry is indexed by the four source call PCs and separately counts repeated
invocations. In a nominal frame `0x800491C8` runs ten times in the actor loop,
so the four call sites produce thirteen disable invocations; redraws may add
more without imposing a source loop cap.

Every other frame memory operation and typed service is forwarded unchanged.
In particular, `0x8004900C` remains an unresolved typed restore fixture; this
recovery does not translate it. Focused synthetic tests cover status values 0,
1, 2, 3, `0xFFFFFFFF`, `0x80000001`, thousands of deterministic random words,
all 16 byte-knownness masks, full machine preservation, ordered CP0 journals,
budgets before MFC0/MTC0, unknown RA after the write, malformed contexts, and
repeatability. Natural integration drives all four real frame call PCs,
persistent CP0 transport, known and unknown old-status returns, forwarded
services, invalid metadata, and refusal prefixes without assets.

Gameplay shown: NO - no direct visual effect. This routine changes only
explicit emulated CPU control state. It does not render pixels, advance the
match, or control native operating-system interrupts. Shared capture and build
registration remain manager-owned.

Manager validation passed 155,118 focused checks, 160 natural-frame checks,
strict C99 compilation, and all 279 asset-free CTests. Independent execution
of the fresh six original instructions matched 12,288 cases over all 35 machine
words, byte masks, operation budgets, and unknown-return prefixes.

The self-driving native receipt records 13 disables across the four sites
(`1/10/1/1`), Status `0xABCDEF01` becoming `0xABCDEF00`, and each typed restore
receiving the original word. Matching before/after CPU frame SHA-256 is
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The frontend still displays User Setup. The synthetic frame services do not
establish advancing gameplay.

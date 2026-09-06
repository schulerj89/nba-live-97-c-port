# GAMEONLY rule-delay no-op recovery

`src/recovered/game_rule_delay.c` owns the complete GAMEONLY subroutine
`0x800295C8..0x800295CF`: 8 bytes and two instructions. Fresh Ghidra evidence
in the ignored recovery workspace records instruction SHA-256
`6d64edf91449c1b17746c1ef18afa2eb25c70bdf1322ab3df5a2630993b7e2f1`.

The source is exactly `JR live ra` followed by a `NOP` delay slot. It ignores
the duration in `a0`, does not inspect `sp`, and performs no call, retained
memory access, clock read, sleep, wait, or register change. A fully known `ra`
returns even when its value is zero, wrapped, or unaligned because the source
does not add an alignment trap. A partially or wholly unknown `ra` reports the
unknown `JR` after accounting for the state-neutral delay slot. Every raw GPR,
HI/LO word, and byte-knownness mask remains unchanged.

The twenty source-proven caller PCs are `0x800596C8`, `0x80060788`,
`0x80060B38`, `0x80060B6C`, `0x80060C7C`, `0x8005D498`, `0x8005D4A8`,
`0x8005D898`, `0x8005D8A8`, `0x80067DF4`, `0x80067EF0`, `0x80067FDC`,
`0x800682D0`, `0x8006D3EC`, `0x8006F2C8`, `0x8006D6B0`, `0x8006ED84`,
`0x8006EF00`, `0x8005CD60`, and `0x80059AEC`. The recovered physics and ball
owners expose narrower callback contracts, so this recovery does not invent a
full-machine bridge for them.

`game_rule_delay_adapter` composes the actual recovered clock-violations owner
`0x80067D38` at its three natural call sites: `0x80067DF4`, `0x80067EF0`, and
`0x80067FDC`. It validates the source event, live JAL return address, and the
exact caller-forwarded duration of 5000 or 20000 before invoking the leaf. All
other clock-violation children remain explicit typed fixtures. The natural
integration test drives one clock-violation invocation through all three sites
for source team values 0, 1, and 0xFFFF, proving both duration variants and the
same production adapter path.

The focused test covers all 16 `ra` knownness masks over zero, boundary,
wrapped, and unaligned targets; all 16 knownness masks across every other live
GPR and HI/LO; unknown `sp` and `a0`; complete raw-machine preservation; zero
operations and memory accesses; repeatability; and malformed machine metadata.
The integration test covers all three actual parent sites, team-controlled
5000/20000 forwarding, state inherited from preceding typed children, adapter
guards, and callback refusal.

Visual classification: **Gameplay shown: NO - no direct visual effect**. The
source is a CPU no-op and cannot change a frame. Native capture and identical
before/after frame hashes remain manager-owned shared integration evidence.

Manager acceptance passes 32,224 focused checks, 152 natural integration checks,
strict C99, all 253 asset-free CTests, and progress/metadata freshness checks.
A private original JR/NOP comparison passes 4,096 cases, including every RA
knownness mask and all 34 raw machine words/masks. Worker ASan checks also pass.

Native input run game-entry-20260905-214500-401eb969 now executes the actual
no-op from clock violations. The phase-0 fixture forwards 5000 once; the
phase-0x82 fixture forwards 20000 at both its reached sites; phase 0x81 makes
no call. The verifier asserts zero operations/reads/stores, every GPR/HI-LO
unchanged, live SP 0x801FFEE8 and exact JAL return addresses. The first-shot
site is additionally exercised by the focused natural-caller integration.
Before/after diagnostic frames match SHA-256
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d.
The separately captured frontend remains Boston/Chicago User Setup. All logs
and media are ignored. Gameplay shown: NO - no direct visual effect.

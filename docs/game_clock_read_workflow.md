# GAMEONLY clock-read recovery

`src/recovered/game_clock_read.c` owns the complete GAMEONLY subroutine
`0x800A5810..0x800A581F`: 16 bytes and four instructions. Fresh Ghidra evidence
in the ignored recovery workspace records instruction SHA-256
`e8dc11d1abddf2e952768e4de6abb193e2cbae4d9423b179825acb709b7d9c0d`.

The source performs exactly this sequence:

```text
0x800A5810  v0 = 0x800D0000
0x800A5814  v0 = LW [0x800D7A70]
0x800A5818  JR live ra
0x800A581C  NOP
```

The owner reads one little-endian retained word and returns its raw value and
per-byte knownness. It does not use `sp`, allocate a frame, store memory,
increment the counter, call a service, or alter any other GPR or HI/LO. A
partially or wholly unknown clock word still returns when `ra` is known. An
unknown `ra` stops at the `JR` after the read has completed. Budget exhaustion,
unmapped memory, and malformed knownness preserve the visible LUI value in
`v0`.

The 37 source-proven callers are `0x8002AB50`, `0x8004A10C`, `0x8004A5BC`,
`0x8002A270`, `0x8002A6BC`, `0x8002A984`, `0x80044874`, `0x800448AC`,
`0x80046CB8`, `0x80046D24`, `0x800444A4`, `0x800444C8`, `0x800801EC`,
`0x80080208`, `0x800803A4`, `0x800803C0`, `0x80080438`, `0x80080454`,
`0x8008F204`, `0x8008F254`, `0x800A585C`, `0x800A593C`, `0x800A5948`,
`0x800A7754`, `0x800B17F4`, `0x800B13B4`, `0x800B19DC`, `0x800B1CB4`,
`0x8002CC18`, `0x8002CCBC`, `0x8002A858`, `0x800A582C`, `0x800A58B4`,
`0x800A5908`, `0x800A7498`, `0x800A962C`, and `0x800A9648`.

`game_clock_read_adapter` composes the actual recovered speech-startup owner
`0x800800F8` at both natural call sites, `0x800801EC` and `0x80080208`. That
parent exposes all GPRs but no HI/LO channel, so the adapter gives the leaf
unknown internal HI/LO and copies only the unchanged GPR file back. The
integration fixture advances `0x800D7A70` explicitly in other typed speech
callbacks; the leaf itself never acts as a host timer.

The focused test covers the five counter extremes, all 16 byte-knownness masks,
all 32 GPRs and HI/LO, unknown `sp`, known and unknown `ra`, budgets zero and
one, exact read journaling, missing/truncated mappings, invalid and overlapping
regions, malformed known metadata, and prefix state. The integration test runs
the actual speech owner through one initial read and two deadline polls while
all remaining services stay explicit asset-free fixtures.

Visual classification: **Gameplay shown: NO - no direct visual effect**. This
CPU leaf only samples retained state. It has no renderer, audio backend, real
timer, or advancing-match effect, so visual capture belongs to manager-owned
shared integration rather than this seven-file recovery.

Manager acceptance: 3,059 focused checks, 26 natural-caller integration checks,
all 247 asset-free CTests, strict C99, progress and metadata freshness checks
passed. A private original-instruction differential covered 4,096 cases and all
four source PCs, including every counter-knownness mask and budget prefix.

Native input-driven run `game-entry-20260905-212012-94a92c59` executes all three
speech clock reads through this owner (1000, 1240, 1241). The counter progression
is an explicit retained-memory fixture outside the leaf. The enclosing native
diagnostic now expects its final value 1241 instead of the earlier untouched
zero. Each receipt proves one read, no store, unchanged SP and the actual JAL
return address. The before/after CPU diagnostic frame SHA-256 is identical:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The separately captured frontend remains Boston/Chicago User Setup. Captures
and logs stay ignored; no advancing match or audible playback is claimed.

# GAMEONLY late-period minimum limits recovery

`nba97_game_late_period_limits` owns the complete GAMEONLY range
`0x80067550..0x800675E3` (148 bytes, 37 instructions). The fresh Ghidra
listing is `game_80067550.txt`, with routine SHA-256
`85107c952ec4e23503c49127f7b40425964341a68a4d9453b3eae1adb6ef98f7`.
The sole observed caller is the recovered match tick at
`0x80068CEC -> 0x80067550`; the leaf has no callees.

The owner first reads the signed 32-bit game clock, then always clears the
halfword at `0x8010606C` in the clock branch's delay slot. Before clock
`0x1C20`, signed period values below three return with that zero. Period three
publishes five and later periods publish four. It reloads the published limit,
subtracts two with 32-bit wrap, and raises the unsigned home and away
halfwords only when their signed zero-extended values are below that result.
The home store precedes the away read, so native-storage aliases observe the
same order as the source.

The C99 owner keeps all 32 GPRs and one knownness bit per byte. It models the
two branch-delay comparisons, the unconditional zero store, signed LW/LH
against unsigned LHU, address validation, access failures, and every operation
budget prefix. Guest addresses remain `uint32_t`; retained mappings perform
all loads and stores, and an unknown `ra` is consumed only at the final JR
after the complete memory prefix.

`nba97_game_late_period_limits_from_match_tick` binds only the exact natural
tick event and forwards every other service. The old tick API contains no GPR,
SP, or retained-memory payload, so the adapter requires an independently
source-proven full entry context. It never derives or invents that state from
the tick's zero-argument service record. The integration test reaches the
actual `0x80068CEC` event and stops at the next unresolved
`0x80068CF4 -> 0x800675E4` boundary.

The focused tests are synthetic and asset-free. They cover signed clock and
period boundaries, home/away thresholds, exact GPR results, per-byte unknown
predicates, both branch-delay effects, native-storage aliases, mapping and
validation failures, unknown return state, exact access order, and every
operation-budget prefix.

Visual classification: **no direct visual effect**. This routine changes
retained CPU minimum-limit state and does not render a court, players, UI, or
an advancing gameplay frame.

The native input-driven diagnostic now dispatches this production tick adapter
after the period-startup fixture. Three cases share the period memory while
supplying an explicit independent full-GPR leaf context. They prove the
unconditional limit clear from 0xBEEF to zero, three operations, two reads and
one store, then reach the next 0x800675E4 service. Diagnostic before/after frames
remain identical; the separate frontend screenshot is User Setup. This does
not resolve the legacy tick's missing register/stack return API.

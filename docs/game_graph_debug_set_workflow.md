# GAMEONLY SetGraphDebug recovery

`src/recovered/game_graph_debug_set.c` owns the complete 27-instruction
GAMEONLY routine `0x800992C4..0x8009932F`, called from `0x80029A28` with level
`0` immediately after `ResetGraph(3)`. The retail format string at `0x80028250`
and Ghidra data flow identify it as PsyQ `SetGraphDebug`.

The routine saves `ra` and `s0`, reads the old debug byte at `0x800C55C2`,
stores the low byte of its argument there, and returns the old byte. A zero low
byte takes the 15-instruction startup path and performs no diagnostic call. A
nonzero low byte reads the live pointer at `0x800C55BC`, then invokes it with
`"SetGraphDebug:level:%d,type:%d reverse:%d\n"`, the stored level, the graph
type byte at `0x800C55C0`, and the reverse byte at `0x800C55C3`. The callback's
return value is discarded; saved stack words are reloaded live afterward.

Compatibility deliberately retains source-era hazards. The full argument is
truncated before both the store and branch, so values such as `0x100` silently
disable diagnostics while `0xFFFFFF01` selects level one. Nonzero levels call
the loaded function pointer without a null or executable-range check. The port
does not clamp these values or substitute a safe callback. Standalone tests
cover both aliases, an unguarded zero target, callback stack mutation, unknown
mapped bytes, alignment/resource failures, and all eleven nonzero-path
operation-budget stops.

Private evidence under `.local/verification/native_completion/game_graph_debug`
binds the owner to source SHA-256
`e202fe748f38f92451a91869867b07e4d5f67c251ad5556acc34dc21cc9b8325`.
The bounded R3000 oracle covers all 27 original PCs across zero, nonzero,
alias, null-dispatch, and ignored-return cases. The visual workflow in
`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers, captures 98 PPM frames and
logs, and records the `SetGraphDebug(0)` receipt. Because this routine only
changes mapped debug bookkeeping, it has no direct native pixel effect.

# Team tactics update recovery

`nba97_game_team_tactics_update` owns the complete GAMEONLY routine at
`0x800747B0..0x80075D3F` (5,520 bytes, 1,380 instructions). The recovery is
based on the fresh Ghidra listing `game_800747b0.txt`, whose instruction stream
has SHA-256
`1c26b7364cec973a77600152182b1a5ada6c547fad563c80b29fb4484194a1ac`.
The ownership audit found only the typed call from the recovered match tick and
no earlier complete owner.

The routine preserves the original order of its 47 calls, mapped loads and
stores, delay instructions, callback-live register file, mutable stack frame,
and HI/LO. It retains signed halfword and byte behavior, wrapping timers,
tie-replacing minimum scans, runtime play-table reads, rejection loops, and the
`MULTU 0xAAAAAAAB` state. Unknown branch inputs stop after the original delay
instruction. Failed mapped accesses and callbacks publish the exact completed
prefix. The operation budget bounds retail loops without changing a successful
path.

All children remain typed full-machine boundaries. Existing scalar owners at
`0x8002AB70`, `0x8007066C`, and `0x800706E4` do not expose the full CPU ABI
needed at these call sites, so this adapter does not invent missing clobbers or
promote their narrow return values. A future composition may bind them after an
independent full-machine mapping is proved.

The match-tick adapter claims only the source-proven zero-argument service at
`0x80068E28` for entry `0x800747B0`. Since the legacy tick interface carries no
CPU state, callers must provide an independent full machine whose known return
address is `0x80068E30`, the value produced by the original JAL. The natural
test reaches that actual match-tick site through an explicit whitelist of
synthetic prerequisite services, runs the real owner, and refuses the next
`0x8006817C` service instead of claiming a continuing simulation.

Independent raw-instruction differential testing passed 14,704 cases and visited all 1,380 source PCs while comparing the full 2 MiB RAM image, all 34 machine words and masks, callback entry machines, mutable SP/T8/HI/LO, stack words, and access/call budget prefixes. The receipts are `team_tactics_update_differential.json`, `team_tactics_update_directed_differential.json`, and `team_tactics_update_final_differential.json`.

Focused checks cover complete five-actor and opposing-five-actor geometry/minimum scans, offense RNG/MULTU and typed play selection, bounded rejection and marker loops, forced and normal defense paths, zero-result fallbacks, the early negative-team exit and its prior stores,
phase/clock boundary behavior, both-negative flags, unknown predicates and
delay prefixes, malformed late knownness, atomic unknown stores, every access
budget on the early path, callback metadata and live machine mutation,
invalid callback machines, region overlap, deterministic full memory and
machine results, and adapter guards. The tests are always active in Debug and
Release builds.

This is CPU-side gameplay state recovery. Gameplay shown: NO - no direct visual
effect. No court, actor, packet, or renderer asset is created by this routine.


`captureGameTeamTacticsUpdate()` runs the same completing nontrivial actual
match-tick fixture without writing a capture file. It returns a JSON CPU receipt with
the recovered range and actual call site, operation/read/store/callback totals,
five-plus-five scan counts, geometry/minimum/timer state, typed child PCs, and
the subsequent crossing-half stop. Invalid fixture behavior throws
`std::runtime_error`. The receipt states that the entry machine is an explicit
independent snapshot because the legacy tick interface has no CPU ABI.


The completing capture accepts exactly 23 source-listed typed child events; each PC, target, delay, argument count, JAL return address, invocation, kind, and required mapped argument address is checked before the synthetic response is applied.

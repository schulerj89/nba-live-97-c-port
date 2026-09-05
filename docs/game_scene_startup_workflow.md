# GAMEONLY scene startup recovery

`nba97_game_scene_startup` owns the complete GAMEONLY routine at
`0x80048D5C..0x80048FE3` (648 bytes, 162 instructions). The boundary comes
from fresh Ghidra evidence in ignored local storage. The listing identifies
the routine hash as
`e4d0c3f2d16ed75081a530cb5140aa37f4a82abd36ec7cdd0a59fc0bed7b5744`;
the enclosing evidence text has SHA-256
`7d538dcb4b19de3bdea4986878d78be9a623d512bb541cab90234f1486ddb06f`.
The function catalog confirms two additional containing callers,
`FUN_8002DFF4` at call PC `0x8002E080` and `FUN_8002E0EC` at call PC
`0x8002E1C8`, besides scene-load wrapper call PC `0x8002DB78`. No complete
native owner existed before this recovery.

The owner uses the existing 32-GPR, per-byte-knownness register layout from
`Nba97GameMatchInitializeRegisters`. It forms the live 0x28-byte stack frame,
resets scene globals, queries eight controller slots, copies signed home and
away roster IDs, re-reads the active-entity root for each of ten entities,
starts four resource services, writes camera state, selects both display/draw
buffers, enables rendering, and reloads five saved GPRs through the final live
stack pointer. All guest addresses remain 32-bit values and all memory traffic
uses validated retained regions.

Every original callee is a typed callback. The callback receives the call PC,
delay-slot PC, target, argument count, operation index, retained memory and all
32 mutable GPRs. The eight `0x8008F224` calls reuse one typed kind; the two
display and two draw calls likewise reuse their respective kinds. No nested
routine is translated here. In particular, `0x80063EDC` remains the explicit
attributes dependency. The existing complete `nba97_game_player_attributes`
owner accepts projected player/entity arrays and returns aggregate effects. It
does not expose this call's live GPR outputs, guest stack, or every retained
access prefix, so its API cannot yet prove this full-register composition.

Source quirks retained by the translation include child mutation of controller
loop counters, compare values and store pointers; the child-controlled signed
roster loop counter; sign extension of all roster halfwords with byte-level
knownness; live entity-root reloads; selector inversion by equality with zero,
so any nonboolean selector becomes zero; source-order camera stores that leave
`0x800FA636` untouched; callback-mutated display/draw bases; JAL delay-slot
arguments; 32-bit address wrap; and epilogue reloads through callback-mutated
`sp`. Pure ADDIU/SLL/ADDU/SUBU/SLTI/SLTIU operations propagate raw values and
byte knownness. Unknown values stop only when an address, branch, or `JR`
requires them; unconditional branch and JAL delay work remains in the reported
prefix. Prefix stops report the exact source PC, address or child target.

`nba97_game_scene_startup_from_scene_load` is the narrow adapter for the frozen
scene-load wrapper's `0x8002DB78` child event. It shares retained memory and
the complete live GPR set. Nested refusal returns the exact scene-startup
prefix to the wrapper and does not claim that the child returned.

The focused tests generate all fixture memory at runtime. They cover normal
access and call order, controller match and mutation paths, signed and partial
known IDs, roster-loop signed boundaries and bounded runaway behavior, live
entity-root aliasing, selectors 0/1/nonboolean, selector and base mutation,
stack relocation and wrap, unknown data, mapping and alignment failures,
malformed callbacks, all child refusal positions, and every operation-budget
prefix. The integration test runs the production owner through the production
scene-load wrapper and verifies both completion and nested-refusal prefixes.

Visual classification: `Gameplay shown: NO - no direct visual effect`. This
routine updates retained CPU scene/rendering state and calls typed rendering
services. The synthetic services produce no pixels, so no gameplay or visual
transition is claimed.

Manager validation passes 22,835 direct owner checks, 305 natural-wrapper
checks and the complete 207-test asset-free CTest suite. Progress, recovery,
instruction semantics and roster configuration freshness checks pass. A private
independent interpreter matches 760 cases across all 162 original PCs, comparing
full retained memory, all 32 final GPRs, child-entry register files and every
operation-budget prefix across signed IDs, controller matches and four selectors.

The production scene-load capture now invokes this exact adapter after the
recovered random warm-up. Diagnostic roster/entity records are generated at
runtime; nineteen typed child responses explicitly stand in for unresolved
resource and rendering work. The source owner performs 184 operations (98 reads,
67 stores,19 calls), queries eight controllers with four matches, copies12+12
roster IDs and ten entity IDs, preserves the camera gap at800FA636, and changes
selector7 through0 to1. Render-enable80021498 becomes1. Shared capture assertions
were updated from their old synthetic-stage expectations of zero to these
source-proven values; the fixture still submits no GPU work.

Native evidence is ignored at
`.local/verification/team_select/game-entry-20260905-181603-c26a5b21/frames/`.
`scene_startup_verified.json` includes the state/call receipt nested in
`scene_load_trace.json`, input frame checkpoints and matching before/after hashes:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The retained image shows User Setup only. Gameplay remains unproven; the loop
entry probe still stops at80068C24 ->80066F88 with no simulation/frame pump.

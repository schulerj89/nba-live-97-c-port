# GAMEONLY scene resources recovery

`nba97_game_scene_resources` owns the complete GAMEONLY routine at
`0x80052C20..0x800530FB` (1,244 bytes, 311 instructions). The boundary comes
from the fresh Ghidra listing `game_80052c20.txt`, whose routine hash is
`d724ab2a6e1540262e72275fab86b346afcb52bd8c74186e16d1831922f00720`.
The complete local evidence file hashes to
`0a5e2b8104f3c874196a354fcf1877451d8755a2f3536491f2ffae68743f35aa`.
The only observed caller is scene startup at call PC `0x80048E94`. Repository
ownership and inventory searches found no earlier complete owner.

The owner keeps all 32 GPRs with one knownness bit per little-endian byte. It
uses validated guest-memory regions for every word access and exposes an exact
access journal. Arithmetic carries partial knownness without failing; an
unknown value stops execution only when the original source needs it for a
branch, effective address, or final `JR`. Every child receives the live full
register file after `JAL` has assigned `ra` and the source delay slot has run.
Children may change memory, stack location, loop registers, cursors, and every
other GPR. A missing, refusing, or malformed child retains the exact completed
prefix.

The source first creates a 0x20-byte frame and calls `0x800536A0`, then writes
the three scene globals at `0x800B72DC`, `0x800FB820`, and `0x800FAC20`. The
mode word at `0x800EB678` is reloaded at every evidenced source PC, so child
mutation can independently redirect later work. Normal mode loads the court
shadow archive, both unchecked team resource roots, ten home and ten away
entries, the letter archive and its 26 entries, player resources, head and
palette buffers, and the final net archive. Alternate mode keeps the original
presentation brackets, loads only the home team entries, selects the frontend
player archive, and publishes the FAT archive at `0x801063C4`, `0x800F0ED8`,
and `0x800F0ED4`.

All resource releases use fresh retained-pointer reads in source order. The
normal cleanup calls `0x8004FD48(700)`, synchronizes, releases
`0x80102918` and `0x800F9FC0`, and reruns the live mode test before
`0x800504A8`. The epilogue reads `ra`, `s2`, `s1`, and `s0` through the live
stack pointer, advances that pointer by 0x20, and requires the reloaded `ra`
only at the source `JR`.

The core exposes every direct child as a typed dependency: `0x800536A0`,
`0x8004D490`, `0x80029BFC`, `0x80029BCC`, `0x800516E4`, `0x80029BD4`, `0x800A3FEC`,
`0x80051294`, `0x80090160`, `0x8004DC08`, `0x8004FD38`, `0x800994F4`,
`0x80090698`, `0x8004FD48`, `0x800504A8`, `0x80050DD0`, `0x80050DC8`, and
`0x800479B8`. The natural adapter composes the existing complete `0x80029BFC`
retry loader when its `a0`, `a1`, `sp`, `ra`, `s0`, `s1`, and `gp` inputs are
fully known. It maps the loader's exact stack/store/call/epilogue prefix back
to all 32 GPRs and marks unreported caller-saved child outputs unknown. Partial
inputs fall back to the scene's full-GPR callback. The complete but narrower
`0x800536A0` court-roster helper, `0x800994F4` GPU synchronizer, and
`0x80090698` heap release owner still cannot prove every output GPR and active
stack effect required here. Treating those unreported registers as preserved
would invent state, so those entries remain explicit typed children.

The focused synthetic test covers the complete normal and alternate call and
memory sequences, both ten-entry loops and the 26-entry loop, all publications
and release arguments, every live mode reload, delay slots, callback-mutated
loop counters/cursors/resources, partial-byte knownness, unchecked wrapped team
indices, alignment and mapping failures, native-storage aliasing, malformed
callbacks, every child refusal on both paths, and every operation-budget prefix
on both paths. The natural integration runs the frozen complete
`0x80048D5C` scene-startup owner and dispatches its `0x80048E94` event through
`nba97_game_scene_resources_from_scene_startup`. Its six normal-path
`0x80029BFC` calls execute the recovered retry-loader owner and its synthetic
load-attempt service; all other startup and resource children remain explicit
synthetic services. Tests also cover the seven-loader maximum created by live
mode changes, the original null-result retry, partial-input fallback, attempt
refusal, and nested loader budget failure at its exact translated prefix.

Visual classification: **Gameplay shown: NO - no direct visual effect**. This
routine orchestrates retained CPU resource pointers and rendering-service
boundaries. Synthetic providers do not constitute scene loading or advancing
native court/player gameplay.

Manager review additionally compared the owner against an independent private
interpreter of the original instructions: 4,800 cases visited all 311 PCs and
matched full retained memory, all 32 final GPRs, every child-entry GPR and every
operation-budget prefix across normal/alternate modes and live mode mutations.
These ignored checks establish semantic behavior, not an exact-recomp claim.

Root validation passes11,190 focused checks,862 natural-startup integration
checks and all209 asset-free CTests. Progress, recovery, instruction-semantics
and roster-scenario freshness checks pass.

The native self-driving capture now routes the actual scene-startup event at
80048E94 through this production adapter. Its normal path performs182 parent
operations (46 reads,64 stores,72calls). Six complete recovered29BFC invocations
each execute a synthetic known-null attempt followed by success, giving8
operations,2attempts and1null each. The other66 calls are explicitly typed
archive/lookup/allocation/render/release fixtures. Generated CPU roots,10+10
team lookup entries,26 letter entries and all six releases are checked in
source order; fixtures do not establish actual asset loading or rendering.
The nested frame is807FFF48 and restoredRA80048E9C.

Ignored native evidence is at
`.local/verification/team_select/game-entry-20260905-182321-48fc9825/frames/`.
`scene_resources_verified.json` references the nested receipt in
`scene_load_trace.json` and the native input frame checkpoints. Before/after
scanout hashes match:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The image remains User Setup, with no advancing court/player state. The loop
probe remains blocked at80068C24->80066F88; no gameplay completion is claimed.

# Team Select retained text placement

2026-08-30. The native screen now retains separate label/value placement and
all four arrow nodes through entry, Cross, team replacement and presentation.
This recovers a bounded source state boundary. It does not establish allocator
history, first-arrow flash color, original GPU pixels, runtime timing or input.

## Missing state and correction

Previously `renderTeamSelect` selected two arrow positions and one label set
directly from the logical side, and drew all values at final coordinates. Those
coordinates already matched ordinary completed frames: every source movement
here has duration1, and the prior checkpoint correctly retained the frame before
callbacks. All98 baseline capture images remain byte-identical after this change.
Do not describe it as a demonstrated visual or input-latency bug.

The first absent datum was the pending movement on each newly constructed away
rank value: native had no object or command; original `4FA3C` leaves its anchor
at(388,218+16*c), with delta(-276,-96), duration1 and elapsed0. The focused C
module `recovered/team_select_placement.c` now owns that state, the ten distinct
labels, twelve value nodes and four arrows. C++ still owns resources and pixels.

| Objects | Construction before first text presentation | First presentation |
|---|---|---|
| Home/away names, values0/6 | Recreated at(388,86)/(112,86) | Unchanged; second line16 below |
| Home rank values1..5 | (388,122+16*c) | Unchanged |
| Away rank values7..11 | (388,218+16*c), delta(-276,-96) | (112,122+16*c) |
| Home labels, home entry | (248,122+16*c) | Unchanged |
| Away labels, home entry | (248,218+16*c), delta(0,104) | Hidden at(248,322+16*c) |
| Home labels, away entry | (248,122+16*c), delta(0,200) | Hidden at(248,322+16*c) |
| Away labels, away entry | (248,218+16*c), delta(0,-96) | (248,122+16*c) |
| Four arrows | x320,460,-458,-318; all y96 | Away entry adds500 to all four |

Here c=0..4. Name descriptors have no label nodes. All four arrows join group
120+entry page; Cross does not regroup them. `39BA8` queues+500 for old page0
or-500 for old page1. `4F7B8` queues opposite200-pixel vertical movements on the
two label sets; values do not move with Cross. A new command replaces a pending
command. On the next presentation `2B5AC` applies the complete delta; the next
tick clears relative motion without applying it again. The module retains
accumulated offsets but does not emulate either GPU primitive page.

`4EF40` recreates only the active six values at saved final descriptor anchors.
`2C244(old,new,2)` transfers color state but excludes relative/crop/target motion
flags and offsets. Thus every accepted team candidate, including repeated Random
candidates, resets value placement only. Labels and arrows survive. Native
inactive counters are canonical zero, not claims about uninitialized source RAM.

## The source wait bypass

`3CF70` creates a label before `3B26C` creates its value, and `2C6B0` prepends the
new node. The selected group head is therefore the value. Immediately after
entry, away rank heads7..11 report8 through `2C668`; the other heads report0.
After one movement tick all report0, before the later buffer-cleanup tick.

That query does **not** delay actual Team Select. `39D88` creates the two logo
descriptors12/13 with type41. `3D930` counts them into controller+10=2 at
8003DD1C; `3AE4C` at8003AE74/AE80 bypasses text settlement when that byte is
nonzero. All24 source first-poll cases perform one mandatory presentation before
input. An early hypothesis of an extra away-ranking entry delay was disproved
by this caller path. The native host preserves the bypass explicitly.

Every requested presentation advances live placement before composition,
including Help and Random waits. Callback mutations only affect a later frame.
The existing native crossfade still requires an uncounted entry preview; it
projects one movement tick on a copy and never advances live placement, tint,
RNG or input. Captures mark that preview separately. This is native transition
behavior, not proof of the original fade or absolute entry presentation phase.

## Construction history and remaining tint evidence

The full entry fixture executes `4FCD8 -> 3D930 -> 4FA3C`. Initial cleanup marks
layers0/1/2 for removal and requests one presentation; it does not clear stored
colors or the allocation hint. A controlled layer3 resident survives cleanup.
Then22 text objects are created: names0/6 each contribute one value, ten rank
descriptors each contribute label then value, and logos12/13 allocate no text.
The selected group is pulsed, then the four arrows are allocated with no intervening
presentation/removal. Only afterward are both names replaced and the heading
created, making29 allocations total in the bounded entry path.

Consequently, pre-arrow descriptor pulses cannot seed the four arrows via reuse
of those still-occupied slots. Their stored startRGB bytes still depend on older
pool occupants. The current renderer retains neutral arrow glyphs; first-flash
history and routing remain pending. Do not use visible128 or a separate cold RAM
dump as a universal stored-color initializer. A valid original capture at
8003D51C after each new arrow store, with state3/condition0/exists1 verified,
would provide the four node+30 RGB triples for that observed route. Full64-byte
nodes, controller arrow records, manager/pool/scan hint and primitive pages make
that capture auditable. Other routes still need their own history or a complete
producer trace from a known snapshot.

The subsequent upstream audit closes ordinary resource callbacks and identifies
the earlier press-start text producer. A direct successful route from a known
empty, zero-RGB pool conditionally preserves zero arrow start colors, but that
is not a universal initializer or an observed continuation. The later actual
demo-return Setup dump has50 nonzero stored RGB triples despite all200 slots
being free. See team_select_text_history_workflow.md for the source proof and
strict arrow-construction capture tool. General Help/menu/warm visits still
require their own history; the state3 entry fixture cannot stand in for them.

## Source owners and verification

These are full overlapping supporting extents, not a new game denominator.
All new instruction credit is0. Existing ledgers remain unchanged. Tests execute
bounded branches with controlled glyph/GPU boundaries; they do not translate all
types in `3B26C`, the allocator, or the complete frontend dispatcher.

| FEONLY owner | Bounded responsibility | New credit / full instructions | Remaining scope |
|---|---|---|---|
|8002B1E4|Relative command|0 /8|Generic caller domains|
|8002B204|Group command|0 /42|General groups/lifetime|
|8002B5AC|Apply delta and cleanup|0 /161|GPU pages and other durations|
|8002C244|Mode2 replacement|0 /155|Other modes and color history|
|8002C610|Node movement query|0 /22|Generic crop callers|
|8002C668|Head query|0 /18|General group lifecycle|
|8002C6B0|Prepend/new-node behavior|0 /515|Font layout and allocation history|
|80039BA8|Both arrow groups shift|0 /28|Other selector screens|
|80039D88|Two type41 descriptors|0 /28|Other graphic callers|
|8003AE4C|Graphics bypass, first poll|0 /210|Other states/physical input|
|8003B26C|Name/rank value creation|0 /1857|All other value types|
|8003CF70|Label then value creation|0 /137|Generic descriptor types|
|8003D690|Name recreation|0 /35|Generic descriptor visibility|
|8003D930|Entry ordering/graphic count|0 /828|Other states and full lifecycle|
|8004EF40|Active six value replacements|0 /56|Cache/palette runtime evidence|
|8004F7B8|Cross placement|0 /95|Tint/runtime evidence|
|8004FA3C|Entry placement|0 /145|Full text/GPU/runtime evidence|

- Original-MIPS/current-C:91,230 assertions/84 cases, including24 entry/focus/
  allocation-hint cases,96 Cross transitions,24 active-value replacements and
  12 controlled pending-command/replacement orders.
- Separate full-entry source audit:3,930 assertions/48 cases, covering12 focuses,
  allocation scan/wrap, surviving residents, Los Angeles and special-team names.
- Public tests: both entries,48 switches,12 refresh cases, replacement/cleanup,
  head queries,24 first polls through the graphics bypass and refusal guards.
- Extracted actual host/renderer:7 cases/1,451 assertions, including all12 entry
  focuses, all78 Random presentations and invented distinct positions proving
  the renderer reads each retained label/value/arrow independently.
- All8 injected host/renderer wiring defects and10 corrupted capture receipts
  are rejected. Preview classification is fixed by harness checkpoint, and
  arrow group retention is checked throughout every screen lifetime.
- Debug and release:46/46 CTest tests;98 repeated captures match across builds.
  Create Player:27/27 repeated captures,753 projected vertices,251 primary
  packet/order records and zero missing sampled texels. Real saves are unchanged.

Private evidence lives under `team_select/audit_a/arrow_motion/`,
`gameplay/audit_b/team_select_motion/` and `team_select/audit_c/text_motion_probe/`
within `.local/verification/`. Final capture runs: Debug
`run-20260830-193520-0f4a6c8b`, release `run-20260830-193649-ede7b5f0`, Create Player
`run-20260830-193610`. Logs use `.local/logs/team_text_motion_*`. Original runtime,
visual/timing/audio comparison and physical walkthrough remain pending.

# Native port status

Frontend capture baseline: code checkpoint `1702b81`, 2026-08-31. The later
court/tipoff checkpoint passes 125 Windows and 121 Linux core tests, but has
not connected gameplay. This page distinguishes live application paths from
tested subsystems; it is not an overall completion estimate.

**Playable basketball is not implemented.** There is no rendered playable court,
complete possession, full CPU/user match, or finished season/playoff mode. The
Windows frontend can accept an ordinary-exhibition setup, but its
`MATCH-HANDOFF-PENDING` boundary does not launch gameplay.

## What works in the Windows application

These paths require locally extracted original assets. “Implemented” below
means a native path exists; it does not mean every source dependency, original
frame, sound or save behavior has been accepted.

| Area | Connected native behavior | Remaining boundary |
|---|---|---|
| Boot and menus | Loading/legal/movie/title, Game Setup, Rules/Options, Help and local profile/settings stores | Native frontend boot is not proof of the complete original cold-start chain. Deeper menu, memory-card and mode paths remain incomplete. |
| View Rosters / View Player | Teams, players, portraits, stat layers/scrolling, Help and Cool Fact speech | The recorded fidelity score is scoped to these screens, not the rest of the game. |
| Re-order / Trade / Sign / Release / Reset | Roster edits, validation/refusals, child routes, native save/discard/restart and Reset paths | Timing, presentation and transitive source acceptance vary by feature. Local save formats do not implement a PS1 memory card. |
| Create Player | Durable 40-slot catalogue; manager/Edit/New/Delete; 32-field editor; articulated original-data preview | Created-player roster insertion/resolution is incomplete. Exact PS1 raster edges/interpolation and whole-screen visual acceptance remain open. |
| Exhibition Team Select / User Setup | Team browsing/ranks, profile and control edits, readiness, and an owned ordinary-match snapshot/presentation choice | No gameplay launch. Special-team and created-player handoffs, extension settings, season/playoff routes and full original runtime comparison remain open. |
| Frontend audio | Five-resource music bank with recovered routing, menu sounds and Cool Fact speech through native output | Host playback uses bounded substitutions for original device observations. It does not prove exact SPU voice/timing behavior, all transition callers, gameplay audio or every original music route. |

Current implementation and evidence details:

- [Re-order](reorder_rosters_workflow.md), [Trade](trade_rosters_workflow.md),
  [Sign](sign_free_agent_progress.md), [Release](release_players_progress.md),
  [Reset](reset_rosters_progress.md), [Create Player](create_player_progress.md).
- [Team Select](team_select_workflow.md), [User Setup](user_setup_workflow.md),
  [match snapshot](match_snapshot_workflow.md),
  [presentation selection](match_presentation_workflow.md).
- [Frontend music](music_playback_workflow.md),
  [View Rosters verification](view_rosters_verification_workflow.md).

The SDL2 compatibility application is a separate, smaller frontend. Linux core
test results do not establish parity with the Windows application.

## Recovered and tested, but not a live match

Portable C owners implement original behavior; native C++ adapters own storage,
resource lifetimes and platform effects. Some of these sources are already
linked into the application, but linking is not the same as running them from
its frontend-to-match path.

| Subsystem | Existing work | What it does not establish |
|---|---|---|
| Match state and periods | Owned accepted players/teams/controllers, lineup/binding/role helpers and composed period initialization under explicit entry conditions | Natural cold/warm loader completion, every substitution dependency, or a running match. See [match runtime](match_runtime_workflow.md). |
| Player updates | Animation/queue, motion/pose resources, bounded physics/jump and input-edge owners with native composition tests | Whole-frame simulation, ball ownership, passes/shots, contact, AI or an actual possession. See [player updates](match_player_update_workflow.md) and [input edges](match_input_edges_workflow.md). |
| Tip contact and ball release | Complete player part-matrix/hand-endpoint owner `55368`, hand/body contact helpers, the post-acquisition tipoff continuation, and ball-release owner `58610`; composed release comparisons end with a loose ball and intended receiver in phase `82` | Live actor-root/camera production, hand-path integration, upstream collision/acquisition, ball simulation and a natural first possession remain unconnected. See [player geometry](game_player_geometry_workflow.md), [tipoff phase](game_tipoff_phase_workflow.md) and [ball release](game_ball_release_workflow.md). |
| Rendering and resources | Body geometry, textures, names/fonts/text pools, heap owners, retained CPU allocations and VRAM transfers; court packet projection now composes with native pixel drawing in flat/textured fixtures | No live rasterized court, complete camera/render loop or gameplay frame. Actual resource/caller integration and remaining raster fidelity domains are still required. See [court packets](game_court_packets_workflow.md), [packet drawing](game_packet_renderer_workflow.md) and [render backend](game_render_backend_workflow.md). |
| Audio startup and transfers | Game sound entry point, common attributes, music reset, callback registration, SPU heap, PIO/DMA sample ownership, interrupt/controller and event composition | Natural host audio initialization, actual callback cadence, complete voices/synthesis, physical device timing or full-match sound. Some real resource transfers still stop where rounded source tails lack proven ownership. See [audio startup](audio_startup_workflow.md) and [sample backend](spu_sample_backend_workflow.md). |

Asset-free CTests use synthetic fixtures. Private source comparisons additionally
exercise original instructions and selected real resources under documented
conditions. Those comparisons are development tools, not an interpreter or
recompilation runtime embedded in the native application. A complete source
routine still depends on its caller supplying the right state and its callees
having real implementations.

Confirmed original bugs are preserved and commented with source provenance;
native-port defects are fixed. See [preserved behavior](preserved_original_bugs.md).
Native ownership checks can refuse an unsupported boundary without inventing
original values, fabricated successful hardware effects or replacement gameplay.

## Reading the progress numbers

The [generated report](progress.md) combines separate evidence inventories:

- Its **68 / 3,701 function records** describe entries currently registered in
  `config/decomp/recovered_functions.json` against the five scoped Ghidra binary
  inventories. The registry has not caught up with newer source modules. Its
  **8 behavior-complete records** are likewise catalogue claims, not the total
  number of recovered routines in the repository. Neither count measures the
  remaining effort or the fraction of the game that runs.
- Native feature statuses are manually maintained, unequal-sized milestones.
  `partial` can mean a working frontend with fidelity gaps or a subsystem with
  only test integration; its notes explain which. `verified` applies to the
  named scope, not every transitive dependency or all original modes. No
  half-credit or overall game percentage is assigned.
- View Rosters' **997 / 997 accounted instructions**, **12 native scenarios**
  and **91.82% weighted fidelity** use fixed, local denominators. They do not
  measure the whole frontend or basketball. Original trace scenarios, source
  accounting, screenshots and instruction matching remain distinct tiers.
- Older scoped ledgers can retain pending labels after later host work. For
  example, Re-order's ten-owner instruction accounting does not reflect every
  later child/save/Reset integration. Those labels must not be interpreted as
  evidence that the current screen has no persistence.

`python tools/report_progress.py --check` verifies that generated files agree
with their committed inputs. It cannot detect missing recovery records or
validate all feature claims. Adding catalogue entries requires source identity,
scope and evidence review; CTest totals are not a substitute denominator.

## Validation checkpoint and remaining acceptance

At `1702b81`, Windows builds passed **122/122 CTests** in both Debug and
RelWithDebInfo. [GitHub run 33413002259](https://github.com/schulerj89/nba-live-97-c-port/actions/runs/33413002259)
passed **118/118 Linux core CTests** and the metadata/verification workflow.
The four-test difference is Windows-specific coverage. These results do not
mean every frontend walkthrough was recaptured at that checkpoint, nor that
a full match was tested.

The later court/tipoff checkpoint passes **125/125 Windows CTests** in Debug
and RelWithDebInfo, and **121/121 Linux core CTests** locally under GCC 11.4.
These totals include the three new test targets; they do not replace the
historical GitHub run above with an unobserved CI result.

The additions also have directed native tests and private original-source
comparisons. The tipoff/release
composition covers 96 supplied-state cases per build, reaching loose-ball
phase `82`. Court verification covers all 507 owned instruction locations and
1,760 comparisons per build, with separate integer-arithmetic checks. These
results are not fresh gameplay captures and do not advance the playable-match
acceptance gate.

Full completion still requires natural frontend-to-match startup, a playable
court and first possession, complete simulation/AI/rules, full CPU and user
matches through results/return, every original mode and its persistence,
remaining roster integration, all reachable audio/music, and original behavior
and presentation acceptance. The [completion acceptance ledger](native_completion_acceptance.md)
records those larger gates and historical checkpoints; no gate is closed merely
because a helper's tests pass.

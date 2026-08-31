# Native port status

Checkpoint 28 passes 135 Windows tests in both build configurations and 131
Linux core tests locally. Its ball-rendering and marker-resource additions also
have separate component comparisons. The preceding checkpoint 27 (`90a0bcc`)
passed 129 Linux core tests in GitHub CI; checkpoint 28's CI is pending publication.
The local totals include full-suite revalidation after the native packet-reader fix.
This page distinguishes live application paths from tested subsystems; it is
not an overall completion estimate or a claim of new frontend captures.

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
| Player rendering pass | Complete `52914` composes actor-root `5200C`, part-matrix/hand-endpoint `55368`, body packet producer `525AC`, shadows and off-screen indicators against shared retained state; original-resource comparisons use actual normalized bodies and poses | Natural entity/control/loader state, resource-lifetime integration, shared frame submission and live actor rendering remain unconnected. See [player pass](game_player_frame_workflow.md), [actor root](game_player_root_workflow.md), [player geometry](game_player_geometry_workflow.md) and [body projection](game_player_projection_workflow.md). |
| Camera and controller | Recovered `51098`, including controller `4EA88` and input helper `8F224`, produces camera state through native fixed-point math and explicit retained inputs | Actual timing, pad/device and monitor effects remain required external boundaries. Natural caller state and integration into the live render loop are unproven; component coverage is not complete controller-path coverage. See [camera workflow](game_camera_workflow.md). |
| Tip contact and ball release | Contact helpers, the post-acquisition continuation and ball-release owner `58610` have separate tests; composed release ends with a loose ball in phase `82` | Live hand-path integration, upstream collision/acquisition, ball simulation and first possession remain unconnected. See [tipoff phase](game_tipoff_phase_workflow.md) and [ball release](game_ball_release_workflow.md). |
| Ball rendering | Complete ball/reflection `49300` and ground-shadow `49D34` use the existing player-frame adapter's retained buffers and geometry; `ball` and `ballShadow` have original/native component comparisons | Packet rendering does not implement ball simulation, attachment selection or possession. Natural entity/resource arrival, shared frame submission and a live match remain unconnected. See [ball pass](game_ball_frame_workflow.md). |
| Ball/shadow/arrow resources | `4D490` through `4CAF4` initializes ball/reflection/shadow packets and arrow templates, copies palettes and requests real BALL/ASDW image uploads through the existing image/VRAM owners | Load, release and SDK synchronization remain required external operations. Packet XY and ordering links belong to later render passes; released image pointers are not retained resource owners. Source comparisons do not prove cold loader/heap execution or natural frame entry. See [marker resources](game_player_marker_resources_workflow.md). |
| Court resource setup | Texture loop `479B8:487B8..48894` uploads actual XATL images. The after-load tail `48A4C..48D28` normalizes court references and allocates/initializes edge storage through shared `90160/901EC` and the recovered heap owner | Neither slice implements the full `479B8` loader or natural resource arrival. Court allocation flags are `0`, while text pools use `0x20`; neither implies zero-filled payload. Unknown bytes remain unknown. See [court textures](game_court_textures_workflow.md) and [court resources](game_court_resources_workflow.md). |
| Pixel rendering | Retained CPU/VRAM storage, court packet projection and native pixel drawing compose in fixtures. Packet reads permit unknown unused bytes while requiring consumed fields and preserving source-memory knowledge. Six initialized ball/reflection/shadow diagnostic views render | No live court or complete camera/render loop. Diagnostic renders use fixture camera, entity and ordering state, with no court or players in the ball views; they are not gameplay captures. See [court packets](game_court_packets_workflow.md), [packet drawing](game_packet_renderer_workflow.md) and [render backend](game_render_backend_workflow.md). |
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

At checkpoint 28, Windows builds passed **135/135 CTests** in both Debug and
RelWithDebInfo; local Linux passed **131/131 core CTests**. Its CI is pending
publication. The preceding checkpoint 27 (`90a0bcc`),
[GitHub run 33431996636](https://github.com/schulerj89/nba-live-97-c-port/actions/runs/33431996636)
passed **129/129 core CTests**; the GitHub metadata/verification workflow also passed.
The four-test difference is Windows-specific coverage. These results do not
mean every frontend walkthrough was recaptured at that checkpoint, nor that
a full match was tested.

Checkpoint 28's separate ball-rendering evidence
includes 585 original/native C cases per build with 16,111 ordered stores and
95 matching refusal prefixes. Another 53 cases compare the actual C++ adapter,
including all 64 geometry words and retained memory knowledge. The tests use
explicit camera, entity, ordering and packet inputs; they do not establish
resource arrival, ball simulation or a natural gameplay frame.
The BALL/ASDW initializer has separate whole-asset and source comparisons,
including load/release/synchronization refusals. Its explicit loader and sync
fixtures must not be mistaken for completed native cold-start services.
An initialized-packet composition additionally runs the actual C++ ball pass
followed by its ground shadow across 60 combinations per build: 15 animation
frames, both banks and reflection enabled/suppressed. All 3,720 stores,
persistent bytes/knowledge and 64 geometry words match original execution.
Both released source containers are unavailable and never read. Camera,
entity, control and ordering inputs remain fixtures, not natural frame state.

A native packet-reader bug initially rejected the ball-shadow packets because
UV2/UV3 contain unused high halves whose source bytes remain unknown. The fix
requires only consumed bytes without modifying retained RAM or its knowledge.
Required tags, opcodes, coordinates, UVs, CLUTs and texture pages still refuse
when unavailable. Six private diagnostic views now draw the initialized ball,
reflection and shadow: both banks at animation frames 0, 7 and 14, with six
triangles and 494 written pixels each. They contain no court or players and
do not come from a natural game loop. Fresh full suites after this correction
pass the 135 Windows and 131 Linux totals above; CI still awaits publication.

Checkpoint 27's separate player-pass composition
compares 120 actors and 175,272 ordered stores, including all 195 instructions
in caller `52914`. Camera comparisons cover 787 cases per build and all 64
retained geometry words. They reach all 127 camera and 147 Euler instruction
locations, but only 1,081 of 1,196 controller locations; no full controller
coverage is claimed. Camera/input/device effects use explicit test conditions.
Neither comparison proves natural match entry or a gameplay frame.

Checkpoint 26's body-projection comparisons
cover all 635 owned instruction locations, actual normalized body/pose inputs
and supplied geometry controls, including the original `ZSF3` value `341`.
The bounded court-resource work covers 240 owned instruction locations and
composes with the actual allocator on YATL and YNEU data, requiring six
allocations each. That checkpoint established component behavior under supplied
entry state, before the separate player-pass work above; it did not establish
the full `479B8` loader or a natural frame.

Checkpoint 25's separate component evidence
includes 80 original pose/body actor-root-to-`55368` composition cases; all 84
XATL images through the bounded texture loop, with 246 transfer events and
23,556 known VRAM pixels agreeing across three comparisons; 4,000 flat/Gouraud
line comparisons per build with no pixel mismatches; and 729 original-source
cases for SDK no-data return handling. These are supplied-state experiments,
not a complete loader, natural host frame or gameplay capture.

Earlier components also have directed native tests and private original-source
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

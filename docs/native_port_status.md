# Native port status

Checkpoint 34 local verification passes 157 Windows tests in Debug, RelWithDebInfo
and Release and 153 Linux core tests locally. It adds the complete outer match
tick and reached frame-pump CPU owner.
Checkpoint 33 (`5d6a09b`) passed 152 Linux core tests in
[GitHub CI](https://github.com/schulerj89/nba-live-97-c-port/actions/runs/33452019236).
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
| Render-frame sequencing | Complete `49018` CPU order and scratch redraw guard; C++ adapter shares memory/geometry across actual pose, net, player, court, attachment, ball, label and marker owners | Camera/display/sync/submission providers and natural resource state remain required. Separate C++ checks reach native net completion and a player-input refusal; no full frame or gameplay launch is claimed. See [frame driver](game_match_frame_workflow.md). |
| Outer match tick | Complete `68BF8` simulation loop plus reached `2DD84` frame pump preserves period timing, live global rereads and original incoming-`s6` register quirks; typed child boundaries bind player update, ball simulation, net transform and match frame owners | Thirty-seven other synchronous source services and natural caller state remain required. Compiling the owner does not launch gameplay or prove a court, possession, rendered frame, or full match. See [match tick](game_match_tick_workflow.md). |
| Net and rim rendering | Complete `4B1A4/4B6C4/4BA84/4C144`, native matrix/projection and actual ZNET codec; original-resource component comparisons | Original loader arrival, surrounding frame services and natural state remain required. See [net workflow](game_net_workflow.md). |
| Ball attachment | Complete `57F5C/58120/581C0` and `2D37C` hand lookup; actual normalized-memory C++ adapter; frame driver selects the original modes | Does not acquire possession or implement loose-ball physics. Mode one deliberately retains ball height. See [attachment workflow](game_ball_attachment_workflow.md). |
| Match state and periods | Owned accepted players/teams/controllers, lineup/binding/role helpers and composed period initialization under explicit entry conditions | Natural cold/warm loader completion, every substitution dependency, or a running match. See [match runtime](match_runtime_workflow.md). |
| Player updates | Animation/queue, motion/pose resources, bounded physics/jump and input-edge owners with native composition tests | Whole-frame simulation, ball ownership, passes/shots, contact, AI or an actual possession. See [player updates](match_player_update_workflow.md) and [input edges](match_input_edges_workflow.md). |
| Player rendering pass | Complete `52914` composes actor-root `5200C`, part-matrix/hand-endpoint `55368`, body packet producer `525AC`, shadows and off-screen indicators against shared retained state; original-resource comparisons use actual normalized bodies and poses | Natural entity/control/loader state, resource-lifetime integration, shared frame submission and live actor rendering remain unconnected. See [player pass](game_player_frame_workflow.md), [actor root](game_player_root_workflow.md), [player geometry](game_player_geometry_workflow.md) and [body projection](game_player_projection_workflow.md). |
| Camera and controller | Recovered `51098`, including controller `4EA88` and input helper `8F224`, produces camera state through native fixed-point math and explicit retained inputs | Actual timing, pad/device and monitor effects remain required external boundaries. Natural caller state and integration into the live render loop are unproven; component coverage is not complete controller-path coverage. See [camera workflow](game_camera_workflow.md). |
| Tip contact and ball release | Contact helpers, the post-acquisition continuation and ball-release owner `58610` have separate tests; composed release ends with a loose ball in phase `82` | Live hand-path integration, upstream collision/acquisition, the outer simulation caller and first possession remain unconnected. See [tipoff phase](game_tipoff_phase_workflow.md) and [ball release](game_ball_release_workflow.md). |
| Ball simulation and scoring | Complete `6EF60` integer ball tick with bounded collision/rim helpers, complete `6DC18` scoring/rim-grid chain, and complete `6E7AC` scorer/actor selection with its `58AA8/6E734` CPU leaves; source comparisons preserve wrapped arithmetic, traps and callback prefixes | The outer `68BF8` match tick, gameplay audio/UI services and natural startup are not connected. This is not a possession or gameplay proof. See [simulation](game_ball_simulation_workflow.md), [scoring](game_ball_scoring_workflow.md), and [scorer/actor state](game_scoring_actor_ai_workflow.md). |
| Ball rendering | Complete ball/reflection `49300` and ground-shadow `49D34` use the existing player-frame adapter's retained buffers and geometry; `ball` and `ballShadow` have original/native component comparisons | Packet rendering does not implement ball simulation, attachment selection or possession. Natural entity/resource arrival, shared frame submission and a live match remain unconnected. See [ball pass](game_ball_frame_workflow.md). |
| Ball/shadow/arrow resources | `4D490` through `4CAF4` initializes ball/reflection/shadow packets and arrow templates, copies palettes and requests real BALL/ASDW image uploads through the existing image/VRAM owners | Load, release and SDK synchronization remain required external operations. Packet XY and ordering links belong to later render passes; released image pointers are not retained resource owners. Source comparisons do not prove cold loader/heap execution or natural frame entry. See [marker resources](game_player_marker_resources_workflow.md). |
| Court resource setup | A canonical source-order owner composes all `479B8..48D5C` child intervals; texture selection/upload and the after-load tail normalize court references and allocate edge storage. Exact `9BF98` page calculation and native heap release are reusable services | Real loader/GPU sync services, allocation-registry population and outer `52C20` producers remain unconnected. Allocation flags `0` do not imply zero-filled payload. See [startup sequence](game_court_startup_sequence_workflow.md), [startup bridge](game_court_startup_workflow.md), [packet startup](game_court_packet_startup_workflow.md), [page offset](game_page_offset_workflow.md), [court textures](game_court_textures_workflow.md) and [court resources](game_court_resources_workflow.md). |
| GPU synchronization | Canonical `994F4/9B9B4/9BAFC/9B57C/9BB30` owner includes the reached `9BDB4(-1)` timer path and `986F8` halfword exchange, with exact queue, interrupt-mask, MMIO, callback and timeout/reset order. The bounded GAMEONLY diagnostic now connects natural caller `29AAC`, defers both preceding MoveImage packets, and captures their pending-to-complete VRAM transition. | A physical production graphics backend is not connected. Backend synchronization status remains deliberately separate from the original routine's return value. See [GPU synchronization](game_gpu_sync_workflow.md). |
| Display masking | Complete 39-instruction PsyQ `SetDispMask` wrapper at `99458` is connected to natural caller `29AB4`; it preserves debug/clear/table order, active-low GP1(03h), raw child return, unguarded dispatch and live o32 reloads. The self-driving diagnostic captures black masked scanout and the enabled completed buffer. | Retail `9B16C` is a typed diagnostic service boundary, not a physical or production GPU connection. Generated scanout is not a court/gameplay frame. See [display masking](game_display_mask_set_workflow.md). |
| Resource-validator registration | Complete six-instruction `A3E20` owner is connected to natural caller `29ABC`; it installs whole-file CRCF callback `A3D60` at `D7B1C`, preserves its unconditional overwrite and incidental pointer return, and captures pixel-identical before/after scanout. | The installed callback remains a separate untranslated function and the production host loader is not redirected. This registration does not validate or load a match asset. See [validator registration](game_resource_validator_install_workflow.md). |
| Frame-rate tracker reset | Complete 14-instruction `A7738` owner is connected to natural caller `29AD4`; it clears five GP-relative tracker words, samples retained clock leaf `A5810` into `D7B4C`, preserves pre-callback order, unguarded storage, incidental `v0` and live `ra`, and captures pixel-identical before/after scanout. | The retained clock is a deterministic source boundary, not host cadence. Consumer `A7460` and actual match timing remain separate; the auxiliary word's role is intentionally unclaimed. This does not render or prove gameplay. See [frame-rate reset](game_frame_rate_reset_workflow.md). |
| Match-session orchestration | Complete 165-instruction `2D8D4` owner is connected to natural caller `29ADC`; it defines both 512x240 buffer pairs, preserves the optional team-location substitution and retail recheck/reloaded-index bugs, crosses four ordered match stages, clears the exit surface and performs eleven presentation waits around DrawSync. The self-driving receipt records all 23 ordinary-path children and pixel-identical before/after scanout. | Initialization `2DB90`, scene load `2DB68`, loop `2DC38` and teardown `2DC58` remain explicit boundaries. The diagnostic does not fabricate their court/gameplay work, so this is orchestration proof rather than a playable possession. See [match-session owner](game_match_session_workflow.md). |
| Loading-screen composition | Complete 50-instruction `29E58` owner is connected to natural caller `29AE4`; it loads `zloadscr.psh`, resolves `LdS1`, brackets three existing `946B8` image-owner calls with four DrawSyncs, uploads at `(0,0)`, `(0,256)` and `(512,0)`, and releases the resource. Self-driving full-VRAM captures prove each generated-fixture placement and preserve the silent null-archive/unchecked null-image quirks. | Loader, lookup, DrawSync and release use concrete diagnostic service fixtures, not the production filesystem/GPU/heap path. Captured pixels are generated evidence rather than retail art or a gameplay frame. See [loading-screen compositor](game_loading_screen_workflow.md). |
| Resource-load retry wrapper | Complete 17-instruction `29BFC` owner is composed at natural `zloadscr.psh` caller `29E70` and `feload.bin` caller `29AFC`. Exact native callback logs prove one and two null retries respectively, and four self-driving before/after captures prove the non-rendering wrapper has no direct pixel effect. Cached arguments, successful live `v0`, mutable epilogue reloads and the persistent-failure infinite loop are retained. | Actual `941C8` filesystem/device/allocation work remains a typed diagnostic boundary. Generated successful pointers do not establish production asset ownership or a gameplay frame. See [resource loader](game_resource_loader_workflow.md). |
| Heap payload-size query | Complete nine-instruction `90D60` owner is composed at natural FELOAD caller `29B08`. A diagnostic load descriptor is published into the retained heap, actual recovered lookup `90618` finds it, and descriptor word `+14` returns requested size 5136. Exact logs and two self-driving pixel-identical frames prove the CPU-only effect; unchecked low-RAM null read, 32-bit wrap, malformed sentinel and live `ra` quirks remain. | Production `941C8` allocation ownership is still a typed fixture, and second caller `A7200` is not composed. This does not render or prove a gameplay frame. See [heap payload-size query](game_heap_payload_size_workflow.md). |
| CD synchronization wrapper | Complete eight-instruction PsyQ `CdSync` wrapper `9DBA0` is composed at natural main caller `29B34` after the first twenty post-FELOAD waits. It forwards `(0,NULL)` to typed internal service `9E740`, retains its `CdlComplete` value `2`, and emits exact logs plus pixel-identical before/after frames. Raw arguments, live/unknown child `v0`, mutable saved `ra`, and no wrapper-added timeout or normalization remain. | The 160-instruction internal `CD_sync` service and physical CD/device behavior are not translated by this wrapper. Three other callers are identified but not composed. This does not render or prove gameplay. See [CD synchronization wrapper](game_cd_sync_workflow.md). |
| Pixel rendering | Retained CPU/VRAM storage, court packet projection and native pixel drawing compose in fixtures. Packet reads permit unknown unused bytes while requiring consumed fields and preserving source-memory knowledge. Six initialized ball/reflection/shadow diagnostic views render | No live court or complete camera/render loop. Diagnostic renders use fixture camera, entity and ordering state, with no court or players in the ball views; they are not gameplay captures. See [court packets](game_court_packets_workflow.md), [packet drawing](game_packet_renderer_workflow.md) and [render backend](game_render_backend_workflow.md). |
| Audio startup and transfers | Game sound entry point, common attributes, music reset, callback registration, SPU heap, PIO/DMA sample ownership, interrupt/controller and event composition | Natural host audio initialization, actual callback cadence, complete voices/synthesis, physical device timing or full-match sound. Some real resource transfers still stop where rounded source tails lack proven ownership. See [audio startup](audio_startup_workflow.md) and [sample backend](spu_sample_backend_workflow.md). |
| Gameplay audio requests | Complete `29258/29590` CPU request routing with `29200/AB0B8/93D94/93DD4` leaves; `AC080` is proven equivalent to the frozen voice-program owner | The general `93734` scheduler remains a synchronous refusal boundary, and the chain is not reached from a natural match. Request routing does not prove a host device emitted audible sound. See [gameplay audio](game_gameplay_audio_workflow.md). |

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

Checkpoint 34 local verification passes **157/157 CTests** in Windows Debug,
RelWithDebInfo and Release and **153/153 core CTests** on Linux. Checkpoint 33 (`5d6a09b`)
passed **152/152 core CTests** in
[GitHub run 33452019236](https://github.com/schulerj89/nba-live-97-c-port/actions/runs/33452019236).
The four-test difference is Windows-specific coverage. These results do not
mean every frontend walkthrough was recaptured, nor that a full match was
tested. Publication and the exact GitHub CI result are tracked separately from
these local results.

Checkpoint 29's frame-driver corpus compares 1,022 cases per configuration
against original `49018/535C8/55F0C` execution, covering all 235 locations and
378 retained failure prefixes. Its child services are explicit fixtures.
The C++ backend separately executes a complete native net pass and preserves
shared geometry/normalized memory through a deliberately unavailable player
input. It does not yet run a complete native frame. Net component comparison
covers 658 cases per configuration, including all 30 original ZNET frames.
Attachment has 1,756 C cases plus 153 actual normalized C++ adapter cases per
configuration. The court bridge has 418 source cases per configuration and
separate native heap-release composition tests. See their workflow pages for
source boundaries, unchanged original quirks and actual integration limits.

Checkpoint 30 evidence adds independently frozen owners for pose,
labels, markers, ball simulation, scoring, court roster/interactive/packet
startup, court-frame composition, the fixed-global net transform and exact
page-offset calculation. The canonical startup sequence accounts for all
1,257 source PCs across its frozen child corpus. The scoring comparison runs 1,924 original-CPU cases
per configuration across 973 reached PCs; the page-offset comparison exhausts
all 65,536 live graphics-mode transitions. Court composition compares 555 cases
and all 507 owned PCs. Each workflow states its explicit service boundaries.
None of these component proofs crosses the Windows `MATCH-HANDOFF-PENDING`
boundary or establishes a visible gameplay frame.

Checkpoint 31 adds the complete 431-instruction `6E7AC/58AA8/6E734`
scorer/actor and AI-state owner. Its original-CPU comparison passes 3,541 cases
per MSVC configuration, including 91,914 ordered stores, 185,360 reads, 729
typed service calls, 2,694 refusal prefixes and every owned PC. Gameplay audio
and score/UI calls remain real typed boundaries; this checkpoint does not
fabricate those effects or connect the owner to natural match entry.

Checkpoint 32 adds the complete 335-instruction gameplay-audio CPU request
owner across `29200/29258/29590/AB0B8/93D94/93DD4`. Its original-CPU comparison
passes 2,672 cases per MSVC configuration and visits every reachable owned PC;
`294B8` is statically unreachable. A separate 2,000-case comparison per
configuration proves that the `AC080` frontier maps to the frozen native
voice-program owner. The general scheduler remains an explicit synchronous
boundary. These are routing and retained-state proofs, not audible playback,
device timing, or a natural gameplay claim.

Checkpoint 33 adds the canonical 437-instruction GPU synchronization closure.
The original-CPU comparator passes 4,114 cases per configuration, reaches all
437 claimed PCs, exhausts all 4,096 queue head/tail pairs, and compares 8,433
device reads plus 26 writes in exact order. A separate 10,000-case native
backend oracle per configuration covers delayed completion and rejects 228
fake acknowledgements. This is component/source proof; it does not connect a
physical GPU, render a match frame, or cross natural startup.

Checkpoint 34 adds the complete 402-instruction outer `68BF8` match tick and
reached `2DD84` frame pump. The original-CPU comparison passes 512 directed
cases per configuration across all 402 PCs, with 30,520 ordered events and 966
exhaustive operation-budget refusal cases retaining 34,218 prefix events. Four
already recovered children are distinct typed mandatory services; the other
source calls also refuse rather than accepting a successful no-op. This does
not supply natural caller/resource state or establish a court, possession,
rendered frame, or completed match.

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
passed the 135 Windows and 131 Linux totals above, including GitHub CI.

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

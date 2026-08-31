# Team Select: bounded vertical slice

Checkpoint: 2026-08-30. This implements the first requested milestone, not
end-to-end confirmation or gameplay. Nothing in this tranche embeds or runs an
emulator/recompilation. Original art, text, adjustments, RNG seed, captures,
disassembly and reference data stay under ignored .local paths.

## Working native boundary

Game Setup Start enters original state3/owner8004FCD8 for exhibition mode.
Cross changes a Setup card; Square changes it backward. Choice counts are
4/3/3/3 for quarter/mode/style/level. The unused superstar image is not a choice.
Quarter/mode/level are session state; style shares FrontendSettings/Rules in
memory. This work does not extend the settings file format or add autosaving.
Season/playoff Start routes remain pending and cannot silently enter exhibition.

Team Select uses ZSET1, title ba08, original fonts, layout, deformed logos,
two indexed team palettes, ordinal ranks, tint and Help. Home is RIGHT and starts
at ID3; away is LEFT and starts at ID24. Name scans use IDs0..30. The other five
criteria scan category ranks, not numeric team IDs. IDs29/30 are All-Star teams,
never the roster editor's free-agent descriptor29. Same-team pairs are permitted.

Arrows navigate, C switches side while retaining criterion, V randomizes,
F opens Help, Right Shift returns to Setup. Keyboard X is not Cross or Cancel.
Escape/Space/Backspace/H/F1 are existing native convenience aliases; they are
not claims about original keyboard bindings. Select retains scanned teams and
the descriptor focus. Exit updates remembered regular teams only for IDs<29,
on both Select and Start; there is no Select rollback.

Start waits for changed input through the recovered exit barrier and enters
state5/User Setup. This now uses the original ba39/cnt3/cnt2/cnt1 assets,
eight-controller assignment logic, optional profile selection, readiness,
name editing, both Help pages, warning/delete modals and Select return.
Its exact-name transactions are separate from normalized native profile CRUD.
See user_setup_workflow.md for this bounded implementation and its remaining
runtime and presentation boundaries. A successful readiness gate now prepares an
owned ordinary-exhibition snapshot; unsupported fields remain explicit. See
match_snapshot_workflow.md. MATCH-HANDOFF-PENDING still blocks gameplay launch.

## Architecture and data safety

- recovered/team_select.c owns selector behavior, six-word RNG and bounded
  random presentation schedule. recovered/team_select_poll.c owns whole-mask
  polling, repeat waits, callback continuations and changed-input exits.
  recovered/team_ratings.c owns weighted scores
  and stable rank ordering. recovered/game_setup.c owns card choice wrapping.
- TeamSelectAssets loads a bounded private pack and derives ranks from the
  CURRENT RosterDatabase::resolveTeamSlots result. Immutable team metadata and
  the roster save base identity are never overwritten with runtime ranks.
- main_menu.cpp composes original assets; win32_main.cpp owns native resources,
  input/transition routing and deterministic capture. Shared Help, palette,
  title, tint and sound systems are reused.
- The native rating adapter requires contiguous rosters of8..15. This is an
  adapter guard, not a recovered restriction inside the original arithmetic.
  Invalid/missing packs or rejected roster loads refuse entry instead of using
  placeholders or silently reverting to stock rosters.
- Created-player catalogue bytes survive the round trip. Existing created-player
  insertion/resolution is unfinished; no claim is made that created players
  already participate in team ranks or a match.
- No Create Player model, mocap, hierarchy, projection or texture implementation
  is changed. Its established numerical tests remain required.

## Reproduce

Run from the repository root after extracting the existing private packs:

~~~powershell
python tools/extract_team_select.py
python tools/extract_user_setup.py
python tools/extract_match_setup.py
pwsh -NoProfile -File scripts/build.ps1 -Configuration Debug -AllTargets
pwsh -NoProfile -File scripts/verify_team_select.ps1 -SkipBuild
~~~

The verifier opens fresh isolated settings/profile/created/roster paths under
.local/verification/team_select, checks the unsafe-default-path refusal, and
fingerprints all real .local/saves and .local/config files before/after. An
isolated roster is changed, committed and reopened to test the C++ ratings
adapter. Never pass a real save as a verification destination.

The optional -OriginalRanks argument accepts a private independent historical
fixture with observed_original_scores and observed_original_ranks, each29x5.
This session's fixture is
.local/verification/team_select/audit_c/rank_comparison.json.
Absent original evidence prints PENDING, not PASS. Native output is never
automatically adopted as an expected retail fixture.

There are98 capture checkpoints in config/decomp/team_select_scenarios.json.
Two fresh processes must reproduce identical state and512x240 PPM frames.
Localized logo/tint/Help changes, Help sound events (including a changed held
key during growth), last-random wait, Setup routes, invalid input and preserved
selection are independently checked. These are handler tests, not physical
keyboard delivery or original pixel equivalence.

Cursor-audio checkpoint validation on2026-08-30:45/45 CTest tests pass in Debug
and RelWithDebInfo. All98 captures repeat within/across configurations; four
additional hand-seeded Circle dispatch receipts match original source state.
Create Player retains27/27 scenarios and its numerical model/packet/texture
checks. Cursor scalar pitch/gain and shared cue RNG consumption are corrected;
speech retains its prior behavior. See cursor_audio_workflow.md for the bounded
contract, original-code comparisons, private runs and remaining SPU/history gaps.

Prior presentation checkpoint validation on2026-08-30:44/44 CTest tests pass in Debug
and RelWithDebInfo. All98 repeated state/frame scenarios and owned snapshots
match across configurations. Create Player retains27/27 repeated captures and
the established numerical model/packet/texture checks. Real saves/configuration
remain unchanged; instruction credit is unchanged. Team Select now retains each
completed frame before callback/random/Help input mutations, including terminal
Help growth without text. See team_select_presentation_workflow.md for source
owners, private run identifiers,33726 source assertions and15 extracted-host
phase cases. Physical runtime, text/arrow history and audio remain separate gaps.

Prior placement checkpoint validation on2026-08-30:43/43 CTest tests pass in both Debug
and RelWithDebInfo, including418 new session assertions. All88 state/frame
scenarios and owned snapshots repeat within each configuration and across them.
Create Player retains27/27 repeated captures and its existing numerical model,
packet and texture checks. Real saves/configuration remain unchanged. Private
runs: Team Select Debug20260830-184704-31dfb735, release20260830-184841-271680d9,
Create Player20260830-184756; logs use.local/logs/user_placement_*. Independent
placement/tint source checks pass11,895 assertions and the extracted host passes
seven save/delete/modal paths. See user_setup_placement_workflow.md for limits.

Earlier input checkpoint validation on2026-08-30:42/42 CTest tests pass in Debug and
RelWithDebInfo. All76 state/frame scenarios and owned snapshot artifacts repeat
within each configuration and match across them. Create Player retains27/27
deterministic scenarios,753/753 projected vertices,251/251 primary packet/order
records and zero missing sampled texels. Historical145-score/145-rank and
existing metadata checks pass with unchanged credit. Real saves/configuration
remain byte-identical with unchanged timestamps. Release and the desktop
shortcut are refreshed. New presentation boundaries intentionally change prior
Team Select frame phases; those older hashes are not a retail oracle.

Private input evidence: Team Select Debug run20260830-181950-752b1d20, release
run20260830-182053-2eafaf77; Create Player run20260830-182038. Logs use
.local/logs/team_poll_final_*. Source comparisons pass55,588 pure-C assertions,
7,985 barrier assertions and60 extracted-host/Session assertions; the independent
verifier accepts one valid capture and rejects57 mutations. See
frontend_input_workflow.md for their scope and full caller denominators.

Prior editor checkpoint:39/39 CTest tests passed in Debug and RelWithDebInfo;
57/57 combined scenarios repeat in each configuration and match across them.
Create Player retains27/27 repeated captures,753/753 projected vertices,
251/251 primary packet/order records and zero missing sampled texels.
The historical145-score/145-rank comparison and existing metadata checks pass;
no instruction credit is added. Real save/config bytes and timestamps remain
unchanged. The release executable and desktop shortcut are refreshed.

Private final evidence: Team Select Debug run20260830-172347-1b14e5e9 and release
run20260830-172548-1380c64b; Create Player run20260830-172343. Final build/test logs
use .local/logs/team_editor_final_*. Editor source-MIPS and independent profile
wire/concurrency probes are described in user_setup_workflow.md. User-requested
checkpoint commits contain public implementation/tests/docs only.

Prior Team Select checkpoint:36/36 CTest tests passed in Debug and RelWithDebInfo;
21/21 Team Select scenarios reproduced in both configurations;27/27 Create Player
captures still pass. Existing instruction-semantics, recovery and progress
metadata checks pass without changing their credited totals. The release
executable and desktop shortcut were rebuilt/refreshed. No commit or push was
made, and no private file was staged/tracked.

Private evidence: Team Select Debug run20260830-162931-bb6cb45f and release
run20260830-163210-5391aa66; Create Player run20260830-163047. Logs are in
.local/logs/team_select_*. The independent C++ saved-roster adapter comparison
also matches145/145 scores and145/145 ranks after the Chicago0/8 swap; exactly
four scores and two ranks change. The public verifier checks cache change and
restart retention; that exact altered-cache oracle comparison is separate
private audit evidence, not original modified-roster runtime equivalence.

## Evidence tiers and remaining work

| Tier | Evidence / limit |
|---|---|
| Source structure |14 Team Select owners,637 instructions; separate34 Setup callback instructions,2596 targeted shared-helper instructions,524 rating-helper instructions |
| Instruction accounting |0 credited here: per-block semantic annotation/review remains pending; denominators are retained in team_select.json |
| Native state tests |Both sides, all31 teams, six criteria, both directions, boundaries, ignored tokens, exit persistence, RNG carry and random barrier; signed rating arithmetic/ties/saturation |
| Host integration |98 checkpoints; completed Team Select frames, native polling, held input/chords/exits, debounced topology and retained placement, editor/modals, save retry/restart and owned partial snapshots; gameplay pending |
| Historical original numerical boundary |145/145 stock scores and145/145 ranks agree with cached original FEONLY RAM; not a live Team Select capture |
| Modified roster arithmetic |Independent oracle checks reorder, cross-team swap and count-changing transfer; no original modified-roster runtime claim |
| Native rendering |Two-run equality; regression stability only |
| Original state3 runtime/visual/timing/audio |Pending synchronized observations |
| Physical walkthrough |Pending; no inference from handler calls |

The independent private C audit also exercised2,976 navigation transitions,
1,922 exit pairs,10,002 RNG vectors and128 rating scenarios. These counts
describe bounded tests, not a completion percentage.

A later private candidate map covers all14 owners/70 original basic blocks/
637 instructions with source hashes and test-assertion references. Candidates
retain descriptor/list/poll/ABI gaps; they have not been promoted to reviewed
instruction credit or original execution equivalence.

Random owner8004F934 selects12 accepted candidates and waits1..12 presentations,
including the final12. Caller8003D930 adds5; poller8003AE4C pumps at least1 before
the next sample:84 minimum presentations excluding pending text work. No cancel
is checked inside. Rejected RNG values consume no presentations; repeated teams
are allowed. Native scheduling uses the nominal1001/30ms presentation cadence
and does not skip unpainted states after a stall. Actual timing, seed history
and frame alignment remain pending. Title uses the separate shared16-bit RNG.

The bounded physical-pad poll/repeat/chord path, postwaits, both Team Select
exit barriers, exact Setup Start history, User Cancel barrier, and topology
debounce are now implemented. See frontend_input_workflow.md and its separate
full-caller ledger. Key messages update held masks before fade/repeat guards;
they no longer directly dispatch Team Select actions. Complete state0 polling,
general queued text movement, directional-arrow flash/history and physical topology
presentation timing remain pending. The bounded state5 placement targets and
same-row modal return are implemented; see user_setup_placement_workflow.md.
Team Select presentation ownership and Help geometry/input phases are now
source-compared; see team_select_presentation_workflow.md. Source Help open/close
selects sounds7/8. Cursor bank/program/tone/compressed sample intervals and the
bounded pitch/gain scalars are source-compared. Accepted native cues now advance
the shared six-word RNG before device submission and Circle candidate generation.
The static seed loads at frontend bootstrap and is not reset on Team Select
entry. Source voice-allocation outcomes, complete prior history, SPU waveform
and live event-time comparison remain pending; see cursor_audio_workflow.md.

## First-mismatch notes

1. Entry producer: previous port routed focused Setup cards to profile CRUD.
   Original Start80 at8003F7C8/mode0 pushes state3/8004FCD8 regardless of card
   focus. Patch separates Start continuation from Cross/Square option changes.
2. Rank producer: static metadata ranks disagree with cached original runtime
   ranks (only8/145 static rank values agree). Original8005DB34 recomputes from
   resolved players. A separate derived cache now agrees145/145 without changing
   immutable save identity; no downstream text compensation was applied.
3. Deterministic capture: Setup portrait card indices differed between runs,
   before any Team Select rendering. loadMenuCards excluded the new capture
   flag from its existing deterministic seed path. Adding that flag fixes only
   the capture boundary; normal launches retain current random behavior.

## Original capture needed next

The emulator was not running during the implementation checkpoint. The later
runtime preparation launched the existing reference installation with the local
disc. A fresh FEONLY code signature found one2MiB backing; eight full independent
functions matched14,764 source bytes. This initial dump precedes a live selector
presentation: context800214F0, shared history0/0/0, teams3/24. It is not a state3
runtime comparison. The title later entered its automatic gameplay demo without
game-key injection, replacing FEONLY; the FE probe correctly refused that state.

Reach Game Setup with ONE physical input at a time, then verify before continuing;
no$psx game-key injection remains unverified. Initial preparation artifacts are
under .local/verification/team_select/runtime-20260830/. The title screenshot
has no paired title RAM dump: the attempted later dump encountered the overlay
change and was refused, not accepted as a synchronized pair.

First probes: break8004FCD8 and the state3 invocation of8003D930. Dump resident
80021D70 length0x20, remembered descriptor800EDCA4[3], and current frontend
context pointer800170C0; inspect its +0x34 controller pointer and descriptor byte.
After one Left, break8004EF40 and compare home/away fields, selected descriptor,
rank table and palette target. Use8004FC80 for exit/remembered regular fields.
Dump only through a newly validated process-memory signature; do not assume
Create Player's old signature or an ASLR host address remains valid.

Synchronize512x240 viewport, selected criterion, title/tint/palette phase and
native scaling before side-by-side frames. Record actual values first; if a
variable differs, trace its producing callback before touching the renderer.
The next implementation dependency is documented in gameplay_first_path.md.

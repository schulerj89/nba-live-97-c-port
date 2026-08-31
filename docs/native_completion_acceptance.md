# Full native completion acceptance

Active reconstruction started 2026-08-31 from `a58f35f`. This is an open
acceptance ledger, not a completion percentage. The native executable currently
stops at an owned partial match handoff; it cannot play a basketball possession.
Passing frontend tests does not satisfy any complete-match gate below.

Source-confirmed original bugs and quirks must be preserved and commented, per
the user. Comments identify the original owner, observable consequence and
evidence. Native memory/I/O safety refusals must be identified separately; they
must not silently replace source behavior with defaults or corrected rules.

## Acceptance matrix

| Gate | Native state at baseline | Evidence still required |
|---|---|---|
| Complete executable/feature/resource inventory | Boot and FEONLY inventories only; gameplay research is separate | Audit every executable/overlay and reachable mode/resource; retain full owners and unresolved branches |
| Boot, legal, movie, title, Setup | Implemented with scoped checks | Original transition, presentation and audio comparisons; repeated fresh/warm entry |
| Team Select and User Setup | Implemented bounded frontend path | Complete original input/timing/visual comparison, all controller/profile variants |
| Accepted current rosters/settings/controls | Owned ordinary snapshot;655B0 and65328 effects before period setup | Close extension/reference fields, created membership, special teams, period/entity bindings and selected-player producer |
| Gameplay assets and scene | Motion resolver implemented/source-compared; scene absent | Native motion sampling/model/court/camera lifetime and source numerical/frame comparison across teams/courts |
| Tipoff through first possession | Not implemented | Original phase/input trace and native deterministic possession with no substituted logic |
| Player/ball simulation and CPU AI | Not implemented | Movement, animation, contact, ball ownership, passes/shots and CPU decisions against original owners/state |
| CPU versus CPU full match | Not implemented | Full actual matches, regulation/overtime/end/results/return without human intervention or scripted outcome |
| CPU versus user full match | Not implemented | Host input route controls movement, selection, passing, shooting and all recovered actions for a full match |
| Rules and events | Frontend rule settings only | Scoring, clocks, fouls/free throws, violations, inbound changes, timeouts, substitutions and period/end logic |
| Pause, resume, exit and next match | Not implemented | Repeated lifecycle tests with preserved state and no stale scene/audio callbacks |
| Results/statistics and persistence | Existing frontend stores; no match results | Recovered stats, profile and mode writeback; isolated save/reload and failure tests |
| Season/playoff and all original modes | Not implemented | Original mode inventory, setup/schedules/progression/save/load and completion paths |
| Roster transactions and Create Player | Existing bounded frontend implementations | Preserve current editor/model checks; close insertion/resolution/ranking, special teams and injuries |
| All menu/pause music | Portable five-track decoding and source routing core; host still loops menu1 | Original backend/caller lifecycle, five-track playback/capture; no guessed shuffle |
| XA music/media | Original containers inventoried only | Channel/index/selection rules, every reachable logical track, synchronization and decode/playback receipts |
| Gameplay SFX/crowd/announcer | Not implemented | Source events, banks/indexes, voice allocation/overlap/gain/pitch/timing and captured listening checks |
| Frontend sound and speech | Bounded scalar/decoder checks | Preserve shared RNG and existing regressions; recover remaining allocation/SPU/runtime behavior |
| Fidelity and original bugs | Scoped source semantics and frontend captures | Original comparisons by correct overlay and state; documented preserved quirks throughout runtime |
| Stability/performance | Frontend only | Repeated full matches, memory/resource stability, timing, input continuity and unmodified real saves |
| Final independent review | Not started | Try to disprove completion against full inventory; no relevant unknown/pending/placeholder paths |

## Dependency order

1. Rebuild both configurations and repeat existing frontend/save guards. Prove
   supported autonomous debugger navigation and breakpoints with fresh readback.
2. Preserve the source entry order: resource resolution, both `655B0` calls,
   `65328`, `65DB0` (including lineup/player/controller dependencies), both
   `65820` calls and `646A8`. Implement portable C owners and native owned data.
3. Integrate original scene/resource lifetimes, phase/tipoff transitions,
   simulation, CPU AI and gameplay inputs through a real first possession.
4. Continue through rules/events, complete matches/results/return, all modes,
   created/special rosters and persistence. Music/audio recovery runs alongside
   the gameplay critical path and must join its actual event/transition owners.
5. Exercise all matrix gates independently and keep tested pushed checkpoints.
   First possession or exhibition-only completion cannot close the full goal.

## Evidence rules

Record original/native hashes, scenario inputs, overlay, PC/phase, seeds,
expected differences and artifacts. Source execution, original runtime, native
unit checks, host integration and complete-match acceptance are distinct tiers.
Original bytes/assets/dumps/recompilations and raw fixtures stay ignored in
`.local/`. Public tests use invented data. Save verification uses isolated
stores and fingerprints real stores before/after. Never refresh expectations
only to conceal a mismatch. Native semantic parity does not mean instruction
identical PS1 output.

Fresh startup builds pass 52/52 CTests in Debug and RelWithDebInfo. Logs are
private under `.local/verification/native_completion/baseline-20260831/`.
These tests establish the starting checkpoint, not gameplay completion.

The first continuation checkpoint passes57/57 CTests in both configurations.
Team Select reproduces98/98 frames plus264 arrow frames in each configuration;
Create Player reproduces27/27 scenarios with753 matched packet coordinates.
Real Team Select save/config bytes and timestamps remain unchanged. Fresh logs
are under `.local/verification/native_completion/checkpoint1/`. Controller
initialization now joins the accepted snapshot. Selection, motion and music
cores have separate source-instruction evidence and are not yet a gameplay or
multi-track host playback claim. Supported debugger address entry remains
blocked; no fresh live original gameplay capture was manufactured or credited.

The next checkpoint adds646A8 direct binding effects, including its observable
divide-trap prefix, and65140/65070 lineup recovery with synchronous substitution
boundaries. Both builds pass59/59 CTests. Tail helpers/substitution dependencies
and host integration remain open; this does not advance the complete-match gate.

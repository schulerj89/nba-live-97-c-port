# Persistent team-strategy snapshot

2026-08-30. The native ordinary-match snapshot now owns fourteen persistent
team-setting bytes, initialized once for a fresh native session. Their later
gameplay projection and exit writeback have separate portable C helpers. This
does not initialize a complete gameplay header or launch a game.

## Source fields and lifetime

All offsets below are hexadecimal. The fields retain opaque names until their
individual strategy meanings are established.

| Index | Resident home / away | Team-header offset | Cold value |
|---:|---|---|---:|
| 0 | `80021DE6` / `80021DE7` | `76` | 1 |
| 1 | `80021DE8` / `80021DE9` | `77` | 1 |
| 2 | `80021DEA` / `80021DEB` | `78` | 0 |
| 3 | `80021DEC` / `80021DED` | `38` | 7 |
| 4 | `80021DEE` / `80021DEF` | `39` | 5 |
| 5 | `80021DF0` / `80021DF1` | `36` | 0 |
| 6 | `80021DF2` / `80021DF3` | `37` | 0 |

FEONLY `80035D80` loads the full 32-bit initialized word at `80021EE4`.
Any nonzero word skips the cold stores; this is not a low-byte flag test.
The initializer does not set that word itself. Main `80028800` calls it at
`800289F4`; later `800360D4` writes one at `800360E0` before entering the
frontend dispatcher. Thus cold values are not a per-confirmation default.

GAMEONLY `80065820` consumes the pair selected by unsigned team-header
halfword `+14 != 0`, with zero meaning home and any nonzero value meaning away.
It preserves prior fields under two branches:

- Nonzero launch-control halfword `8001EDEC`: set only field 0 to one.
- Ordinary launch with zero human-count halfword `+42`: set fields 0/1 to one.
- Ordinary launch with nonzero humans: copy all seven resident bytes.

The native helper takes an in/out seven-field record. It must not replace a
CPU or nonzero-launch record with zeroes. Injury slots below twelve refuse
before mutation because the owner's subsequent injury/substitution work is
outside this helper. All supported slots twelve through 255 skip that branch.
The current ordinary snapshot retains the existing `FF/FF` injury invariant.

On exit, GAMEONLY `80067930` copies all seven fields from each team header back
to the resident pairs when launch control is zero, including CPU teams. A
nonzero launch preserves all fourteen bytes. Its preceding statistics and
resource cleanup are separate obligations. The game-loop caller invokes this
owner at `800691BC`; launch `62` follows a reinitialization path instead.

## Native ownership and safety

`recovered/match_strategy.c` owns the exact byte branches. The structs group
seven bytes per side rather than reproducing the source's interleaved memory.
Null and unsupported-injury refusals are native guards, leave outputs unchanged,
and do not claim source failure behavior. Local copies preserve input/output
overlap. No RNG, profile lookup, roster change, UI policy or file I/O occurs.

`MatchSession::initializeFresh` prepares cold strategy and control maps locally
before publishing either. The host calls it once before its first User Setup,
because no earlier native strategy consumer exists. This explicitly adopts a
fresh native epoch; it is not a claim about exact original initialization time.
Repeated identical initialization preserves warm or invalidated strategy;
changed control defaults refuse as before.

The session tracks whole-group knownness. `buildMatchSnapshot` refuses unknown
strategy before consuming its copied RNG, then copies known fields into the
owned result. Successful preparation clears only `MatchStrategyFields` for
this group. `MatchExtensionSettings` stays set: other extension bytes and
their producers are not filled with guessed defaults. Special/created-player
membership and all earlier snapshot guards are unchanged.

The field-only `writebackStrategy` adapter requires the current accepted
snapshot revision and an explicit live launch-control halfword. The source
rereads that flag on exit; assuming the accepted snapshot value would miss a
later change. A nonzero live flag preserves values and knownness. The adapter
changes only the live strategy group and its native provenance receipt. The
old snapshot, capture revision, controller maps and shared RNG remain unchanged.
A complete ordinary writeback can restore knownness after invalidation;
repeated cold initialization cannot. Revision validation is native protection
against delayed stale results, not proof that gameplay executed. Repeated full
writeback for the same current revision is allowed.

There is no current host gameplay-return call to this adapter. Tests supply
explicit synthetic post-gameplay field records. A future gameplay adapter must
reach the original writeback boundary with actual mutated header fields and
honor the preceding cleanup and supply the actual live launch flag. The initial
snapshot is not such a result.

The existing accepted frontend order remains controls `61674`, presentation
`46D24`, rules `3E7A8`; strategy capture adds no RNG draw. The confirmation cue
still precedes presentation selection. Native preparation/allocation failure
must preserve the existing snapshot, controls, strategy and shared RNG.

## Evidence and full-owner inventory

`config/decomp/match_strategy.json` retains all containing denominators with
zero new instruction credit. These are bounded dependencies, not a game-wide
completion measure.

| Owner | Accounted / full instructions | Scope and remaining uncertainty |
|---|---:|---|
| FE `35D80..36008` | 0 / 162 | Cold guard and fourteen stores; other settings/descriptor initialization separate |
| FE `360D4..3610C` | 0 / 14 | Initialized-word lifetime; wrapper/global behavior not ported here |
| GAME `65820..659F0` | 0 / 116 | Seven-field projection; 73 source instructions visited under no-injury boundary |
| GAME `67930..67A60` | 0 / 76 | Final fourteen-byte copy; source tests hook statistics/cleanup |
| GAME `68BF8..691F8` | 0 / 384 | Exit caller inventory only; gameplay loop unimplemented |

Previously inventoried FE callers `28800` (227) and `3F7C8` (1173) are
references only. GAME team initialization, period setup, statistics and
resource cleanup retain their separate unresolved scope.

- Public core test: 526,391 checks, including unsigned branch domains, supported
  injury sentinels, preservation, cold/warm guards, overlap and atomic refusal.
- B current compiled C versus original GAME instructions: 3,840 cases and
  11,523 assertions; no mismatch. Statistics `83100` and cleanup `670B0` are
  explicit hooks; the no-op `79758` executes.
- A independent current-C comparison: 66,719 checks across 987 cold cases,
  14,240 projection cases, 1,792 writebacks, 257 overlap cases and 54 refusals.
  Its separate original cold/lifetime audit passes 26,362 checks over 6,580
  initialized-word patterns and ten wrapper/caller fragments.
- Snapshot tests cover owned cold and warm values, stale-revision refusal,
  unchanged prior publication/controls/RNG, unknown-state refusal, repeated
  initialization and full writeback restoring knownness.
- Four native accepted snapshots retain the same known fresh group across
  profile changes, cancellation, unsupported-team refusal and a modified roster.
  This is frontend integration, not original gameplay-return evidence.
- An independent extraction of the current session methods passes 37
  publication/ownership checks with controlled preparation and allocation
  failures, including warmed state. Writeback and repeated initialization
  allocate nothing; the controlled builder is not an original-source oracle.
- The current public verifier rejects 128 paired corruptions per build, in
  Debug and release. Both copies receive the same mutation, exercising field,
  type, extent, knownness, provenance and pending-bit guards independently of
  the nondeterminism check.

Debug and RelWithDebInfo each pass 50/50 CTest tests. All 98 Team Select/User
Setup frames and 264 arrow-flash frames repeat within and across builds; they
also remain unchanged from the preceding plate checkpoint. The four snapshots'
prior fields, including RNG, controls, rosters and rules, remain unchanged;
only generated profile identity is normalized for the separate process runs.
The new strategy group is verified independently against its cold source values
and initializes exactly once in each host trace.

Create Player retains 27/27 repeated scenarios, 753 projected vertices, 251
primary packet/order records and zero missing sampled texels. Historical ranks
still match 145/145 scores and 145/145 ranks. Real save/configuration bytes and
timestamps are unchanged. Existing global instruction/recovery totals are
unchanged, and the release desktop shortcut is refreshed.

Final private Team Select runs: Debug `run-20260830-212233-33e452ce`, release
`run-20260830-212255-8e5e4718`. Create Player: `run-20260830-212224`. Logs use
`.local/logs/match_strategy_*`.

Private receipts are under
`.local/verification/gameplay/audit_b/next_strategy_snapshot/` and
`.local/verification/team_select/audit_a/strategy_snapshot/`. Original bytes,
source executors and snapshots remain ignored. Original live runtime, imported
warm/card settings, complete team headers, other extensions and gameplay remain
pending. No save format or Create Player model implementation changes.

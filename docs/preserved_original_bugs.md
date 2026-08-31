# Preserved original behavior

User instruction: keep bugs from the original game and comment them. Source
comments identify the original owner and observable behavior; this index makes
the confirmed cases easier to find. A surprising behavior is not automatically
a bug, so intentional-looking quirks remain described as such. Native port
defects should still be fixed.

| Original owner | Preserved behavior | Code and evidence |
|---|---|---|
|GAME65328 |Joined controllers retain their old selected-player halfword, including invalid values |`game_controllers.c`, controller tests and workflow |
|GAME653E8 |An existing player claim does not repair the controller's stale selected word |`game_controller_selection.c`, selection tests and workflow |
|GAME653E8 |No qualifying candidate reuses stale/incoming s6 and can select the wrong side or overwrite a claim |Same owner; incoming register provenance remains explicit |
|GAME7066C |Wrapped negation/shifts and signed comparisons retain INT_MIN/overflow results |Selection distance helper; full32-bit original comparisons |
|GAME65140 |An exhausted player in lineup position0 does not recover; later recovery counts increment even without a swap |`game_lineup_recovery.c`, recovery tests and workflow |
|GAME65140 |Status arithmetic saturates by the low16 sign bit, allowing wide sums to wrap to small positive values |Same owner; signed elapsed and overflow cases |
|GAME646A8 |Duplicate lineup entries use the last inverse-map write; entity subtraction retains raw underflow |`game_player_bindings.c`, bindings tests and workflow |
|GAME646A8 |Player byte+9 zero traps after earlier binding/entity writes; no reciprocal or later tail calls |Exact DIVIDE_TRAP prefix in bindings owner and original oracle |
|GAME6459C/644FC |All-zero ratings retain carried register values, including a prior sort score as a player ID |`game_team_roles.c`, complete source tail-chain comparison |
|GAME64388 |Swaps do not advance the sort cursor; the result can remain incompletely sorted |Roles helper; exact source saved-return sentinel comparison retained |
|GAME56FFC force1 path |An in-range old frame/time can survive forced reset; primary mode2 copies secondary values without clamping |`game_player_initialization.c`, initializer/reset tests and workflow |
|GAME65DB0 lookup |A raw option outside the ordinary five still reads its original adjacent word |Private256-word lookup windows in `GameplaySetup`; no default duration |
|GAME60EF8 |Already sorted entities keep stale render indices; only actual insertion moves update the index halfword |`game_period_dependencies.c`, exact ordered sort writes and tests |
|GAME63EDC/51ED8 |Raw rating byte1E values23,24,25 or a zero FDB64 divisor trap after the height and attribute3A writes; raw entity IDs can alias height slots through the original shift wrap |`game_player_attributes.c`, exact trap prefixes and raw-ID source comparisons; no rating clamp |
|GAME56CE0/572C0 |Full motion queues silently drop; channels can diverge; producer searches exactFFFF while consumer treats any negative head as empty |Animation queue/advance owners and whole-source comparisons |
|GAME572C0 |Transitions discard remaining time; old primary header mode controls synchronization; blend promotion loses unrelated conversion bits |`game_animation_advance.c`, directed source and actual-resource replays |
|GAME57B18 |Foot-lock counter overflow into8000..FFFF reenters the early cache branch; unused B fields stay stale |Pose request owner and composed foot-lock tests |
|GAME55018 |Asymmetric Euler adjustment changes A even at weight0; full unsigned weights and unusual distance expressions remain |Gameplay pose sampler and raw-angle original comparisons |
|GAME6CFE0 |Landing marker comparison reads the just-copied unchanged height, so its2C=FF store is unreachable; actor12 can clearA0 without recalculating9C |Physics owner, ordered-store proof and source invariant |
|GAME706E4 |Wrapped INT_MIN negation can cause a source division trap or adjacent-table read |Direction helper; no square-root/atan2 replacement or index clamp |
|FE2F36C/6B6A0 |Router retains a zero-status branch although the original BUSY helper cannot return zero |`music_routing.c`, source-quirk comment and lifecycle tests |
|FE2F330 |The sixteen-slot music table permits repeats and includes the special track in ordinary selection |Music extractor/routing workflow; no guessed shuffle |
|FE7B2BC/7A81C |A long fade can compute step0 and stall; envelope countdownFFFFFFFF decrements and short stages can divide by zero |`music_voice.c`, source service/fade comparisons and workflow |
|FE72254/72954 |Only full1024-byte/channel staging blocks are submitted; all five tracks discard their partial final block |`music_stream.c`, all original track chunk comparisons and workflow |
|FE7309C |Natural drain compares signed read index with write index minus one without modulo correction; ending at write index0 can miss key-off |Stream stop/advance helpers and full original IRQ comparisons |
|FE31A88 music arm |Zero adjacent volume skips reduction; repeated View Player entry overwrites saved volume; exit restores the saved low byte even after intervening edits |`music_transition.c`, exact source state/callback comparisons; no extra gain call to cancel the original fade |
|FE3122C/8ABF0 |Already-finished nonnegative announcer voice keeps its old value; CRCF validation ignores the trailer's length field |`frontend_resource.c`, actual lifetime/content comparisons; native callback obligations remain explicit |

This is a growing index, not proof every original bug has been found. Callers
and host adapters must preserve these outcomes when integrating the recovered
owners; unit-level preservation alone is not full-game runtime verification.

Unknown original state must not become a guessed zero or default player.
Original out-of-array reads are a separate native storage boundary: refuse
explicitly when the required owned state is unavailable, without claiming the
original would have refused or succeeded. A source divide trap is different:
its verified effects before the trap are retained. See each owner for whether
a native refusal is atomic or retains a completed prefix.

Port-language undefined behavior is not an original PS1 bug. For example,
replacing C++ negative-left-shift expressions in the SCHl decoder with defined
arithmetic preserves the existing PCM output; it does not authorize changing
the predictor rounding to match another decoder.

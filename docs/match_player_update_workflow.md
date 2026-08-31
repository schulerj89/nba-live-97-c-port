# Native player update composition

`updateMatchRuntimePlayers` composes the recovered `6801C` coordinator with
actual animation `579FC` and physics `6CFE0`/`706E4`. It is a bounded simulation
owner, not a gameplay input loop, ball update or playable possession.

The coordinator reads ten entries from the live entity pointer table. It does
not walk ten consecutive physical records. Duplicate entries update the same
record repeatedly; an entry can refer to the represented ball slot. Each
callback uses the entity identity captured by the original saved register.
The current-entity global is published before each update, and the team
context changes at entry and at slot5 in original order. Physics uses this
actual team reference, never a side inferred from an entity field.

The immutable `GameplayAnimation` resource also owns the257 direction bytes
at GAME D72B4 and two eight-byte signed boundary tables at B8A54/B8A5C. These
already lie within the existing private animation pack. Their views share its
retained lifetime and require the same setup generation as the match.
Direction calculation is the recovered integer helper, not atan2 or sqrt.

Physics shares the existing period scalars, simulation tick and FDBD4 auxiliary
field. FE910 and rule21D8F are explicit entry values, unknown by default.
The accepted rule maps to `rules[8]`; its intervening loader provenance still
belongs to the entry owner, not this simulation update. The current/team reference fields likewise start
unknown until their actual coordinator stores.

Only the mutable records and globals consumed by these owners are staged.
Accepted rosters and immutable resources are retained without copying the
roster vectors each tick. Direct coordinator and physics store receipts drive
publication into the candidate. Animation applies its own exact write masks.
This preserves untouched bytes, including partly known fields that would be
lost by importing an entire typed view.

Confirmed original bugs remain in the composed path and are covered by
regression cases: near-negative angle snapping keeps a large positive modular
delta for EA, the landing marker compares the just-copied previous height and
never reaches its intended FF write, and actor12 can clear A0 while retaining
the earlier9C movement value. See [the behavior index](preserved_original_bugs.md).
Port defects are not covered by the preservation rule and must still be fixed.

The four rule/audio callbacks reached by physics are not yet composed. A
reached call returns pending with its exact callsite, argument and preceding
receipt; it never acts as a successful no-op. A failed or pending update does
not publish the staged live state. This native transaction is an ownership
boundary, not a claim that the original PS1 game rolled back a partial update
or recovered from a divide trap. Receipts are diagnostics, not resumable
continuations. No external callback effects occur in this adapter yet.

Public tests exercise actual animation and physics, table aliases, original
angle/landing/stale-speed bugs, resource byte views, partial knownness,
unknown/unowned entries and the first real pending rule/audio boundary.
Independent private review ran400 random full updates and300 actual-resource
trajectory updates per build against original6801C/579FC/6CFE0 and reached
callees, without successful callee stubs. Each build's random cases compared
1,825,600 known record bytes and34,045 exact coordinator store/callback events;
six directed numeric cases and15 contract diagnostics also pass.

That review found and verified a fix for a native provenance-validation defect:
`MatchRuntimeRecord::read` now validates every byte of a field before returning
unknown. It cannot hide malformed later bytes behind an unknown first byte.
Both invalid-knownness and nonzero-unknown-payload cases refuse publication.
This was a port defect, not preserved original behavior. Private evidence is
under `player_update/root_review`. Neither synthetic state nor this source
comparison proves natural entry or a full match.

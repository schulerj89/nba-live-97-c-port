# Period lineup recovery

`recovered/game_lineup_recovery.c` implements GAME65140 (122 instructions) and
GAME65070 (52 instructions). The latter invokes649D8 synchronously through an
explicit callback. A callback request alone is not a completed substitution.
The native host does not yet invoke these owners or run a period.

## State and source order

The owned state holds each team's12-entry lineup and inverse map, five saved
preferred slots, recovery counter, human count and automatic-substitution byte.
It also holds24 signed status halfwords and the FDB8E/FDB54 scalar fields.
These are semantic fields, not an emulated memory address space. Team0/1
identities map to original header side words0/5; status bases are0/12.

65140 processes statuses0..23 in order. Nonnegative values other than7FFF add
23 times the elapsed argument with original32-bit wrapping; a negative low16
result saturates to7FFF. Other negative values remain unchanged exceptFFFF on
the elapsed>=120 branch. That branch searches the current team's lineup,
scans backward past negative-status entries and may swap two lineup slots.
It increments header+66 and writes7332 for the recovered player.

After that scan, elapsed>=120 writes marker11 and invokes65070 for home, then
away, where human count is zero or unsigned automatic byte is nonzero. The
away condition is read after the complete home callback chain.

65070 writes substitution lock1, visits five preferred positions in order,
and rereads their current inverse map. An inverse index>4 and status>=7332
calls649D8(team, active position, inverse index,0, first). The first flag is1
until the first call, then0; each team's invocation starts again at1. It clears
the lock only after all five positions finish. Do not save a list of decisions
and perform them later:649D8 calls646A8, which changes the inverse map consumed
by subsequent iterations.

## Preserved bugs and native boundaries

- AFFFF status in lineup position0 remainsFFFF, even on a recovery interval.
- The backward scan has no lower bound in original code. An exhausted prefix
  can read before the lineup. Native code reports OUTSIDE_STORAGE at the first
  unowned read; it does not invent a valid earlier player or report success.
- A recovery increments+66 even when no swap is needed. Its counter wraps.
- Addition tests the low16 sign bit rather than doing a wide saturating sum.
  Large or negative elapsed values can wrap into small positive statuses.
- Signed lineup values are added to the global status base before checking
  storage. For example away lineup-1 can read home status11. No per-team clamp
  hides that source behavior.
- 65070 passes every signed inverse index>4 to the callback, including indices
  beyond11. Any subsequent unowned access belongs to the649D8 adapter, rather
  than being silently repaired by65070.

Argument errors have no effects. Reached storage or callback failures retain
the completed prefix, including lock1 at an unfinished65070 call. They are
explicit native boundaries, not original successful returns. A caller cannot
clear the lock, continue the period or retry the whole routine as if no effects
occurred. A higher-level owned transaction can keep incomplete state private.
The callback must perform the actual callee and return success before source
execution continues. Missing callbacks are only refused when actually needed.

## Verification and integration

Public synthetic tests cover sentinels, signed arithmetic, overflow, recovery
position quirks, cross-team reads, exact callback arguments, first-call flags,
callback mutations and explicit unfinished states. They pass MSVC/W4/WX in
both debug and optimized standalone builds.

Private `.local/verification/native_completion/lineup_recovery/verify_lineup.py`
compares compiled C to actual GAME instructions in1703 recovery and1601 direct
automatic-substitution cases, in both builds. All122 and52 owner instructions
are exercised;2689 callback requests match with state at each call. Original
stores stay within the declared fields and stack. The649D8 hook deliberately
mutates later-consumed fields in selected scenarios to verify live read order.
It is an oracle boundary, not verification of649D8 itself or live PS1 gameplay.
An independent review found no defects and matched64 further original cases
with150 cascading callback mutations; its receipt is in
`.local/verification/native_completion/team_roles/lineup_review.json`.

65DB0 calls65140(120) after its scalar resets; the halftime branch also calls
65140(600). Apply earlier655B0 status effects and current lineups first. Its
ordinary pregame substitution path still needs649D8 direct effects,646A8 and
the actual tail helpers. Midgame649D8 additionally has presentation, audio,
timing and rule effects. Do not replace any of those with a guessed swap.

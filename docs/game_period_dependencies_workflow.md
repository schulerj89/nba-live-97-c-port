# Period leaf dependencies

`game_period_dependencies.c/.h` closes the actual remaining leaf owners used by the `65DB0` coordinator: render insertion sort `60EF8`, phase/reset `5828C` with `58260`, and conditional motion switching `56B78` with secondary `56AA4` and primary `5699C`. The two motion setters can also run independently. There are no callback stubs, original instruction tables, runtime memory or asset-loading calls in this native module.

All three APIs return effects atomically and support byte overlap between input and output. They leave output unchanged on failure. Values and references retain the existing explicit provenance types. A successful projection can contain unknown retained or copied fields; apply that provenance, never its zero metadata as a fabricated game value.

## Render ordering

`nba97_game_period_sort_render` consumes the current eleven owned `FDCC0` references, signed raw X words and previous index halfwords. It returns the final table/index state plus every original table and index write in order. One insertion step writes the displaced reference, its index, the moving reference and its index. The maximum is 55 steps and 220 writes.

The source compares signed X and preserves the existing order for ties. Table aliases remain aliases. Only entities participating in an insertion get index writes: an already sorted table retains stale or unknown index values. Rebuilding all indices would change the original behavior. Unknown coordinates of unreferenced entities are irrelevant; reached unknown coordinates or references stop explicitly. Unowned references are native storage errors, not null/default entities.

## Phase/reset helper

`nba97_game_period_reset_phase` accepts the actual raw 16-bit phase. Signed phase below 128 becomes zero, including every negative raw phase. It then performs the actual `58260` writes in order: `FDBD4=0`, `FDBD6=0`, `FDBD0=FFFF`, `FDBD2=FFFF`. It never restores the old phase. The output records four writes, or five when phase is cleared.

The coordinator's callback context must own these four reset fields separately because `65DB0` itself does not read or write them. Copy the resulting phase back into the period state before completing the callback.

## Conditional and standalone motion setters

`nba97_game_period_switch_motion` uses `Nba97GameAnimationState` and the actual normalized `Nba97GameMotionHeaderView` from the player initialization contract. Operation zero is complete conditional `56B78`, operation one is standalone primary `5699C`, and operation two is standalone secondary `56AA4`.

The conditional owner checks signed primary lock `48`, signed secondary lock `4C`, current clip equality and bit one of flags `60`/`64`. If allowed, it calls the secondary setter first and then the primary setter. Each setter independently checks its own lock, clip and flags. A reached setter can therefore return unchanged even after the wrapper calls it. `secondary_called` and `primary_called` record actual native owner entry; ordered write events identify its real effects.

The request remains a full 32-bit word. Original `SLL(request,2)` discards the high two bits before directory lookup, so `header_index` must equal `request & 3FFFFFFF`. Native storage accepts only actual owned indices 0..83 when a lookup is reached. High request bits still affect full-word clip equality and signed `request < 21`; do not replace the request with its stored halfword or directory index. An early return does not demand an unused header. Missing and unresolved reached headers return distinct explicit errors.

The following source behaviors are preserved and commented:

- A setter clears its status bit in `9A`, writes its marker `70`/`78` and accumulator `94`/`96`, then changes the clip. Unknown status remains unknown after the bit clear.
- Header bit zero forces frame and timing to zero. Otherwise, an in-range frame is retained only if the **old** low flag bits are zero. Old bit zero can rewind a valid frame even when the new header would normally retain it.
- For primary header byte `+2 == 2`, synchronization also requires full request equality with secondary clip `4A`. The source copies secondary frame/time after the secondary setter finishes. It does not clamp the copied frame to the primary count, even if the copied frame is out of range.
- A request with high bits can store the same low clip halfword in both channels while failing full-word synchronization equality. Negative raw requests can update default `4E` through the signed comparison even when their aliased directory index is above 20.
- Unknown frames needed for comparisons stop. Count zero proves a rewind for every unsigned frame. Unknown frame/time copied during synchronization remains explicitly unknown rather than becoming a fake zero.

Motion views must come from the actual resource directory and normalized header bytes `+0`, `+2` and `+7`. The period tipoff request is 39, but this implementation does not hardcode clip 39 or manufacture headers. The earlier forced-reset API remains scoped to `56FFC(entity,1)` and is not silently reused for this different path.

## Evidence

Private receipts and separate native builds are under `.local/verification/native_completion/period_dependencies/`. `verify_dependencies.py` directly executes all reached original leaf instructions, including branch delays and transitive setters, with no leaf callee hooks.

| Owner | Original instruction denominator | Unique executed |
| --- | ---: | ---: |
| `60EF8` | 49 | 49 |
| `5828C` | 16 | 16 |
| `58260` | 11 | 11 |
| `56B78` | 42 | 42 |
| `5699C` | 66 | 66 |
| `56AA4` | 53 | 53 |

Each `/Od` and `/O2` build passes 1,000 original sort cases, 261 reset cases, 7,500 synthetic motion cases and 100 cases using actual motion-39 headers from original `640D8` normalization of the real resource. Across both builds, all 195,602 ordered write events and 9,164,800 final bytes, including untouched bytes, match. All 237 source instructions execute. The only resource setup hook supplies the owned resource to `640D8`; the tested leaf owners have no hooks.

Public synthetic tests separately cover all 65,536 raw signed phases, 131,072 frame/count/old-flag combinations, reverse sorting, aliases, stale indices, header short circuits, raw request aliasing, provenance and atomic failure/overlap. Both builds pass `/W4 /WX`.

Original GAMEONLY SHA-256 is `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`; actual mocap SHA-256 is `31ef711fb043c1d8b2ae22c15487af0fb32cf6b1fe86ccec65f50166db5fa559`. Original code, resources and interpreter evidence remain private. These comparisons use explicitly synthetic prior state and do not claim a natural gameplay capture.

## Host integration

Add the source and test in the host-owned build. For period callback `60EF8`, populate actual current render references/X/index state, apply the returned ordered writes and copy the resulting render table and entity indices back. For `5828C`, apply all reset fields and the returned phase. For `56B78`, resolve the callback's actual entity and full request, supply its previous animation state and matching real resource views, then apply only written fields and their known bits. Return callback success only after these effects have been applied to the candidate and shared context. No shared host, build, UI or Git files are changed here.

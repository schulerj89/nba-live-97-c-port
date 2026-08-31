# GAME scoring actor and AI-state workflow

## Scope

`src/recovered/game_scoring_actor_ai.c` is the native owner for GAME
`0x8006E7AC..0x8006ECD8`, the scorer/actor selection and state-mutation call
made by the recovered ball-scoring chain. It also owns the two complete
call-free leaves reached by that function:

- `0x80058AA8..0x80058BC0` — initial scorer/team statistic mutation; and
- `0x8006E734..0x8006E7AC` — scorer and eight-player total propagation.

Those extents total 431 original instructions. Every instruction is bound to
the original `GAMEONLY.BIN` SHA-256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.

The returned value is the original raw selected team/scoring record address.
The owner does not invent native actor objects, translate addresses to host
pointers, or substitute a default scorer.

## Retained memory and service boundaries

Reads and writes use exact original addresses, widths, and source PCs. The
operation budget bounds all visible accesses and calls. Unknown bytes,
alignment failures, callback refusal, and budget exhaustion retain the exact
successful prefix; the entry is not transactional or resumable.

Three unowned synchronous source calls are explicit:

| Call site | Entry | Arguments | Boundary |
| --- | --- | --- | --- |
| `8006E820` | `80056FFC` | selected actor, `1` | gameplay audio/event routing wrapper |
| `8006EA14` | `8007F074` | selected record, scorer index, score mode | actor/UI presentation and downstream platform work |
| `8006EC20` | `8007F20C` | `0`, secondary scorer index | actor/UI presentation and downstream platform work |

A callback success means the real call's synchronous mutations are already
visible through the same memory adapter. There is no success stub. These
boundaries have no return value consumed by `6E7AC`.

## Preserved original behavior and quirks

- Team side is chosen by the sign of the XOR between `8001EE04` and the live
  ball X word, rather than by a normalized sign comparison.
- Five physical actor records have bytes `+DE/+DF` cleared unconditionally at
  stride `F4`; no native count clamp is added.
- `58AA8` tests one statistic against `999` but several later paths increment
  its neighboring halfword. For example, `6EA4C` tests `+8` and increments
  `+A`; the non-mode-1 route tests `+0` before incrementing `+2` and, for mode
  3, tests `+0` again before incrementing `+6`.
- The associated-player updates in `58AA8` and the assisted-scorer updates in
  `6E7AC` are not saturated. Their 16-bit wraparound is retained.
- `6E734` updates the primary actor's `+1A` first, then scans exactly eight
  live player pointers. It updates `+1C` only for active, non-primary players
  whose identity differs from the scoring identity.
- The current scoreboard pointer at `800FDC40` is reread before publishing the
  opposing signed score margin. The second read is not folded into the first.
- All byte and halfword additions retain original truncation, including the
  actor `+DE` byte counter and negative reciprocal score margin.

## Verification

The strict public test executable passes 362 checks in each of:

- MSVC Debug `/Od /W4 /WX /sdl`;
- MSVC Release `/O2 /W4 /WX /sdl`;
- GCC Debug `-O0 -Wall -Wextra -Werror` with UBSan; and
- GCC Release `-O2 -Wall -Wextra -Werror` with UBSan.

The private original-CPU comparison passes 3,541 cases in both MSVC
configurations. Each configuration compares 91,914 ordered stores, 185,360
ordered reads, 729 labeled service calls, 2,694 refusal/unknown prefixes,
final retained RAM, return values, and 906,990 original instructions. The
combined directed and prefix set visits every one of the 431 owned PCs.

Service fixtures are comparison controls only. This proof does not establish
real audio playback, complete UI behavior, natural match entry, a visible
gameplay frame, a possession, or a full game.

# First tip contact and phase transition

The first phase consumer is now traced. `65DB0` starts phase `81` with a `78`
countdown and the ball at height `5C00`. A successful ball contact in `602CC`
calls `5D140`, then its continuation changes phase `81` to `82`. This is a tip
transition, **not a claim that a player keeps possession**: the intervening
`5BC34 -> 58610` releases the ball and selects an intended receiver.

This checkpoint supplies bounded CPU owners in `game_tipoff_phase.c`. It does
not yet run the whole collision/input/ball loop or produce a playable frame.
The previous first-path document's “phase consumer untraced” frontier is
superseded by this source trace, not by a natural gameplay execution claim.

## Actual loop and input path

`68BF8` invokes `67468`, which performs the third `65DB0` and `63EDC` startup
calls. Its quarter-zero branch additionally calls `673F0`, which performs
another `65DB0` at `67448`; the unconditional initial frame is later at `674E0`.
During each admitted simulation step, `68BF8` runs `67550/675E4`. While
`FDB7C` is nonzero, its countdown route uses `7A668` and subtracts `FDB6C` from
the halfword, then clamps a negative signed result to zero. The zero-countdown
route runs clocks, `2DE34`, player update `6801C`, and ball update `6EF60`.

The subsequent order is `60EF8` sort, `60FBC` collision processing, conditional
re-sort, AI/rules, then `686B8` input. `686B8` maps the actual device word via
`2D2DC`, computes edges through `700E4`, and calls `61760`. The logical `20`
input route can reach the existing `6A2E4` jump owner or its fallback motion
queue `77/78/79`. This is not a direct physical-button constant or a guaranteed
won tip. The input owner checks countdown, actor/motion state, and other flags.

`60FBC` compares sorted entity references and calls `60E8C`. That helper gates
the wrapped signed X difference, swaps its arguments if the second entity's
word0 is10, and calls `602CC` with the already-computed X difference. `602CC`
checks same-side possession, both B4 cooldowns, Z distance, `7066C` distance,
height difference, the period clock, and stop flags. In phase81 with no intended
receiver it tries hand0, hand1, then body contact. The geometry helpers return
signed classifications; a negative classification takes the separate deflection
path and must not be turned into a successful catch.

## Closed owner boundaries

| Native function | Actual source extent and result |
| --- | --- |
| `nba97_game_tipoff_hand` | Complete `2D37C`; reads the live hand arrays and produces X,Z,height native temporaries. |
| `nba97_game_tipoff_hand_contact` | Complete `600F0`; wrapped signed bounds and the required mode0 `5FC88` boundary. |
| `nba97_game_tipoff_body_contact` | Complete `60008`; signed16 argument narrowing, ground override, classification and reached `5FC88` call. |
| `nba97_game_tipoff_contact` | Complete `601B8/60240`; publishes FDC30 before reading hand geometry, then calls the owned geometry helpers. |
| `nba97_game_tipoff_release` | Complete `5BC34` and `2AB70`; publishes receiver/actor/RNG effects, then requires actual `58610`. |
| `nba97_game_tipoff_after_acquire` | **Only `602CC:608A4` through return**, with actual post-`5D140` registers supplied by the caller. Includes phase81 and the ordinary continuation branches. |

The continuation requires captured S1 (entity), S4 (ball), S3 (previous current
team), and the actual full v0 returned by `5D140`. It cannot be used as a
replacement for the earlier collision checks or possession acquisition.
`5D140` publishes FDBCC and FDC34, team bookkeeping, player state and further
effects before returning; none of that is silently supplied by this API.

On the phase81 branch the continuation writes FE880, FDB96 and FDB72; performs
the four original B4 stores; copies the claim halfword to FDBD0; runs `5BC34`;
requests sound12 for full v0 zero or sound10 otherwise; conditionally resets
each jumper through `582DC`; refreshes the current FDC40 value; writes phase82
and FE884=3; and performs the final FDC40 store. The ordinary continuation
requires `58260`, clears ball velocity18, and restores incoming S3 instead.

## Shared hand geometry and preserved quirks

`2D37C` selects FED20 when the full hand argument equals entity9A bit0, otherwise
FAA04. It indexes by the entity's full word0 times8. Each row contains signed
X,height,Z halfwords. X and Z are shifted left5; the height component shifted
left5 is added, with 32-bit wrapping, to the entity height. This is not the
ZHOTS foot table and not a frontend six-clip pose.

The actual producer link is in `52914`: `52AA8` publishes FAA04+slot*8 into
F0FB4 and `52B08` publishes FED20+slot*8 into FC62C for the body geometry path.
Those live geometry results, including source timing, remain required; merely
sampling motion angles does not establish the hand arrays. The same arrays
serve ball attachment through `58120/581C0`.

Original behaviors deliberately retained:

- `60958..60964` writes second/first/second/first B4, even when references alias.
- `5BC54` selects `entity_table[unsignedSide + 3 + ((rng & 8) != 0)]`, with **no
  side-times-five term**. This is a confirmed original indexing quirk; the port
  does not silently replace the receiver with an inferred teammate.
- `2AB70` uses shared1EDEE, tests bit14, and makes two stores for a zero seed.
- `600F0` has an asymmetric signed height window, and arithmetic wraps before
  signed tests. `60008` has exactly one FDC30 store on its admitted branch;
  decompiler duplicate assignments are not additional original stores.
- `609EC` reloads current FDC40 on the tip branch. It does not restore the team
  pointer captured before acquisition. Motion tests re-read table references
  after child calls instead of caching earlier pointers.

## Ownership, refusals and verification

The access adapter maps proven original data addresses to retained native
objects, with live aliases and per-byte knownness. This is a data-access
contract, not a production CPU/device emulator. It must validate each reached
span and refuse missing, misaligned or noncanonical metadata. Untouched memory
is not preflighted. Whole-value unknown copies stay unknown; mixed byte knowledge
that the view cannot represent must refuse rather than erase known bytes.
Opaque loaded pointers are checked when first dereferenced, after intervening
source stores. In particular unknown FDBD2 still permits FDB88=1, unknown
jumper references still permit the three team stores, and an unknown receiver
still permits the preceding RNG/FDC02/FDC00/FDBCE writes. Directed tests protect
these prefixes; an initial overly eager native check was corrected before
integration, without changing original behavior.

Required calls are synchronous and share the same mutable state. A missing or
refused callee preserves the reached write prefix. Receipts are not resumable;
atomic publication requires cloning all owned state and the callback context.
The native temporary outputs of `2D37C` must not alias source state. No source
stack addresses are fabricated for them. The full-address memory mapping is
still required for encoded entity/global references.

Private evidence is under `.local/verification/native_completion/first_possession/`.
It includes fresh read-only Ghidra exports, direct raw GAMEONLY instruction
words, strict Debug/Release builds, public boundary tests, and native/original
comparisons. The comparisons execute actual source instructions for the owned
slices; external callees are explicitly synthetic mutable/refusal boundaries.
A separate set obtains the continuation inputs by running actual `602CC`
acceptance and `5D140` before comparing the native continuation. These are
explicit source-harness fixtures, not original-device captures or a proven
natural period-to-possession chain. No emulator runs in production.

The C sources and standalone tests are registered in CMake. Live integration
still needs the native state adapter, actual post-`5D140` caller boundary, and
listed real callees. The `58610` requirement now has a complete native owner;
see [ball release and its composed verification](game_ball_release_workflow.md).
It uses actual B8198/B81B0/B81C8
trajectory tables, receiver velocity/court limits and player/status/RNG inputs;
it clears possession and writes the ball's three velocity halfwords. It is not
the existing `6CFE0` player physics owner. Its completion still leaves the hand
geometry producer and upstream `602CC/5D140` composition to integrate.

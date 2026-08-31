# Match render-frame sequencing

`game_match_frame.c` implements the complete CPU sequencing of GAME `49018`,
including its `535C8` scratch guard and `55F0C` reads. This connects the order
of the recovered render passes without inventing their required inputs.
`GameMatchFrame` calls the actual native pose, net, player, court, attachment,
ball, ball-shadow, label and marker owners through one borrowed
`GamePlayerFrame` memory owner. It also implements the `56074` and `5605C`
geometry-control writes. The court adapter transports the preceding shared
player/net projection geometry, ZSF4 and independent LZCR state and exports
every retained prefix on refusal.

This is not a playable match or a complete render-service backend. Remaining
camera, display, synchronization, ordering-table submission and other platform
calls require real implementations through `Nba97MatchFrameIo`. A missing
service stops at the reached original call. The Windows application's
`MATCH-HANDOFF-PENDING` behavior is unchanged: linking this frame owner does
not establish the earlier frontend-to-startup resource path.

## Original order

1. Run `530FC`, toggle the raw bank with `old == 0`, and publish the original
   main and small ordering-table addresses. Enter the interrupt section,
   initialize the small table before the main table through `99960`, then
   restore the captured status.
2. Set projection distance from `B729C`, run camera `51098`, set offsets to
   the original 256 and 120, and run `75D40`.
3. Run net `4B1A4`, players `52914`, then court `4AC68`.
4. Read pause through `FC660`. Only an unpaused frame reads the unsigned
   selector through `FC630`: exactly 1, 2 or 3 invokes `57F5C`, `58120` or
   `581C0`. The caller passes 1, but those functions discard their incoming
   argument. Other selector values do not acquire a default attachment.
5. Run ball `49300`, shadow `49D34` and labels `35BEC`. XOR the byte at
   `*B2048 + 0x53` with 1 before `994F4` executes: the write is in its call's
   original delay slot.
6. Set `1029B0` to zero. Run `4A044` within the original interrupt section,
   synchronize, reload the live index, increment with 32-bit wrapping and
   compare as signed against 10. This nominal ten-actor loop must observe
   index changes made by its callees. `4A044` updates marker presentation;
   it is not the player simulation tick.
7. Set display and draw environments, reloading the bank between calls.
   Synchronize, submit the small table before the main table, retire text
   groups 0, 1 and 2, and restore the captured interrupt status.
8. Check the four original scratch sentinels in order, short-circuiting on
   mismatch. If the mask of scratch word `004` matches, reread it separately.
   Bit `0x200` repeats from `530FC` without returning to the outer game loop.

The redraw loop does not advance simulation again. Bank changes, signed loop
behavior, repeated reads, store order and stale untouched state are preserved.
The original `545C4/545E0` calls change private ABI stacks only; their private
bytes are outside visible storage. Public scratch `004` and `030..03C` remain
required retained inputs. No native stack address is exposed as original RAM.

## Shared state and failure behavior

`GamePlayerFrame::bindContext` validates and borrows the existing address,
normalized-reference and byte-knowledge adapters. It does not reset memory,
geometry or child progress. The descriptors, allocations and callback owner
must outlive synchronous use, and cannot be replaced during a call.

The driver publishes normalized ordering-table references where supported.
The native net pass reads the same memory and imports the complete player
projection state. Its geometry is exported back on success **or refusal**.
`average_scale4` is the real retained ZSF4; it is separate from ZSF3 and is
never inferred from it. H keeps the original signed register representation
of its low 16 bits, which projection consumes as unsigned 16 bits.

The C sequencing layer counts its own memory operations and calls. Each
native child has a separate operation budget and progress record. A budget
is a native bound, not a simulation count or a resumable source PC. A failed
child retains its completed RAM and geometry changes; a failed frame retains
all prior calls and writes. There is no automatic interrupt restoration,
rollback, ordering-table reset or retry after failure. Atomic publication
requires cloning and rebinding the complete memory, geometry and platform
state outside this adapter.

`48FF4` must return the actual known captured status; other original call
returns are unused. The remaining external entries are `48FF4`, `4900C`,
`99960`, `51098`, `75D40`, `994F4`, `99CA4`, `99ACC`, `99A58` and `319B0`.
Existing recovered owners should be composed for their effects. A callback
returning success without performing those effects is only a test fixture,
never a production implementation. `530FC`, `4AC68`, `35BEC` and `4A044` now
dispatch directly to their native owners.

## Verification and limits

The private source audit checks all 186 instructions of `49018`, 46 of
`535C8`, and three of `55F0C` against original GAME SHA256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
All 235 locations execute in the sequencing comparison corpus. The nine
instructions of the two geometry-control helpers are separately inspected.
There is no CPU interpreter in production code.

In each MSVC configuration, 1,022 original/native sequencing cases compare
15,633 ordered stores, 28,283 reads, 69,379 call events and persistent memory
knowledge across 391,328 original instructions. The 378 matching failure
prefixes cover operation limits, service refusal, unknown status/data and
source alignment traps. Mutable service fixtures exercise live bank,
ordering-table and actor-index changes and scratch-triggered redraws.
**Child calls are explicit fixtures in this sequencing corpus**; these totals
do not claim that their original instructions executed in the same run.

The asset-free sequencing test has 152 checks. The separate C++ backend test
has 42 checks and executes the complete native net pass, including actual
native decoding, projection, packet links and animation under synthetic
resource/camera/platform inputs. It then reaches an intentionally unavailable
player input and stops. It checks deliberately unusable serialized resource
pointers, normalized references, untouched packet knowledge, separate ZSF3
and ZSF4, and geometry retained after a partial net failure. It does not
claim a complete native frame or an all-child original/native comparison.
The separate court-compose test adds 845 checks per strict configuration and
compares retained memory, access order, geometry, knownness and progress with
the complete C `4AC68` owner. It does not make the backend fixture's unavailable
player state into a reached court call.

The [net](game_net_workflow.md), [player](game_player_frame_workflow.md),
[attachment](game_ball_attachment_workflow.md) and
[ball](game_ball_frame_workflow.md) owners have their own original-source
comparisons. The [court startup bridge](game_court_startup_workflow.md) has
separate loader-boundary and real native heap-release tests. Their component
results do not prove natural resource arrival, a connected camera/court and
submission path, first possession, a gameplay screenshot or a full match.

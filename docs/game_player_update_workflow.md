# Player-update coordinator

`game_player_update.c/.h` recovers complete GAME `6801C`, 88 instructions. It updates steering, calls actual animation followed by physics, normalizes the resulting angles and clears the source global flag. The two child implementations remain synchronous boundaries so the host can compose its existing owned animation and physics state without duplicate canonical entity types.

The mutable view contains six fields per owned entity (`A6`, `A8`, `A2`, `EA`, `1A`, `9A`), eleven entity-table references, current-entity `FDC3C`, team-context `FDC40` and halfword `FE8C4`. Each value/reference has explicit knownness. The receipt records every direct entity/global/reference store and callback in order, with a maximum of 93 events. Callback-internal effects belong to the child owners.

## Pointer and callback order

The source computes the table start as `8001EDF4 + 1DF8 = 80020BEC` and reads **ten pointer entries**, ending before entry ten at `80020C14`. It does not walk ten physical `F4` entity records. A referenced owned entity can repeat or be out of normal table order; those aliases remain aliases. The normal unused ball entry is retained. A reached unknown or unowned entity reference stops explicitly rather than becoming a default player.

At function entry, the owner writes home team reference `8001EDF4` to `FDC40`. At each iteration it reads the current table entry, captures that entity identity, then writes it to `FDC3C`. At table slot five it additionally writes away team reference `8001EEB8` to `FDC40`, **after** writing current entity and before steering. This is a table-slot boundary, not a test of physical entity identity or side byte.

Each actor then calls `579FC` at `680FC`, followed by `6CFE0` at `68104`, using the same captured entity. A callback may change the pointer table, `FDC3C`, team context, flags or any exposed fields. Those changes must be published into the mutable view before returning. The second call still uses the originally captured entity even if the first call changed `FDC3C` or the current table entry. The next iteration rereads its table entry, so edits to future entries take effect.

The source writes team context only at entry and slot five. It does not restore home/away context after every callback or at return. A callback's team-context mutation therefore persists until the next explicit source write. A callback receives owner, callsite, table slot and captured entity identity; it must not infer the captured identity from a mutable global.

## Steering and preserved quirks

The owner reads signed `A6` and `A8`, subtracts with source wrapping arithmetic and masks the difference to ten bits. Differences below 512 turn positively; 512 through 989 turn negatively. Each non-snap turn changes `A8` by 35. Smaller positive differences and negative-wrap differences 990 through 1023 snap to the raw target halfword. The exact 512 tie chooses negative rotation.

The negative-wrap snap preserves a source bug: after snapping `A8`, the delta remains the **positive modular value** 990 through 1023. The initial `EA` store therefore uses that large value times four, not the short negative displacement. For example, current angle one and target zero write `EA=4092`. If status `9A` bit one is set, the owner first writes that value and then writes its negation. Both stores are retained, including when both values are zero. Actor byte 20 skips the `EA` stores entirely and does not require a known status field.

After both child calls, the source rereads `A8`, `A6` and `A2` from the captured entity and masks each to 1023 in that order. These are the current post-callback fields, not saved pre-call inputs. Unknown copied/masked fields retain unknown provenance; they are never replaced with a claimed numeric zero. After all ten actors complete, it rereads `FE8C4` and writes `FE8C4 & FFFD`, retaining every other bit.

The API mutates a caller-owned candidate in source order. Missing callbacks return pending at the first reached call, whose event remains incomplete. Callback return one means its actual work completed and state has been synchronized; zero means pending and a negative result means failure. On pending/error, the direct prefix and receipt remain visible, without angle normalization or final flag clear that the source has not reached. Use an outer candidate for atomic publication and do not resume by blindly rerunning an unfinished prefix.

## Evidence

Private receipts and strict `/W4 /WX` native builds are in `.local/verification/native_completion/player_update/`. Each `/Od` and `/O2` build matches 4,096 original coordinator cases. The first 2,048 exhaust every ten-bit delta with both status signs; the remaining cases include live table aliases and child mutations to angles, future table entries, current entity, team context, actor/status fields and final flags. All 88 source instructions execute.

The synthetic boundary comparison checks every original direct store/call in order, full owned state at all 81,920 child boundaries per build, and all eleven entity records including untouched bytes. Across both builds, 665,014 receipt events and 21,987,328 final entity bytes match. The child hooks in this proof are explicitly synthetic mutable boundaries, not recovered-callee credit.

A separate composition proof executes original `6801C` with actual original `579FC`, `6CFE0` and their reached transitive instructions, without semantic callee hooks. Read-only entry observations compare each native child boundary without replacing original execution. Actual post-`640D8` mocap headers and original lookup bytes are used. The explicit synthetic ten-player start supplies actor byte two, zero horizontal velocity, `C4=600`, `C8=256`, known positions and actual setup transitions 39 then 77 followed by queue entries 78 and 79.

That proof runs 300 complete updates per build across ticks one, two and three. It compares 12,000 actual child entries and 33,818,400 entity bytes across both builds, including each child-entry snapshot and final state. The unused eleventh entity remains unchanged. All ten supplied players reach the same respective source peak heights 7200/6912/6624. Normal unmutated context ends on away team, captured entity nine, and `FE8C4=FFFD`. This is bounded coordinator/animation/physics composition, not an input, possession or natural gameplay-capture claim.

GAMEONLY SHA-256 is `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`; mocap SHA-256 is `31ef711fb043c1d8b2ae22c15487af0fb32cf6b1fe86ccec65f50166db5fa559`. Source instructions, binary resources and interpreter material remain private.

## Integration and next source boundary

The native build now links this C source and test. The [C++ player-update bridge](match_player_update_workflow.md) applies prior coordinator stores to a candidate, runs the actual animation or physics owner using `call.entity`, and refreshes this view before returning success. It resolves `FDC40` through the actual team reference and retains child pending/errors with their prefix receipts; no missing owner returns a successful no-op. This does not yet connect gameplay input or natural entry.

The most direct remaining tipoff producer is the gated action route in `61760`, not a manual enqueue of motions. Its logical mask `20` path calls `6A2E4(entity,0)` at `62158` when the earlier distance/claim/phase conditions permit. Otherwise its direct fallback writes `C4=600` at `62174`; phase `81` calls `56B78(77)` at `6218C`, then queues 78 and 79. The non-tip fallback instead quarters signed horizontal velocity and uses 68/69/70.

The next bounded dependency to recover is full `6A2E4` (303 instructions) with the actual `2AB70` RNG step and a precise boundary for its reached `5A570` branch. It uses actual ball position/height/vertical velocity, bound-player bytes `17`/`09`, bound status `20`, player distance/angle fields, globals and original `B89C4`/`B89CA` threshold tables. Its ordinary launch derives horizontal velocity from ball displacement divided by 20, halves it for low rating, sets `C4=600`, then quarters it again in phase `81` before requesting 77/78/79. That differs from `61760`'s direct tip fallback, which retains the existing horizontal velocity. It also consumes RNG even when its zero argument prevents a failed probability test from rejecting the action.

Neither route should be invoked unconditionally. Full `61760` is a 685-instruction input/action dispatcher with additional gameplay and tactical branches; its caller gating and `6A2E4` dependencies remain separate unfinished work. This coordinator recovery does not start or claim that broader input work.

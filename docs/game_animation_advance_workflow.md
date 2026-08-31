# Animation advance and landing

`game_animation_advance.c/.h` recovers complete `579FC`, frame advance `572C0`, base-motion remapping `5703C` and both landing force setters `56D30`/`56DE0`. The frame and remapping owners are also available independently. There are no semantic callback stubs or embedded original tables/resources. `game_animation_internal.h` is shared implementation support for this owner and the queue module.

The actor extends the existing animation state without duplicating its fields. Extra halfwords include forced frames/times/flags, queue tails, blend increment, movement/timing inputs and vertical velocity. It also accepts explicit `word00`, previous height `2C` and actor byte `1A`. Knownness is field-specific. Unknown copied or retained fields remain unknown; a needed unknown comparison stops explicitly. Unknown payloads must be canonical zero metadata and must never be applied as fabricated game values.

Effects contain the complete candidate, masks of written fields and the original store count, including repeated loop stores. There are no externally visible callbacks during these owners. Finite timing loops are reduced arithmetically, retaining their final fields and store counts; no arbitrary frame budget changes their result. Failure leaves output unchanged, including a reached unknown field, missing resource, invalid owned reference or original nontermination. Input/output byte overlap is supported. Immutable resource views may not alias mutable actor storage.

## Resource contract

Every clip view contains the actual normalized flags at `+0`, mode at `+2`, timing step at `+3`, and frame count at `+7`. Use the normalized timing value: `640D8` halves byte `+3` when processing initial flag `8` without flag `10`. Supplying the raw resource byte would change animation speed. There are two 84-entry motion directories. A reached source directory index must resolve to an actual owned header; there is no generic replacement clip. `available=2` means unresolved, zero means absent, and one means an actual header.

Remapping consumes seven signed-halfword lookup views corresponding to `B850C`, `B8538`, `B8564`, `B8590`, `B85BC`, `B85E8` and `B8614`. The usual 22-entry windows are sufficient for the normal default domain, but the original has no such bounds check. Each view declares its actual owned index range, including a signed first index. A reached raw index outside that window returns a native reference error; it is not clamped. The private proof includes a raw default index 60,000 followed by negative intermediate indices, with explicitly supplied original-address windows.

Remapping applies speed `A0`, marker `E4`, controlled-entity equality and height in source order. Signed speed below six chooses `B8564`, otherwise `B8590`; marker zero chooses `B8614`, otherwise `B85E8`; controlled equality chooses `B850C`, otherwise `B8538`; nonzero height adds `B85BC`. The comparison uses the full entity word against the sign-extended global halfword. It then updates eligible live clips and forced clips, flags and out-of-range frames. Default 11 deliberately remains 11 when the mapped result is zero, even though live clips can become zero.

## Timing, queues and source quirks

`572C0` processes the secondary channel before the primary. Timing accumulators and global tick `FDB6C` are signed halfwords; movement `9C` and scale `9E` are unsigned halfwords. Ordinary mode zero uses tick times 256; mode one uses movement; mode five uses scale times tick. Mode three is ordinary tick timing for forced channels and live primary, but angular timing for live secondary. Live primary mode two can synchronize with the already processed secondary channel; otherwise it uses movement. Unsupported modes retain their accumulator.

The following original behaviors are retained:

- Linear timing subtracts `step3 * 16` for each frame; angular secondary timing uses signed `EA`, step3 without scaling, and strict inequalities. An accumulator exactly at either angular endpoint does not cross it.
- A reached zero-step loop can fail to terminate in the source. Native code reports `NBA97_ANIMATION_SOURCE_NONTERMINATING` atomically. It never substitutes a nonzero step. A zero step can still terminate through a queued/default transition; it is not rejected unconditionally.
- Queue consumption treats **any negative** head as empty, unlike the exact-`FFFF` producer search. A consumed queue shifts three clip/auxiliary pairs and writes the final sentinel; its last auxiliary byte remains stale.
- Immediate queued/default transitions discard leftover time. A nonzero queued blend starts a forced clip, initializes its frame/time and blend state, and leaves the previous normal timing accumulator stale. Both channels share increment `80`.
- Blend counters add modulo 65,536 before the `>=256` promotion comparison. Promotion repacks only selected low bits of `9A` and discards all its other bits. This is preserved, not changed to a narrower bit clear.
- Primary mode-two synchronization copies secondary frames without clamping them to the primary frame count. After an immediate primary transition, the source checks the **old** header's mode byte, even though the clip and flags have already changed.
- On an immediate secondary transition, new flags containing `0100` copy `C4` to vertical velocity `18` only if velocity is zero. The primary transition does not perform this launch write.

`579FC` remaps only when both signed forced locks are negative. If current height is zero, previous height is nonzero and actor byte `1A` is not 20, it clears `16`/`14`, writes `E6=10`, and conditionally forces landing clip 38 with blend 40. Secondary and primary eligibility differ exactly as in the source: primary also checks controlled entity, cached flag bit one and current clip 37. Force setters preserve their own old-frame/flag rules; forced primary mode two copies secondary **forced** frame/time without requiring equal locks or clamping. Frame advance always follows successful landing setup. Neither queueing nor `579FC` writes actor `1A`, current height or previous height.

## Original-instruction evidence

Private source, original interpreter, separate native builds, exact source hashes and receipts are under `.local/verification/native_completion/animation_advance/`. The public tests exercise queue launch, stale timing, unsigned blend wrap, status-bit loss, old-header synchronization, angular endpoints, default-11 retention, landing, copied unknowns, native reference failures and atomic original-nontermination reporting. Both builds use `/W4 /WX`.

| Owner | Original instructions | Unique executed |
| --- | ---: | ---: |
| `56C28` | 23 | 23 |
| `56C84` | 23 | 23 |
| `56CE0` | 20 | 20 |
| `56D30` | 44 | 44 |
| `56DE0` | 35 | 35 |
| `5703C` | 161 | 161 |
| `572C0` | 463 | 463 |
| `579FC` | 71 | 71 |

Each `/Od` and `/O2` build is compared against 1,200 queue cases, 1,200 remapping cases, 3,500 frame cases, 3,500 full-advance cases, 250 additional real-resource cases and 85 directed raw-width/signed-map cases. All 840 original instructions execute. Every returning comparison checks all 244 entity bytes, written-field footprints and original store counts. Five explicit zero-step cases independently execute bounded repeating original loops and verify atomic native nontermination. No tested owner or transitive callee is replaced by a hook; the resource setup boundary only supplies actual mocap bytes to original `640D8`.

An additional 500-step actual-resource replay covers ticks zero, one, two, three and raw `FFFF`. Original `56B78` supplies the setup transitions 39 then 77; native queueing supplies 78 and 79, and every native full advance is compared with original `579FC`. With tick one, call 15 enters 78 and writes velocity 600 from the explicitly supplied `C4`; call 61 enters 79; call 91 returns to default zero. Tick two reaches those boundaries at calls 8/31/46 and tick three at calls 5/21/31. Tick zero and negative tick do not advance this sampled sequence. These are bounded animation observations with fixed supplied height, not a physics, input or possession proof. All actor `1A` values remain unchanged.

GAMEONLY SHA-256 is `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`; mocap SHA-256 is `31ef711fb043c1d8b2ae22c15487af0fb32cf6b1fe86ccec65f50166db5fa559`. Original instructions and resource bytes remain private. Synthetic prior states are explicit and are not represented as natural gameplay captures.

## Caller integration boundary

The host build owner adds both animation sources and tests. Populate the actor from the current owned entity, preserving each known bit; provide actual `FDB6C`/`FDBCC`, normalized clip views and seven table windows. Apply effects to the candidate before subsequent simulation or pose-request work. `57B18` consumes the resulting live/forced frames, clips, flags and blend fields; it must not reconstruct missing timing or silently reset unknown fields.

The input caller's actual tip path sets `C4`, switches to 77 through recovered `56B78`, then queues 78 and 79 through this module. That caller still owns its original gating and actor-state transitions. `6801C` composition and sibling physics owner `6CFE0` remain separate: they must advance actual position/height and preserve source call order. Queue and animation completion alone do not establish a jump trajectory, tip winner or possession.

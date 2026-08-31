# Stateful gameplay pose frame

`game_pose_frame.c` recovers the retained-memory effects of GAME `530FC` and
its reached `54FCC` conversion and `55018` blend closure. It supplements the
existing `game_pose_sample.c` value projection. The sampler remains useful to
resource-backed model composition; this owner performs the original frame's
context writes, scratch-buffer writes, pointer publication, and cursor updates.

## Integration contract

Call `nba97_game_pose_frame` at the original `530FC` entry with the live
`Nba97PlayerFrameContext`. The owner uses only `access`, `user`, and
`operation_budget`; it does not invoke a CPU interpreter, loader, renderer, or
synthetic child service. On success it has processed the ten physical actor
records starting at the pointer in `FC654`.

The owner copies `F0ED8` to `F0ED4`, clears `1029B0`, advances the live context
cursor by `0xBCC` per actor, and publishes each actor's final primary and
secondary pose pointers at context `+BBC/+BC0`. It copies request B frame
halfwords to context `+18/+1A` even when the B clip is absent, matching the
source order. Conversion uses the original maps at `B79B0/B79E0`; converted
and blended output addresses are the original fixed scratch regions.

Compose it as a direct native `530FC` dispatch inside the existing `49018`
match-frame sequence. Do not call `game_pose_sample.c` for the same source side
effects, and do not treat successful pose processing as proof that model,
camera, texture, or GPU services have run. Failure retains every earlier source
write and reports its exact stopped PC/address. It is not atomic or resumable.

## Preserved source behavior and bugs

* `54FCC` executes at least once even when its signed count is zero or negative.
  It flips signed X/Z as `0x800 - angle`, preserves Y, and leaves joint marker
  halfwords untouched.
* `55018` reads all six source halfwords before its three stores. Source and
  destination aliases therefore use captured inputs. The original asymmetric
  candidate search, strict ties, wrapping low-32 multiply, arithmetic shifts,
  and weight-128 path are retained through the existing Euler arithmetic owner.
* An opaque or partly unknown `F0ED8` source `LW` makes the entire register
  unknown before the following `SW`. The native owner deliberately does not
  preserve individually known bytes in that destination word. This was found
  by the original-instruction comparison and is a retained PS1 register
  behavior, not a host-memory convenience.
* Converted and blended scratch root/marker padding remains untouched. The
  owner does not initialize it, validate clip counts, clamp frame indices, or
  replace original aliases with host allocations.
* Source pointer/reference and knownness failures retain their completed
  prefixes. No rollback, fabricated null pointer, or retry cursor is provided.

## Evidence and limits

Private evidence is under
`.local/verification/native_completion/pose_frame/`. Strict MSVC Debug/Release
and GCC 11.4 Debug/Release with UBSan each pass 1,382 public checks. The
independent comparison executes unmodified original GAME instructions without
callee hooks for 790 cases per configuration: 298,707 stores, 606,165 reads,
7,123,381 instructions, and 92 refusal prefixes. Native and original event
order, retained memory, and byte knownness match.

Raw source binding covers all 307 words of `530FC`, all 19 words of `54FCC`,
and the 212-word `55018` owner. Original tests execute 204 `55018` words. The
remaining eight are listed in `source_check.json` and proven unreachable over
the complete signed-halfword difference domain: a negative absolute-value
branch after a positive delta, a positive `delta - 0x800` branch while delta is
nonpositive, and selection of `abs(delta - 0x1000)` over the nearer
`abs(delta + 0x1000)` when delta is at most `-0x800`.

These fixtures use owned synthetic resource, entity, map, and scratch state.
They do not establish natural match entry, a connected gameplay frame, or a
visible court/player screenshot.

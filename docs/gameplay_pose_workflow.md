# Gameplay pose requests, sampling, and foot offsets

These owners recover the gameplay path independently of the six frontend clips:

| Native owner | Original owner | Scope |
| --- | --- | --- |
| `game_pose_request.c` | GAME `57B18` (273 instructions) | Ten physical entities, normalized header caches, resolved pose requests, foot stabilization |
| `game_pose_sample.c` | GAME `530FC` (307), `54FCC` (19), `55018` (212) | Renderer-value projection: twenty signed Euler triples and signed root height |
| `game_pose_sample.c` foot helpers | GAME `66F88` prefix-producing loop; `2D76C` (90), `AA814` (79), `AA788` (11) | Actual ZHOTS rows, quarter-wave lookup, rounded fixed-point rotation |
| `gameplay_pose.cpp` | Native resource lifetime | Immutable motion, ZHOTS, and trig byte ownership; no guessed source globals |

`530FC` also writes render-context pointers, copies retained request B frame fields to context `+18/+1A`, advances context/global cursors, and uses scratch buffers. The sampler projects only the resulting values used by model composition; it does not claim those other effects. `66F88` additionally performs temporary height-cache writes and retrying I/O. The prefix helper does not claim those effects either.

## Integration contract

Add `src/recovered/game_pose_sample.c`, `src/recovered/game_pose_request.c`, and `src/gameplay_pose.cpp` to the native target alongside the existing gameplay mocap owner. The two new public tests are `tests/game_pose_sample_tests.cpp` and `tests/game_pose_request_tests.cpp`; the sample test also links the C++ motion/pose owners. No test requires original assets.

Extract the original assets privately and retain one `GameplayPoseResource`:

* `ZHOTS.BIN`: retail raw-CD LBA 249065, 25,284 bytes, SHA-256 `14389433d47a33d16d5bd4ee9e82b2ffdf85b85e7f8f7f8a1944f6df456d0061`.
* `foot_trig.bin`: 1,028 bytes from GAME address `800D6E30`, offset `0xD6E30 - 0x15000` in the supported GAMEONLY overlay. It contains 257 signed little-endian words; SHA-256 `c3b03a2581960f9b22f2f29fb52f30bf36eb87c3896c8e4f62c708b3c247f880`.
* The existing immutable `GameplayMocapResource` owns raw ZMOCAP plus the original `640D8` normalized index. The new owner retains that exact generation with its derived foot prefixes. Replacement construction throws before publication; an earlier shared owner remains valid.

Source `66F88` stores each of 84 unsigned-halfword prefixes **before** adding the maximum normalized count from its two optional directory entries. Missing entries contribute zero. The retail sum is 2,107 rows; each row is 12 bytes, exactly the retail ZHOTS length. Different channel counts, aliases, and signed backward motion-data offsets remain intact.

At the actual `57B18` boundary, resolve the ten consecutive `0xF4` records beginning at the entity pointer stored in `20BEC[0]`. This is not a loop through ten independent entity-table references. Copy the fields listed by `Nba97GamePoseEntity`, keeping existing knownness; zero payloads are not evidence of source initialization. The current/previous animation fields and extra halfwords `52/56/62/66` come from the actual preceding animation owner. Header timing byte `+3` belongs to that advance owner; this request owner consumes normalized flags/count only.

Call `nba97_game_pose_requests` with the immutable motion index and the actual foot callback. `GameplayPoseResources::resolveFoot` is an adapter for the completed foot leaf. Its context is the **address of a retained `GameplayPoseResource` variable**, not `.get()`. It reads current secondary ID/frame, `C6` scale, `9A` mirror flag, `A8` angle, and word `10` height, requiring known values. Copy changed request/cache/foot fields back into the canonical entity state. A native refusal retains a prefix of source writes and reports the number of wholly completed entities; it is not a resumable cursor and must not be retried blindly.

After requests succeed, use `nba97_game_pose_packet` and `GameplayPoseResources::sample`. The packet consumes entity IDs `84/86/88/8A`, already physical frame fields `8C/8E/90/92`, blend weights `94/96`, and conversion flags `9A`. Unused negative-B frame/weight fields need not be known. Sample channels independently. Output parts `0..7` come from the secondary stream; `8..19` come from primary. Copy these Euler components and root height into the existing Zdomf model-composition value interface without calling its frontend decode, logical-tick sampling, conversion, or blending helpers. The existing composition code consumes angles and root height, not marker/root-word metadata. This establishes the pose-value bridge, not equality of the complete gameplay model/camera/texture renderer.

## Preserved source behavior and bugs

* A negative signed previous ID means absent, including values other than `FFFF`. Nonnegative IDs are looked up in their channel only.
* Flag 8 halves physical frame indices. An odd current frame without a previous clip creates a same-clip blend of weight 128. It wraps the second physical frame to zero **only** when widened `logical + 1` equals that channel's normalized byte count. There is no modulo or clamp. Logical `FFFF` increments to 65536 for that comparison.
* An inactive B request leaves its old B frame and blend weight untouched. A previous clip branch preserves weight and conversion flags. Midpoint synthesis only replaces the corresponding previous-conversion bit in `9A`.
* GAME `54FCC` flips signed X/Z as `0x800 - angle` with halfword truncation, preserving Y. It maps primary source parts by `{0,1,2,3,8,9,10,11,4,5,6,7}` and secondary by `{4,5,6,7,0,1,2,3}`. It writes only three angle halfwords per joint.
* GAME `55018` adjusts the **starting** Euler representation using asymmetric finite candidate searches and strict comparisons. It does not repeatedly normalize angles. Even weight zero can return a different equivalent representation of A. We preserve its unusual positive-X distance expression `abs(delta - 0x1800)` and tie behavior. Full unsigned 16-bit weights are accepted, multiplication keeps the low 32 bits, and shifts are arithmetic. Weight 128 has the original separate midpoint path. Reusing the frontend blend helper changes results.
* Secondary root height uses the original wrapping multiply/shift interpolation. Converted/blended scratch root word `+0` and marker halfwords are left unwritten by source. `Nba97GamePose` omits them rather than supplying fabricated prior scratch contents.
* The source renderer performs no clip-count check. A physical frame past the declared count can still read valid owned bytes, including the animation mode-2 copied-frame quirk. The native sampler preserves such reads when their complete frame stays inside the file. It does not validate marker constants or demand equal stream counts.
* Foot data is **ZHOTS**, not a foot inferred from the posed skeleton. `2D76C` uses row `prefix[secondary ID] + secondary logical frame`, not the sampled physical frame. Leg zero reads signed bytes `+6/+7`; every nonzero leg reads `+9/+A`. Byte `+8/+B` is not consumed. It scales by unsigned `C6`, mirrors X from `9A & 2`, rotates through the actual 257-word table, and returns entity height as its third output. It does not infer a frequency or use the frontend packed-trig resource.
* Foot stabilization chooses leg zero when flag `40` is present (including flags `C0`), otherwise leg one. Switching boolean leg resets `EC`, while any nonzero old `E0` already matches leg one. After the foot call, `EC` increments as a halfword and is compared **signed** with four. Overflow into `8000..FFFF` therefore takes the early foot-lock branch. Position add/subtract wraps at 32 bits.

Native pointer/reference/extent/knownness checks are explicit host safety policies, not recovered retail error handling. Sampling and resource-prefix output are unchanged on failure. Request processing instead preserves its completed prefix. No source retry loops, allocator side effects, clock values, initial entity values, or scratch metadata are synthesized.

## Evidence and limits

Private evidence is under `.local/verification/native_completion/gameplay_pose/`. The independent comparison first runs original `640D8` against the original ZMOCAP bytes; it does not use the C index to normalize the oracle's memory. It then executes unmodified original `530FC/54FCC/55018` and `2D76C/AA814/AA788` instructions. Original `57B18` runs its actual foot callee; observation of its callback entry does not replace the leaf.

Both Debug and Release builds pass the 54 sampler/resource checks and 213 request checks, with MSVC `/W4 /WX`. Each configuration's independent comparison covers 4,500 blend cases (canonical and arbitrary signed halfwords, full weight range), 1,600 complete render poses, 2,048 direct foot cases, 3,840 complete request entities, and all 84 prefix values. Request comparisons match callback-time field snapshots and all non-stack RAM. They visit all 307 `530FC`, 273 `57B18`, and 90 `2D76C` instructions. `55018` visits 204 of 212: the remaining eight are unreachable under its signed-halfword inputs (negative absolute-value branch of an already positive delta; positive `delta-0x800` while delta is nonpositive; and selecting `abs(delta-0x1000)` over `abs(delta+0x1000)` when delta is at most `-0x800`).

The downstream part order is also directly visible in GAME `55368`: it starts at secondary `BC0+4`, switches to primary `BBC` at part eight, and adds source global `103EDC` to X at part eleven. Its 16,384-byte packed rotation table at `800B3254` is byte-identical to the existing frontend `ZDOMTRIG` source at FEONLY offset `ACD40` (SHA-256 `b106234165685d79392c926192d1b2f9e30125307abde6040747e401a35d0e87`). This packed rotation table is distinct from the 1,028-byte foot table.

These are original-instruction comparisons and native tests, not live-console execution or proof that a visible native gameplay scene has been completed. The scene owner still supplies actual entry state, animation advance, physics, rendering resources, model composition, and camera/presentation ordering.

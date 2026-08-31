# Player physics and direction

`game_player_physics.c/.h` recovers all 308 instructions of `6CFE0` and the full 96-instruction `706E4` direction/magnitude helper. It performs the actual coordinate, gravity, boundary, movement and three-second state updates. Four rule/audio calls remain explicit synchronous integration boundaries; the native core does not claim those implementations ran when they are absent.

The state contains 17 owned entity fields, 14 globals and the actual word at the current `FDC40` target plus `10`. Values have explicit knownness and original widths. Entity offset/width and global address/width helpers identify the mapping; global addresses are offsets from `80000000`. The extra direction word is a value read through the caller's real team reference, not a fabricated pointer or side-derived sign.

The physics API mutates a caller-owned candidate and records every direct store/callback in order. A failure or pending callback leaves its executed prefix and receipt visible. The host must use a candidate if it needs atomic publication. This differs from the standalone direction helper, which returns atomic effects. Canonical unknown metadata is retained on untouched/copied fields; reached unknown comparisons stop explicitly. Resources, mutable state and receipt must not overlap.

## Original behavior

The function first stops horizontal velocity when the controlled entity, `FE8E2`, grounded height and zero vertical velocity conditions all hold. It copies current X/Y/height to `24`/`28`/`2C`, then applies gravity as `velocity -= tick * 24` and `height += velocity * tick`. All source word arithmetic wraps before signed comparisons. Height below zero is clamped; actor byte 20 bounces only when the updated vertical velocity is below -192, using arithmetic shift of its negation by two. Other actor states stop vertically.

The source's intended `2C=FF` landing marker has a preserved bug: it compares the height just copied to `2C` against the still unchanged height at `10`. No callback or intervening height store separates those operations. They are equal in this owned-storage contract, so the `6D0E4` store is unreachable. The native code retains the same comparison instead of repairing the marker. The following animation call can still observe the normal prior-height value stored by physics.

Horizontal motion uses signed velocity and tick halfwords, with wrapped word accumulation. Crossing X thresholds `±17800` chooses boundary directions 7/3; hard clamps are `±1A000`. Crossing Y thresholds `±C800` maps the current direction through the actual signed bytes at `B8A54` or `B8A5C`; hard clamps are `±F000`. Only outward-pointing velocity is cleared at a hard clamp. The boundary result writes `(direction-1)*128+1`, or zero, into `C2`. Negative raw table bytes remain negative; they are not replaced with a valid direction.

The three-second logic preserves its exact region inequalities, actor checks and signed comparisons. The non-actor-1 branch tests signed `X XOR team_direction <= 0`, including exact equality; it is not merely a sign-bit test. Actor 1 also checks `FE8E0`, signed phase, four blocking globals and the remaining-time word. The timer adds signed halfwords, triggers at 300 or more and caps at 300. Disabled rule option `21D8F` skips calls but still caps. The original can request its rule/audio sequence again while the conditions remain true; the core adds no debounce.

The helper writes direction to `A2` only for a nonzero vector. Physics always stores the returned magnitude to `A0`, truncates it to a signed halfword, multiplies by signed tick and unsigned `C8`, wraps both products and arithmetic-shifts by eight for `9C`. Large raw velocities can therefore produce a negative signed intermediate. For actor 12 with `(FE910 & 10) == 2`, the final source store clears `A0` without recomputing `9C`. That stale movement relationship is retained.

## Direction resource and raw-input contract

`706E4` negates negative raw 32-bit components with wrapping arithmetic, chooses an octant with signed comparisons, scales both values down by eight-bit logical shifts until they fit 16 bits, then calculates a rounded ratio into the real byte window beginning at `D72B4`. Equal scaled components use 128 directly. The result is mapped through the octant, written as a halfword, and exactly 1024 is written again as zero. Magnitude uses the original approximate expression and signed comparisons, not a square root or generic `atan2`.

Physics supplies sign-extended velocity halfwords, so its reached byte indices fit `0..256`. A 257-byte actual table view is sufficient for that caller. Standalone raw 32-bit inputs retain the original `INT_MIN` negation bug: it remains negative and can select the wrong major axis, cause division by zero or address a wider adjacent table window. The native helper reports source division by zero or an unowned lookup explicitly. It never substitutes a direction, changes the major axis or clamps a table index. A zero vector returns magnitude zero and does not require a table or known previous direction.

The two boundary views contain signed original bytes; source indices are only 0, 3 and 7. The direction and boundary windows can refer into the same immutable source pack as the animation remapping data. No original lookup bytes are embedded publicly.

## Synchronous rule/audio boundary

When the rule branch is reached, calls occur in this order:

1. `29590(11)` at `6D3D0` if `FDB94` was zero, otherwise `29590(12)` at `6D3E0`.
2. `295C8(5000)` or `295C8(20000)` from that already selected branch, at `6D3EC`.
3. `62300(9)` at `6D3F4`.
4. `62660()` at `6D3FC`.

No stale register is invented as an argument to the no-argument fourth call. The callback receives the mutable state after prior direct stores. It must run the actual callee in the host context and publish relevant mutations back before returning success. Its return values are one for complete, zero for pending and negative for failure. With no callback, the native owner returns pending at the first reached call and leaves its event incomplete. After all four complete, the owner writes `FE882=2`, writes timer 300, and computes movement from the callback-updated velocities, tick, scale and actor flags.

The receipt records direct owner stores and callback completion; callback-internal stores belong to those separate owners. Returning one from a test callback establishes only that test's synthetic boundary behavior, not recovered rule/audio credit.

## Verification

Private source exports, separate strict `/W4 /WX` builds and original-MIPS receipts are in `.local/verification/native_completion/player_physics/`. Each `/Od` and `/O2` build matches 5,000 physics cases and 5,006 raw direction cases. Four additional `INT_MIN` cases independently execute the original divide-by-zero `BREAK` and verify atomic native error reporting. Physics comparisons check every direct store/call in order, all owned state at every callback, and all 244 final entity bytes including untouched bytes. The 788 callback events are explicitly synthetic, with deliberate mutations of fields read after return. Across both builds, 126,008 physics receipt events match.

All 96 helper instructions execute. Physics reaches 307 of its 308 instructions; the sole missing instruction is `6D0E4`, the unreachable landing-marker store described above. Its equality proof is a source invariant, not additional runtime coverage. All other physics instructions execute. The public tests separately cover launch/bounce, clamps, negative boundary bytes, signed-XOR equality, callback branch capture, pending prefixes, stale `9C`, zero-vector provenance and raw helper traps.

A further private replay executes original `579FC` followed by original `6CFE0` without owner hooks, using actual post-`640D8` mocap headers and table bytes. It compares the same pair of native owners at both intermediate boundaries for 480 steps per build, totaling 468,480 entity bytes. The explicit synthetic actor starts at midcourt with actor byte 2, `C8=256`, `C4=600`, and actual setup transitions 39 then 77 followed by queue entries 78 and 79. For ticks 1/2/3, launch occurs at calls 15/8/5, first grounded height at calls 63/31/20, and peak heights are 7200/6912/6624. These different tick trajectories are the original arithmetic, not normalized jump curves. Actor byte remains 2. Landing writes `E6=10`, but live clip 79 is above the generic landing-force threshold, so no clip-38 fallback is fabricated.

This establishes bounded animation/physics composition with explicit starting state. It does not recover `6801C` composition, input gating, a natural gameplay capture, tip winner, rule/audio implementations or possession. GAMEONLY SHA-256 is `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`; mocap SHA-256 is `31ef711fb043c1d8b2ae22c15487af0fb32cf6b1fe86ccec65f50166db5fa559`.

## Host integration

The build owner adds the new C source and test. Construct the state from the actual entity, globals and current team reference. Apply each direct effect to the candidate, preserve field knownness, and dispatch any reached callbacks synchronously with shared-state synchronization. The normal simulation call order remains animation advance followed by physics; do not reset prior height or velocity between them. Feed the resulting animation/movement state to subsequent pose and gameplay owners. Missing rule/audio owners remain an explicit pending boundary rather than a successful no-op. No host, CMake, UI or Git files are changed by this recovery.

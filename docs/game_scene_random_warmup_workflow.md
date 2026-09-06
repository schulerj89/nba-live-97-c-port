# GAMEONLY scene random warm-up recovery

`nba97_game_scene_random_warmup` owns the complete GAMEONLY routine at
`0x800802AC..0x80080303` (88 bytes, 22 instructions). Fresh read-only Ghidra
evidence is kept privately at
`.local/evidence/tipoff-recovery/game_800802ac.txt`; its routine digest is
`48d621e3c50d99bbcd8289dfb45f224bc03aa5cfdf642239bf198016741459cf`.
The scene-load wrapper `0x8002DB68` reaches this owner at call PC
`0x8002DB70`. A tracked-source search found no prior complete owner.

The routine subtracts `0x18` from the raw live `sp`, propagating per-byte carry
knownness before the first store requires a concrete address, and saves `ra`
at offset `0x14`. Its
first JAL assigns `ra=0x800802BC`, then the delay slot saves the entry `s0` at
offset `0x10` before startup child `0x800800F8` begins. It calls random child
`0x8002AB70` twice. The first return is masked to seven bits and increased by
64 in the second call's delay slot, producing the usual 64..191 warm-up count
in `s0`. The second return is masked to 16 bits in the seed child
`0x80093694` delay slot. The routine then calls step child `0x800935C4` while
live `s0` is nonzero; each call sees `s0` after the source decrement in its JAL
delay slot. Finally, it reloads `ra` and `s0` through the final live `sp`, adds
`0x18` to that same stack pointer, and returns.

All 32 GPRs use the existing `Nba97GameMatchInitializeRegisters` word and
per-byte-knownness layout. Children receive the exact JAL link register and
post-delay state, may mutate every GPR and retained memory, and return through
typed synchronous callbacks. The seed child can replace `s0` with zero and
skip the loop. Each step child can replace the decremented `s0`, changing the
next BNE result or creating a runaway loop; the parent operation budget bounds
that prefix and reports it as a limit, never as completion. Masked-off bytes
become known, uncertain count and decrement bytes stay uncertain, and a branch
continues only when its zero/nonzero result is established. No native random
generator or invented child return is used.

The baseline also contains `nba97_game_jump_rng`, an atomic owner for the
`0x8002AB70` state transition used inside a different recovered caller. That
API accepts a detached halfword and returns aggregate write effects; it does
not expose this boundary's retained-memory access/failure prefix or final live
scratch GPRs. It is therefore not compatible with this full-register callback
without inventing those observables. Both random calls remain typed here; the
algorithm is neither copied nor replaced with a host RNG.

Guest stack words use little-endian retained-memory mappings. Stores preserve
partial byte knownness, and child changes can relocate the two epilogue loads
or rewrite either saved word. Effective-address and stack arithmetic wraps at
32 bits. Missing and overlapping mappings, malformed knownness, unaligned
accesses, unknown control state, callback refusal, and every operation-budget
prefix retain their exact observable progress.

`nba97_game_scene_load_with_random_warmup` is the narrow production
composition for the recovered natural caller. It routes only scene-load child
`0x800802AC` into this owner using the wrapper's live retained memory and full
register file. Scene startup child `0x80048D5C` remains on the wrapper's typed
provider. Adapter progress retains the nested warm-up result and prefix when a
warm-up child refuses or its operation budget is exhausted.

The asset-free focused test generates its stack fixtures at runtime. It checks
all 128 first-random low-seven-bit values with unrelated high and sign bits,
second-seed truncation and partial knownness, exact calls and delay slots,
full register forwarding, live `s0` zero-skip and mutation, byte-boundary
decrement behavior, bounded runaway, live stack relocation, saved-word
knownness, mapping/alignment/wrap failures, callback refusals and malformed
state, exact memory journaling, and all 72 operation-budget prefixes for the
minimum 64-step path. The integration test enters through the recovered
scene-load wrapper, exercises the same production adapter, forwards the full
64-step result into its unresolved second child, and verifies nested budget,
missing-provider, and second-child-refusal prefixes. Fixtures contain no retail
data.

Standalone strict MSVC x64 builds pass in both Debug
(`/Od /W4 /WX /sdl`) and optimized (`/O2 /W4 /WX /sdl`) configurations:
914 direct-owner checks and 12 natural scene-wrapper composition checks in
each configuration. Build products remain under the worktree's ignored
`.local/build/scene-random-warmup/` directory.

Visual classification: `Gameplay shown: NO - no direct visual effect`. The
routine changes retained CPU register and stack state while warming an
unresolved random service. It emits no GPU packet, framebuffer, UI, scene, or
advancing match state. Synthetic child fixtures are test evidence only and are
not scene loading or gameplay.

Manager validation: 914 focused and 12 natural-caller checks passed after the
initial unknown-SP prefix correction. The complete asset-free CTest suite passes
205/205, with progress/recovery/instruction/roster freshness checks current.
An ignored, independent interpreter of the private original instructions matches
1,584 cases across all 22 PCs: full retained memory, all 32 final GPRs, every
child-entry register file, and operation-budget prefixes across all 128 counts,
seed zero-skip and step mutation. This is semantic evidence, not an exact-match
recompilation claim.

The native input-driven verifier now calls this same production composition
from the match-session scene-load event. Explicit synthetic service responses
produce count 65 and seed 0xCAFE; the 65 step entries observe s0 descending from
64 to zero. Its nested receipt records 73 operations, two stores, two reads,
69 child calls, saved/restored ra=0x8002DB78 and frame sp=0x807FFF78. The adapter
records the actual intercepted scene-wrapper event rather than inventing a call
receipt. Scene startup remains a typed fixture in this commit.

Local capture `game-entry-20260905-180608-00ed238c` contains
`scene_random_warmup_verified.json` and `scene_load_trace.json`. Native before/after
scanout hashes both equal
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The unchanged scanout is the diagnostic color pattern. The separate frontend
handoff capture shows User Setup; neither establishes an advancing match.
Capture files and the independent oracle remain ignored local evidence.

The first warm-up child now composes the recovered speech startup at
`0x800800F8` through its production adapter. The six-word seed also uses its
existing production adapter; only the random/count and step services remain
synthetic here. The nested speech receipt verifies the signed timeout after
an equality retry without claiming audible playback.

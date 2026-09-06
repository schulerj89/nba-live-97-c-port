# GAMEONLY display environment recovery

`nba97_game_display_environment` owns exactly GAMEONLY
`0x80099CA4..0x8009A153` (inclusive), 1200 bytes and 300 instructions. The
boundary comes from the fresh Ghidra listing
`.local/evidence/tipoff-recovery/game_80099ca4.txt`; its instruction-byte
SHA-256 is
`68b6e8d4cf5fd051425a8d1f4b8c9ec1252cc2e0790f713166d6c256853248e0`.

The C99 owner keeps all 32 GPRs, HI, LO, and byte-knownness explicit. It uses
mapped 32-bit guest addresses for the live stack, display environment, cached
environment, debug and hardware bytes, and the mutable dispatch table. Its
access journal records each mapped load or store in source order. The owner
packs GP1 display-origin command 05 on every call. Cache differences cause the
source signed clamps and GP1 range commands 06 and 07, then mode differences
produce GP1 command 08. It always delegates the final ordered 20-byte cache
copy and returns the callback-mutable live `s0` in `v0` before restoring the
five saved registers through the live `sp`.

The nine original call sites remain typed boundaries in exact source order:

1. Dynamic debug target `[0x800C55BC]` at `0x80099CF0`.
2. Origin helper `0x8009A8A8` at `0x80099D14`.
3. Dynamic GPU target `[[0x800C55B8]+0x10]` at `0x80099D6C`.
4. Video-mode service `0x800985CC` at `0x80099DE8`.
5. Dynamic GPU target at `0x80099F78`.
6. Dynamic GPU target at `0x80099FA4`.
7. Video-mode service at `0x8009A034`.
8. Dynamic GPU target at `0x8009A114`.
9. Copy service `0x8009CB0C` at `0x8009A128`.

Callbacks receive the actual JAL/JALR entry machine after the delay slot. They
may change every GPR, HI/LO, retained memory, dispatch pointer, stack pointer,
and saved registers. Later source instructions consume that live state.
Unknown branch inputs stop with their delay effects visible, and malformed
accepted callback machines return `NBA97_TEXT_ARGUMENT` with the exact prefix.
The operation budget bounds mapped accesses and callbacks without inventing a
source return.

The production adapter intercepts only the recovered scene-startup DISPLAY
events at `0x80048F20` and `0x80048F78`, validates their source metadata and
JAL return address, and runs this owner over the same retained memory. A
per-invocation provider supplies HI/LO when the parent has evidence for them;
otherwise both remain explicitly unknown. All other scene services continue
through the typed fallback callback. Nested metadata errors remain
`NBA97_TEXT_ARGUMENT`, including accepted children with malformed HI or LO.

The focused synthetic test covers unchanged and changed caches, hardware types,
debug and video modes, origin/range/mode packing, signed clamp and resolution
boundaries, dynamic targets, exact calls and delay arguments, callback-mutated
registers and stack, operation-budget prefixes, partial-known branches,
read-before-store aliasing, invalid regions, alignment, refusal, malformed
children, and deterministic replay. The integration test executes the actual
scene owner through both `0x80048F20` and `0x80048F78`, while keeping every
other scene child typed, and checks explicit known and unknown HI/LO transport.

This isolated CPU command-capture path has **no direct visual effect**. A native
GPU submission implementation can later consume the typed commands, but this
owner does not render pixels or claim gameplay.

Manager integration composes the actual scene startup at both display calls
with the recovered BIOS copy trampoline. Runtime fixtures make the first cache
match and the second differ, exercising all four GP1 command classes. The
synthetic GPU callback uses the original table target 0x8009B16C; video mode
and BIOS byte transfer remain explicit synthetic services. The natural test
checks the exact commands, copied cache, final live return and copy-budget
failure prefix. Native capture logs this same composition with matching pixels.
Independent original comparison passed 3,188 cases across all 300 instructions,
full 2 MB RAM, all 34 machine words, callback entries, mutable stacks/registers,
dynamic targets, thresholds, and budgets zero through 99.

Manager verification passed 146 focused checks, 61 natural/composed checks and
all 301 asset-free CTests. Strict C99 and progress/recovery/instruction/roster
freshness checks passed. Native run
`.local/verification/team_select/game-entry-20260906-023953-8a7b893a`
recorded 98 scripted states. The unchanged-cache invocation performed 36
operations (29 reads, five stores, two callbacks); the changed-cache invocation
performed 49 (35 reads, seven stores, seven callbacks). Both CPU-only frame
hashes were `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
User Setup remained displayed; GPU/video/BIOS services were explicit fixtures.

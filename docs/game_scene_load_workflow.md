# GAMEONLY scene-load wrapper recovery

`nba97_game_scene_load` owns the complete GAMEONLY routine at
`0x8002DB68..0x8002DB8F` (40 bytes, 10 instructions). Fresh Ghidra evidence is
kept privately at `.local/evidence/tipoff-recovery/game_8002db68.txt`, with
SHA-256 `5e2cf98a5fc45be394c4897b6d4701580cf2ac3bd483b3d16dac2b6d2dab1d5b`.
The recovered match-session owner reaches it at call PC `0x8002DA84`.

The wrapper subtracts `0x18` from the live 32-bit `sp`, stores `ra` at frame
offset `0x10`, calls `0x800802AC` and then `0x80048D5C`, reloads `ra` through
the final child's live `sp`, advances that `sp` by `0x18`, and returns. Both JAL
delay slots and the JR delay slot are NOPs. The two children remain typed
synchronous boundaries; this recovery does not translate or simulate either
child's scene algorithm.

All 32 GPRs carry a word and a per-byte known mask. Each JAL publishes its real
link value before child entry, while every other live register and every child
mutation passes through. The second JAL overwrites only `ra`. A child can move
`sp`, so the epilogue intentionally reads a different mapped stack word when
that happens. Entry-frame subtraction, effective-address addition, and final
stack restoration wrap at 32 bits. Guest words use little-endian retained
memory; mapping, overlap, canonical knownness, and alignment failures remain
observable, as do completed prefixes and the two exact child call records.

`nba97_game_scene_load_registers_from_session` is the narrow natural-caller
adapter. The match-session event proves `sp`, `gp`, `ra`, and `s0..s2`; it marks
all other GPRs unknown. It does not infer zero-valued argument registers from a
zero argument count.

The asset-free direct test covers both calls and NOP PCs, complete 32-GPR
forwarding, final-child state, live stack relocation, partial knownness,
unknown `sp` and `ra`, callback refusal and malformed output, exact access
journaling, all four operation-budget prefixes, mapping/alignment/overlap
errors, and 32-bit stack/effective-address wrap. The integration test runs the
production wrapper at the natural `0x8002DA84` match-session boundary and
checks both completion and a second-child refusal prefix. Fixtures are generated
in memory at runtime and contain no retail data.

Standalone strict MSVC x64 builds pass in both Debug (`/Od /W4 /WX /sdl`)
and optimized (`/O2 /W4 /WX /sdl`) configurations: 116 direct-owner checks
and 41 natural match-session composition checks in each configuration. Build
products stay under the worktree's ignored `.local/build/` directory.

Visual classification: `Gameplay shown: NO - no direct visual effect`. This
wrapper only changes retained CPU registers and one stack word. Rendering and
real scene loading still depend on the two unresolved child implementations;
no synthetic scene is counted as source scene output.

The native game-entry visual driver reaches this same owner through recovered
Game Setup, Team Select and User Setup input handlers. It logs both child PCs,
the saved stack word changing from `0x8002DA84` to `0x8002DA8C`, exact reload
order and final registers. `scene_load_verified.json` checks the CPU receipt
against the immediate before/after scanout hashes. The complete asset-free
Debug CTest suite passes 199/199, including progress and metadata freshness.
Native frames and logs remain ignored under `.local/verification/`.

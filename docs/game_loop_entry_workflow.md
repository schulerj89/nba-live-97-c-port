# GAMEONLY loop-entry wrapper recovery

`nba97_game_loop_entry` owns the complete eight-instruction GAMEONLY routine
`0x8002DC38..0x8002DC57`. Match session reaches it at call PC `0x8002DA8C`.
The fresh Ghidra instruction listing has SHA-256
`9fd509dbd7e045a5eb066df7b6d95c7a4e7aa60c20f3a6281310a0747e3ca7af`.

The routine allocates a wrapping `0x18`-byte o32 frame, stores incoming `ra` at
frame offset `0x10`, calls `0x80068BF8` at `0x8002DC40`, reloads `ra` through
the child-mutable live `sp`, advances that `sp` by `0x18`, and returns. The JAL
sets `ra=0x8002DC48` before its NOP delay slot. Both stack accesses are exact
32-bit little-endian guest-memory operations with alignment, mapping, and
byte-knownness checks. No guest address is cast to a host pointer.

## Existing match-tick composition

`nba97_game_loop_entry_with_match_tick` routes the sole child to the existing
complete `nba97_game_match_tick` owner. Its memory callback reads and writes the
same retained regions received by the wrapper. Generic services, player update,
ball simulation, net transform, and match frame remain distinct typed providers.
No missing provider is accepted as a successful tick.

The match-tick API exposes an ordered memory/call prefix and status but is not a
resumable continuation and does not expose live output GPRs or guest stack
state. Every adapter invocation therefore refuses the wrapper at `0x8002DC40`,
including when the tick itself reports completion. The adapter receipt retains
the exact tick result, stop PC, address, entry, counts, and shared-memory prefix.
Every unreported nonzero GPR, including `sp`, saved registers, `ra`, `v0`, and
`v1`, is published unknown. The wrapper cannot select its following live-stack
load until a future source-proven tick interface supplies those outputs.

`nba97_game_loop_entry_registers_from_session` adapts the natural match-session
event without guessing missing registers. It supplies only zero, `sp`, `gp`,
`ra`, and the event's `s0..s2`; all other GPRs remain unknown. In particular,
the match tick receives unknown incoming `s6` unless another source-proven
caller path supplies it.

## Verification

The direct unit test covers exact call and stack order, JAL/NOP state, live `sp`
relocation, partially unknown restored `ra`, callback refusal and malformed
registers, every parent operation-budget prefix, alignment, mapping, overlapping
regions, invalid register metadata, and 32-bit stack/address wrap.

The integration test composes bounded and missing-service tick prefixes. The
natural match-session case reaches the wrapper and then the tick's first
unresolved `0x80066F88` service, proving the nested refusal. No completed-tick
fixture with successful empty providers is accepted as evidence.

The native visual driver captures an isolated retained-memory probe using the
actual diagnostic match-session event. This probe terminates at
`0x80068C24 -> 0x80066F88`, records the wrapper's saved stack word and all 31
unavailable nonzero GPR outputs, and produces no simulation steps or frame
pumps. The older broad coverage harness continues using its explicitly
synthetic RUN_LOOP stage response after the probe; that continuation is not a
source match-loop return. `loop_entry_verified.json` asserts this scope and
pixel-identical immediate scanout captures. Media and logs remain ignored.

Validation passes 59 focused checks, 12 adapter/natural-caller checks and all
203 asset-free Debug CTests, including progress and metadata freshness.

Gameplay shown: BLOCKED. The exact missing service is GAMEONLY `0x80066F88`;
successful wrapper return also requires the existing tick interface to expose
source-proven output GPRs and mutable guest-stack state. Gameplay requires
that the native loop advance and render actual court/player state.

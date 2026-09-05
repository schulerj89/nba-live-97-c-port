# GAMEONLY roster-binding recovery

`nba97_game_roster_bindings` owns the complete GAMEONLY routine at
`0x80063D58..0x80063EDB` (388 bytes, 97 instructions). The boundary comes from
the fresh private Ghidra listing `game_80063d58.txt`, SHA-256
`ae04f6f8be23ad8b85b73281f9f7df1bc375693040a95f677f46fd20479bc3c5`.
Known callers are `0x8002DB90` at call PC `0x8002DBC8` and `0x800659F0` at
call PC `0x80065A54`; the routine has no callees. The older
`nba97_match_roster_indices` helper remains a bounded native projection and is
not a complete owner of this source range.

The owner snapshots the low halfwords of the two selected-team words, links the
two resident team blocks, writes 32 record pointers in descending destination
and source order, publishes the fixed global roster root, and builds twelve
home and twelve away pointers in both mirrored tables with their lineup
halfwords. A normal return completes 159 guest accesses: 50 reads and 109
stores. Each of the twelve iterations reloads both 32-bit team indices and both
unsigned count bytes. Count bytes are not clamped to the native helper's
ordinary 15-player boundary; values from 0 through 255 retain the original
comparison behavior.

All guest addresses remain 32-bit values. Team-index multiplication, cursor
increments, and effective-address additions wrap modulo 2^32. Mapped reads and
writes preserve little-endian byte knownness and source alignment traps. The
post-access observer is host instrumentation for tests and composition probes:
it can synchronously mutate retained memory or any live GPR before the next
source instruction. The access journal records the completed source prefix,
while the operation budget stops before the next attempted access. The reverse
loop's final `a2 -= 0x68`, both conditional jump stores, the away branch's
`v0=t3+t5`, and the outer loop's final `a0 += 0x6E` all retain their delay-slot
effects.

The direct asset-free test covers every unsigned count byte, both mirrored
tables and lineup arrays, the exact 32-pointer reverse table, low-halfword
truncation, live team/count reload mutation, 32-bit address wrap, native-storage
aliasing, unknown bytes, malformed knownness, missing mappings, alignment,
observer refusal, return-address knownness, exact access PCs, final live GPRs,
and every operation-budget prefix. The integration test invokes the owner at
the recovered match initializer's natural `0x8002DBC8` callback and copies the
child's complete live-register result back to its caller. It supplies synthetic
retained regions and a real zero-fill callback; no retail asset or binary
fixture is used.

The native input-driven game-entry capture invokes this same owner from the
match initializer against shared retained memory. Runtime-generated count
bytes 3 and 12 exercise record-zero aliases and the full away table. The visual
verifier checks both mirrored tables, the published guest root, provenance and
matching before/after scanout hashes in `roster_bindings_verified.json`.
Remaining child responses are explicitly synthetic; this is CPU reachability,
not an advancing match loop. Classification: `Gameplay shown: NO - no direct visual effect`.

The focused suite passes 7,136 checks and the natural-caller suite 3,716.
An additional ignored instruction differential executes the original 97
instructions across 1,051 cases, including all count bytes and every access
budget prefix for five branch families. It matches all 32 GPRs, the full mapped
byte region and the ordered access PC/address/value/width/kind journal. The
private source bytes, harness, DLL and receipt remain under
`.local/evidence/tipoff-recovery/`.

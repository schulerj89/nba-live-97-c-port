# Frontend resource lookup recovery

`frontend_resource_lookup` owns the complete FEONLY routine at
`0x8008A2C8..0x8008A407`: 320 bytes and 80 source instructions. The raw source
hash is
`2bc268004a25001f37dc4a8df569c9a94b5dea9e5253ab3533dbc18e08df00d1`.
It is a native C99 instruction translation with all 32 GPR words and byte
knownness masks, HI/LO, retained guest memory, exact branch delay slots, and
bounded failure prefixes.

The routine first calls the lookup service at `0x8008A2E0 -> 0x8008A0A8`. A
cached descriptor always has bit `0x8` cleared in its flags before the old
`0x10` decision is used. A descriptor without old bit `0x10` is copied into a
new allocation and then freed. A miss follows the cache chain rooted through
`0x800C7374`, calls `0x80089FFC` with the initial key, and computes the next
key from the low cache-class nibble. It repeats from `0x8008A36C` only while a
computed next key is nonzero. A secondary hit clears its flags,
allocates, and copies without freeing the found descriptor. The source
deliberately dereferences the secondary allocation result even when it is
zero; the owner therefore reads guest address zero and reports the resulting
retained-memory outcome instead of adding a native null guard.

The seven call sites have fixed contracts:

| Site | Call | Arguments |
| --- | --- | ---: |
| 1 | `0x8008A2E0 -> 0x8008A0A8` | 1 |
| 2 | `0x8008A314 -> 0x800771F0` | 4 |
| 3 | `0x8008A33C -> 0x800909A8` | 3 |
| 4 | `0x8008A344 -> 0x80077638` | 1 |
| 5 | `0x8008A36C -> 0x80089FFC` | 2 |
| 6 | `0x8008A3C4 -> 0x800771F0` | 4 |
| 7 | `0x8008A3DC -> 0x800909A8` | 3 |

All seven are typed dependencies. The adapter composes the committed DC
memory-copy owner at both `0x800909A8` sites and forwards the lookup, heap,
free, and chain services to explicit callbacks. It also validates the natural
committed DF parent edge `0x8007B1F0`, delay `0x8007B1F4`, target
`0x8008A2C8`, FEONLY program, invocation one, one argument, and
`RA=0x8007B1F8` with a fully known mask. A refused or limited nested owner
publishes its exact machine and journal prefix to the parent before the parent
reports the refused call.

The runtime capture is a standalone DI CPU probe. Its synthetic fixtures
return an initial descriptor and allocation while preserving the remaining
machine and RAM. It serializes bounded instruction and memory-access journals,
access width and knownness, complete call metadata and call-time machines, and
the final full machine. The unresolved services remain production typed
dependencies; only the capture implementations are synthetic.

The integration test separately proves actual committed DF-to-DI composition.
It exercises both the initial and secondary copy paths with the committed DC
owner, verifies copied payload bytes and byte-knownness, preserves the parent
RA/SP and HI/LO values, accepts a fully known memory plane absence, checks
nested operation limits and callback refusal propagation, and rejects corrupt
parent PC, delay, target, invocation, site, program, argument count, RA value,
RA mask, machine, and journal metadata. The focused test covers all 80 source
PCs across source paths, exact site arguments, flag-store order, both chain
termination forms, the secondary address-zero read, relocated callback-live
frames, malformed and absent knownness planes, every operation-budget prefix,
and each reachable child refusal prefix.

Strict MSVC `/W4 /WX` focused and integration executables are built outside
the worktree under `.local/build/frontend_resource_lookup_worker`. The final
focused executable passes 160 counted checks, the integration executable
passes 38 counted checks, and the standalone capture parses as JSON with 41
executed instruction events, 16 memory-access events, and four typed calls on
its selected path. These tests prove native routine behavior and owner
composition. They do not show rendered gameplay; the gameplay status remains
`BLOCKED` until the manager completes a native capture through the recovered
game path.

Manager source review corrected the cache-chain back edge to resume at
`0x8008A36C`, leaving `0x8008A368` initial-only. Independent raw-instruction
comparison passed 2,234 cases across all 80 PCs and seven child sites against
C SHA-256 `5e71fded0de337ea2030432101645312a5c00588f8648f797d75920b34d02ee7`.
The private receipt is `.local/evidence/tipoff-recovery/dj_lookup_differential.json`.
It compares all 34 machine words and masks, full 2 MiB RAM and knownness,
exact PC/access/callback journals, all path budgets/refusals, callback-live
state, flag/null quirks, and memory/return faults.

Final manager MSVC Debug passed 160 focused and 302 integration checks,
including full machine return transport for both copy paths, absent knownness
and relocated nested frames. All 415 asset-free CTest tests passed in 35.91
seconds; progress/recovery/instruction/roster freshness passed. The native
input run captured 128 frames in
`.local/verification/team_select/game-entry-20260906-151733-c0d1f924`.
The verifier independently reconstructs all 41 selected-path PCs, 16 accesses,
four full child machines and the full returned machine in
`frames/frontend_resource_lookup_verified.json`.
Before/after pixel SHA-256 is
`42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7`.
The inspected image remains User Setup. Gameplay shown: BLOCKED.
The first missing lookup service is `0x8008A2E0 -> 0x8008A0A8`;
heap, chain and file services and the live match lifecycle remain required.

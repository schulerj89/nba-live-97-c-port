# FEONLY frontend overlay-load entry recovery

`nba97_frontend_overlay_load` owns the complete FEONLY range
`0x8007B11C..0x8007B13B` (32 bytes, 8 instructions). The fresh source-range
SHA-256 is
`97d8f0e4eb51bd581d1431e5995abb4ea56b67408568f334d91a8b93e61029e2`.
The owner allocates a 24-byte frame, saves `ra`, calls `0x8007B15C`, restores
`ra` through callback-live `sp+16`, releases the live frame, and returns.

The source call has three live arguments. The recovered `frontend_main` caller
sets `a0=0x80024854` (the GAMELOAD filename guest pointer) and `a1=0`. The
owner preserves those values and sets fully-known `a2=1` in the JAL delay slot
at `0x8007B128`. Ghidra annotates the immediate `0x8007B15C` child as a void
formal call, but fresh `0x8007B15C` evidence shows that it forwards the live
registers and fresh `0x8007B1D0` evidence shows its transitive callee consuming
`a0`, `a1`, and `a2`. The typed child event therefore reports argument count
three.

The C owner retains all 32 GPR words and byte-knownness masks plus HI/LO.
Stack reads and stores are little-endian guest operations over validated
retained regions. A callback may mutate the full machine, memory, and `sp`; the
epilogue uses that callback-live `sp`. Operation budgets stop before the next
store, call, or load and retain the exact completed instruction, access, and
callback prefix. Missing or refusing children, malformed machine state,
unknown addresses, invalid knownness bytes, unmapped memory, and alignment
faults return their explicit `NBA97_TEXT_*` result without inventing a child
result.

`nba97_frontend_overlay_load_from_frontend_main` is the narrow natural-caller
adapter. It accepts only the committed `frontend_main` event at `0x80028ACC`
with delay `0x80028AD0`, entry `0x8007B11C`, FEONLY program identity,
argument count two, invocation one, fully-known `a0=0x80024854`, `a1=0`, and
`ra=0x80028AD4`. A completed owner maps to `CALLEE_RETURNED`; failures preserve
the owner's exact state and reject the parent call.

The focused test executes the owner directly and covers all eight PCs, exact
store/call/load ordering, transitive argument forwarding, delay-slot `a2`,
child `v0`, full-machine mutation, callback-live frame relocation and aliasing,
all operation-budget prefixes, callback refusal, unknown and unaligned return
targets, wraparound stack arithmetic, optional knownness planes, malformed
regions, and deterministic repeatability. The integration test executes the
committed `frontend_main` owner and routes its natural `0x80028ACC` event through
the adapter. All other services are explicit synthetic fixtures. The loader
fixture preserves the machine and memory except for supplying fully-known
`v0=0x80170000`; later fixtures supply a size and the dynamic entry word only
to exercise the source prefix. The GAMELOAD callback is refused at
`0x80028B68 -> 0x801E1410`, so the integration proves the exact prefix without
claiming a transfer. A separate case refuses the next `0x80028AD8` service and
proves the shorter composed prefix.

The textual capture records all eight instruction PCs, both stack accesses,
the child event and its complete machine, the final machine, the synthetic
fixture contract, and the next unbound production boundary. This CPU-only
routine has no direct visual effect. Gameplay shown: **BLOCKED**. The
filesystem-backed behavior of child `0x8007B15C`, the later GAMELOAD services,
and an advancing native match loop with rendered court and player state remain
unbound; a synthetic loader handle or terminal transfer is not gameplay.

Final manager validation (2026-09-06): final MSVC Debug rebuild passed;
369 focused and 34 actual-main integration/capture checks passed directly.
All 405 asset-free Debug CTest tests passed in 30.73 seconds. Progress,
recovery metadata, instruction semantics and roster-contract checks passed.
The independent raw-instruction comparison passed 40,960 cases over all eight
PCs, all 34 machine words/masks, retained stack bytes/knownness, exact journals,
child mutations, moved/unknown stacks, RA faults and operation/refusal prefixes.
Final compared C SHA256:
e58bce5eaa1233725de0e6df80f2b22991e366d39d57f375c0c926eea6d8a546.

The native input-driven verifier passed with 118 frames at ignored
`.local/verification/team_select/game-entry-20260906-140708-f6047692`.
`frames/frontend_overlay_load_verified.json` proves the exact parent/child
arguments, all source PCs and accesses, complete child/final machines and
matching PPM pixel hashes:
42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7.
The manager inspected `frames/frontend-overlay-load-after.png`.
Gameplay shown: BLOCKED by the documented production loader and match
boundaries; the standalone CPU capture leaves User Setup visible.

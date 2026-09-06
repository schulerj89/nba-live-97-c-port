# GAMEONLY actor-contact eligibility recovery

## Boundary and evidence

This owner translates only GAMEONLY `0x8005F948..0x8005FAA7`, 352 bytes and 88 instructions. The source is the fresh Ghidra listing `.local/evidence/tipoff-recovery/game_8005f948.txt`; its instruction SHA-256 is `ffc105ab1358cf77d142007dc72e5428807fcee9de4e6f592dfde243bc378402`. The only known caller is the recovered actor-contact gate at `0x8005FACC`.

The routine accepts a full 32-GPR and HI/LO machine with per-byte knownness. Guest addresses remain `uint32_t` values mapped through validated memory regions. It saves `s1`, `s0`, and `ra`, evaluates the source mode, identity, state, team, coordinate, and geometry gates, and restores through the callback-mutable live `sp`. The unequal-team and same-team action calls return only their low byte. The same-team X check deliberately has no lower bound.

## Typed calls

The owner exposes four source callsites through three typed kinds:

| PC | Target | Delay slot | Arguments |
|---|---|---|---|
| `0x8005FA18` | `0x8007066C` | NOP | normalized X, signed Y delta |
| `0x8005FA2C` | `0x8005F888` | `a1 = live s1` | live first actor, live second actor |
| `0x8005FA70` | `0x8007066C` | NOP | normalized X, signed Y delta |
| `0x8005FA84` | `0x8005F328` | `a1 = live s1` | live first actor, live second actor |

Callbacks receive the full mutable machine and mapped memory. Accepted mutations remain live, including `s0`, `s1`, `sp`, HI/LO, and memory. A refusal stops at the call prefix. A callback that accepts and then returns malformed knownness produces `NBA97_TEXT_ARGUMENT` while retaining the malformed callback state in progress.

The geometry-child adapter binds both `0x8007066C` callsites to the existing complete `nba97_game_selection_distance` owner. It validates fully known A0/A1 and the exact JAL return address before calling the scalar owner, then publishes only the proven source effects: wrapping absolute values in A0/A1 and the scalar result in V0. `INT_MIN` remains negative after source wrapping negation. Every other GPR and HI/LO is preserved. The scalar API cannot represent partially known operands, so the adapter returns an explicit `NBA97_TEXT_ARGUMENT` result and refuses before calling it when either input is not fully known. Action calls continue through the configured full-machine typed fallback.

## Natural composition

`nba97_game_actor_contact_eligibility_from_actor_contact_gate` implements the actual frozen AM boundary. It accepts only the source event at `0x8005FACC`, delay `0x8005FAD0`, entry `0x8005F948`, three arguments, and a fully known `ra` equal to `0x8005FAD4`. Invalid event, machine, memory, or journal metadata leaves the incoming machine unchanged. Once AN starts, the adapter returns its exact progress machine even on a limit, mapping failure, refusal, unknown decision, or malformed child result.

The integration fixture links the frozen AM owner and the existing geometry owner directly. It proves the source coordinate gate invokes AN with the shifted delta, AN composes the actual geometry owner before its typed action fallback, a completed AN returns through AM as `v0=1`, AM rejection does not invoke AN, and nested failure state propagates. Direct adapter cases cover positive, negative, and `INT_MIN` inputs plus all-other-register preservation and partial-input refusal.

## Validation

The focused fixture is generated entirely in memory and contains no assets. It covers the special-mode and phase-82 identity paths, negative-owner state asymmetry, both team routes and their signed Y boundaries, negative fractional arithmetic shift, wrapped coordinate subtraction, the one-sided same-team X gate, signed geometry returns, low-byte action returns, exact call metadata, mutable callback state, refusal and malformed callback prefixes, per-byte unknown decisions, unknown stores without a knownness array, mapping/alignment/live-stack failures, and every operation-budget prefix. The owner is compiled as strict C99 with Clang `-std=c99 -Wall -Wextra -Werror -pedantic`; the C++ adapter and fixtures use the corresponding strict warning set. MSVC builds compile the same sources with `/W4 /WX`.

## Classification

Gameplay shown: NO - no direct visual effect. This is a CPU eligibility and dispatch routine with no direct visual effect. Recovery does not claim advancing gameplay or a visual change.

The geometry register bridge was checked against fresh Ghidra evidence for the already owned GAMEONLY `0x8007066C..0x800706E3`, 120 bytes / 30 instructions, SHA-256 `02ea580b62a62f699d3cef1e4d73f7abf5de0608480e02dd6ebff20cd1c70034`. A private original-instruction comparison passed 10,000 cases covering all 30 PCs, signed extrema, all 34 machine words and knownness. The eligibility owner separately passed 3,144 original-instruction cases covering all 88 PCs and complete memory/callback/budget prefixes. These private comparisons use ignored evidence; committed tests generate their fixtures at runtime.

Manager validation: 67 focused executions, 14 natural/adapter cases, strict C99, and all 271 asset-free CTests passed. Native input-driven run `game-entry-20260906-000025-89bca4f9` captured 98 driver states and an independent actual-coordinate-gate/eligibility/distance composition: 15 operations, 10 reads, 3 stores, 2 callbacks, typed action `0x123456CD` became `0xCD`, parent gate returned 1. Before/after CPU frames both SHA-256 `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`. The visible native frontend remains User Setup; this fixture does not advance a match.

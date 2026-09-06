# Frontend I/O completion recovery

`nba97_frontend_io_complete` owns the complete FEONLY range `0x800392A0..0x800392F7`: 88 bytes and 22 instructions with source SHA-256 `dca1d4f4bf2b7847a1175abe703ab434c4ed51efccb2af341b536de466f98d7a`. It reads the active word at guest `0x800F84C4`. An inactive word returns fully-known V0=1. When active, it scans as many as eight status words starting at `0x800EF840` with a 36-byte stride; the first definitely nonzero status returns V0=0, while eight zero statuses return V0=1.

The owner retains the source delay behavior. A0 is cleared after the active load even when the branch cannot be decided. Each status branch increments A0 before a nonzero or unknown exit. Each zero-status loop branch increments V1, including after the eighth slot. The final source state therefore exposes A0, V1, and AT as well as V0. Other GPRs and HI/LO remain unchanged. The guest reads occur before a live RA knownness or alignment failure at the final JR. Missing memory, malformed byte knownness, unknown branch decisions, and operation limits retain their exact completed instruction and access prefixes.

`nba97_frontend_io_complete_from_frontend_exit_drain` accepts the recovered natural caller at `0x800394F0`, delay `0x800394F4`, target `0x800392A0`, and RA `0x800394F8`, with zero formal arguments and repeated invocation numbers. `nba97_frontend_exit_drain_with_recovered_io_complete` routes that site to the recovered poll owner and forwards the drain owner's other six child services. Optional parent records preserve each observed call event, full boundary machine, nested result and progress, and complete nested access and instruction journals. Rejected bindings do not publish new records, and pre-journal failures clear current evidence rather than exposing an earlier invocation.

`nba97::captureFrontendIoComplete()` emits printable JSON with stable top-level fields `program`, `address`, `inclusive_end`, `bytes`, `instructions`, `source_sha256`, `completed`, `result`, `contract_failure`, `classification`, `gameplay_shown`, `fixture_contract`, `status_memory`, `drain`, `io_complete`, `final_machine`, and `next_unbound_boundary`. It starts with a synthetic standalone drain machine and retained memory, then genuinely composes the recovered drain and I/O-completion owners. The first actual poll reads a nonzero slot and returns zero. The explicit unbound `0x80038E84` pump fixture preserves the full CPU machine and clears only `0x800EF840`; the second actual poll scans eight zero slots and returns one. The other five fallback children preserve every GPR word/mask, HI/LO, and guest memory. The receipt includes the complete drain PC/access/call journals and both complete nested poll traces, accesses, boundary machines, and final machines.

Strict local validation compiles the focused C11 owner and C++20 natural integration with MSVC `/W4 /WX`, placing all outputs under the ignored root path `.local/build/frontend_io_complete_worker`. Focused coverage includes inactive and all active exits, every nonzero slot, all 22 PCs, all active/status knownness masks, definitely nonzero partial words, unknown decisions, absent knownness planes, memory validation and mapped failures, every load budget prefix, every RA knownness mask, misaligned RA, full machine preservation, and repeated live reads. Integration covers repeated natural drain calls, full parent records, the explicit pump memory effect, nested failures, exact typed guards, stale-evidence prevention, and printable capture serialization.

The earliest production boundary remains the FEONLY overlay-entry JAL `0x8007B838` and startup service `0x80028810 -> 0x8007B844`. Both recovered drain callers now have exact poll adapters. The three-owner integration executes exit drain, I/O drain, and this completion poll in source order. A zero poll still reaches the unbound pump at `0x800394AC` within I/O drain or `0x80039500` within exit drain, both targeting `0x80038E84`. The standalone capture retains an explicitly described preparation fixture for `0x800393F0`; the separate integration test executes that recovered owner. Production frontend lifecycle, loader handoff, and an advancing native court/player match remain required before gameplay or tipoff can be claimed.


The manager added `nba97_frontend_io_complete_from_frontend_io_drain` for the
second natural call at `0x8003949C`, delay `0x800394A0`, and RA `0x800394A4`.
Its records copy actual observed event fields without a pointer reinterpretation
or substituted caller PC. Three-owner tests execute both nested polls and then
the outer poll against live retained slots, with and without a knownness plane.
They also prove a second-poll budget failure propagates through both enclosing
owners and clears the new invocation's access receipt. Exact second-site guards
reject invalid PC, delay, target, invocation, site, argument count, overlay or RA.

Final manager MSVC Debug validation (2026-09-06): 190 focused checks and
25,030 integration/capture checks passed directly; all 403 asset-free CTest
tests passed in 29.74 seconds. Progress, recovery metadata, instruction semantics
and roster-contract freshness passed. Private raw-instruction differential:
57,600 cases, all 22 PCs, all 16 active/status masks, complete machine/retained
state, source journals, budget and return-failure prefixes. Final C SHA256:
138fcba0f763edcccf590da98076fe6320162cbab45f43c82b238f1dc60c7a65.

Native input verification passed with 116 frames at ignored
`.local/verification/team_select/game-entry-20260906-135806-f561adab`.
`frames/frontend_io_complete_verified.json` verifies both real nested reads,
their full instruction/access/machine traces and all parent state transitions.
Before/after PPM pixel hashes both equal
42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7.
The manager inspected `frames/frontend-io-complete-after.png`.
Gameplay shown: BLOCKED by the documented production lifecycle, pump and
loader bindings; this CPU-only capture leaves User Setup visible.

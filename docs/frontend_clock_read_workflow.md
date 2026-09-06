# Frontend clock-read recovery

`nba97_frontend_clock_read` recovers the complete FEONLY subroutine at `0x8008DA5C..0x8008DA6B`: four instructions and 16 bytes with source SHA-256 `9bf283cf0c65c4bd13e3e94df28927dc756088764e78bf2e59298f9faeef85c0`. The C owner performs the live little-endian load from guest `0x800D9AB8`, returns its word and byte knownness in V0, preserves the other 31 GPRs and HI/LO, and returns through live RA. A missing region, malformed knownness, exhausted operation budget, unknown RA, or misaligned RA reports the exact source prefix. The load completes before either RA failure.

`nba97_frontend_clock_read_from_frontend_exit_wait` is the typed full-machine adapter for the recovered wait owner's two clock calls. It accepts the single initial call at `0x8002EFE4` with delay `0x8002EFE8` and RA `0x8002EFEC`, and repeated loop calls at `0x8002F018` with delay `0x8002F01C` and RA `0x8002F020`. Both target FEONLY `0x8008DA5C` with zero formal arguments. `nba97_frontend_exit_wait_with_recovered_clock` routes those sites to the recovered reader and forwards the other eight services to the caller's typed fixture or future production binding. Its progress retains the observed parent event and complete boundary machine, current nested result, and current read access. Invocation gates and clearing prevent rejected or pre-journal calls from publishing stale evidence.

`nba97::captureFrontendClockRead()` returns printable JSON with stable top-level fields `program`, `address`, `inclusive_end`, `bytes`, `instructions`, `source_sha256`, `completed`, `result`, `contract_failure`, `classification`, `gameplay_shown`, `fixture_contract`, `clock_memory`, `wait`, `clock`, `final_machine`, and `next_unbound_boundary`. The deterministic receipt starts from a synthetic standalone wait machine and retained memory, then genuinely composes the recovered wait owner with this recovered reader at both natural sites. It reports the complete 50-PC wait trace, nine wait accesses, all ten observed parent call machines, and each clock read's result, access, loaded value, and complete final machine. The `0x80039260` fixture changes only clock RAM from 1000 to 1361; the other seven fallback children preserve memory and all machine fields except the two poll V0 results. The wait owner's handle and secondary-global writes remain recovered source behavior.

The earliest production boundary is still the unowned FEONLY overlay-entry JAL `0x8007B838` and startup service `0x80028810 -> 0x8007B844`. Within the recovered wait path, the next full-machine boundary is `0x8002EFDC -> 0x8007B2BC`; an existing voice-handle semantic owner does not expose retained-state and output-machine transport. After the newly bound clock call, `0x8002EFF0 -> 0x8006B6A0` remains similarly unbound despite its existing music-status table owner. Actual loader handoff and an advancing native court/player lifecycle remain required before gameplay or tipoff can be claimed.

Strict local validation compiles the focused owner alone as C11 and the integration path as C++20 with MSVC `/W4 /WX`. Outputs belong only under the ignored root path `.local/build/frontend_clock_read_worker`. The focused suite covers all four PCs, all 16 clock knownness masks, no-known-plane reads, live repeat reads, full machine preservation, memory extents, malformed inputs, operation prefixes, and all RA knownness/alignment outcomes. The integration suite covers both natural wait sites, repeated loop reads, absent-plane composition, nested failures, exact adapter guards, stale-evidence prevention, and printable capture serialization.

Final manager validation (2026-09-06): the final MSVC Debug build passed;
650 focused and 22,416 natural-caller integration checks passed directly.
All 399 asset-free Debug CTest tests passed in 29.62 seconds. Progress,
recovery metadata, instruction semantics, and roster-contract freshness passed.
The private raw-instruction differential passed 16,384 cases covering all four
source PCs, all 16 clock byte-knownness masks, machine preservation, budgets,
and return failures. Compared C SHA256:
64bcaadde35ee0522e5f22f0add1293818a8d10e0974e90a612337f27c2c7a17.

The native input-driven verifier passed with 112 frames at ignored
`.local/verification/team_select/game-entry-20260906-133950-35a8ffd2`.
`frames/frontend_clock_read_verified.json` proves two actual reads of 1000
and 1361 within the recovered wait, full source state/call/access order, and
matching before/after PPM SHA256:
42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7.
The manager inspected `frames/frontend-clock-read-after.png`.
Gameplay shown: BLOCKED by the production boundaries described above; the
capture is a standalone CPU composition and leaves User Setup visible.

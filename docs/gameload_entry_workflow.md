# GAMELOAD entry recovery workflow

## Ownership and evidence

`nba97_gameload_entry` is the complete C99 owner of GAMELOAD
`0x801E1410..0x801E14B7`, 168 bytes and 42 instructions. The independently
hashed source range is
`86de52922bd45fe1e8c5dd5768bb04d31a1a1ba8d0c9bc429d8a53b1919ae560`.
The raw program file used for the slice has SHA-256
`9474cc8b9109a1b9eebf14bae1ea1c1542c2d486e24477c086e7cf2523b1961f`.
The fresh source listing is
`.local/evidence/tipoff-recovery/gameload_801e1410_continue.txt` in the
repository root.

This is a GAMELOAD program owner. The existing FELOAD owner at the same
virtual address has different clear bounds and remains separate. The owner
does not reproduce either child at `0x801E1590` or `0x801E136C`.

## Recovered behavior

The source clears exactly 2,073 aligned words in
`[0x801E903C,0x801EB0A0)`. It loads the raw stack-top word at `0x801E8B70`,
applies the trapping signed `ADDI -8`, ORs bit 31 into `sp`, and derives the
heap size by subtracting the retained `0x801E8B6C` word and stripped
`0x001EB0A0` base. It stores heap size at `0x801E8B50`, heap base at
`0x801E8B4C`, and entry `ra` at `0x801E903C` in source order.

The first typed callback is InitHeap at `0x801E1498 -> 0x801E1590`. Its delay
slot executes a trapping `ADDI a0,a0,4`, so the child receives
`a0=0x801EB0A4`, the live derived `a1`, `ra=0x801E14A0`, and all other CPU
state. A returned callback is followed by the live reload of saved `ra` from
`0x801E903C`. The second callback is GAMELOAD main at
`0x801E14AC -> 0x801E136C`, with `ra=0x801E14B4`. A returned GAMELOAD main
falls through to the source `BREAK 1`; only a child that explicitly reports
`TRANSFERRED` completes the entry as a transfer.

All 32 GPRs, HI/LO, and byte-known masks remain explicit. Stores preserve
partial source knownness when a knownness plane exists. Unknown signed-overflow
decisions refuse before committing the `ADDI`; definite overflow reports the
arithmetic trap. Memory accesses, callback attempts, and dynamic source PCs
retain their exact completed prefix on every failure or budget limit.

## Adapter and natural composition

`nba97_gameload_entry_from_frontend_main` accepts only the recovered frontend
main boundary `0x80028B68`, delay `0x80028B6C`, GAMELOAD target
`0x801E1410`, invocation one, zero arguments, and live
`ra=0x80028B70`. A returned child path that reaches `BREAK 1` is refused back
to frontend main with the owner result and full resulting machine intact. An
accepted adapter return therefore means the GAMELOAD owner observed a real
typed child transfer.

`nba97_frontend_main_with_recovered_memory_copy_and_gameload` wraps the
committed frontend-main and frontend-memory-copy composition. The committed
copy owner executes the natural `0x80028B54` payload copy, and this owner then
receives the copied dynamic entry at `0x80028B68`. Every other frontend service
is forwarded to the caller. The integration fixture runs this full recovered
chain with and without a knownness plane and proves that a returned GAMELOAD
main ends at the real entry `BREAK 1`; it does not invent a successful
GAMELOAD transfer.

## Capture schema

`nba97::captureGameloadEntry()` emits deterministic JSON for a synthetic
standalone entry machine and retained 2 MiB memory. It uses the raw static
configuration values `0x00800000` and `0x00008000`. Both unbound child fixtures
return and preserve the supplied full machine and memory.

The receipt includes source identity, explicit fixture contract, result and
terminal flags, exact operation/access/read/store/clear/callback/PC counts,
four access boundary samples, both typed call records with full 32-GPR and
HI/LO snapshots, final full machine, published memory words, and the exact
next unbound boundaries. Because a successful trace contains 10,402 PC events
and 2,079 accesses, the full journals use canonical FNV-1a-64 hashes. The
receipt documents byte order and field order used by both hashes so an
independent verifier can reproduce them.

The receipt reports `gameplay_shown` as `BLOCKED`. The first missing production
service is `0x801E1498 -> 0x801E1590` InitHeap with full-machine transport;
after a returned InitHeap fixture, `0x801E14AC -> 0x801E136C` GAMELOAD main is
also unbound. No capture field claims gameplay or tipoff.

## Validation

Strict worker outputs live only under the ignored root path
`.local/build/gameload_entry_worker`. The focused owner test covers the normal
returned-to-BREAK trace, both transfers, refusal and malformed callback state,
all operation-budget prefixes, signed overflow and ambiguous partial inputs,
saved-RA knownness, absent knownness planes, malformed/unmapped memory, native
region aliasing, exact event contracts, and source/journal counts. The
integration test covers the natural frontend-main/copy/entry composition,
adapter guards and stale-binding evidence, honest child refusal, and capture
determinism/printability.

The manager's independent raw differential compared 5,452 cases over all 42
PCs and both child sites, including all 34 CPU words and masks, retained bytes
and knownness, exact PC/access/call journals, all operation budgets, callback
mutation, saved-RA reloads, signed traps, transfers, refusals, and configuration
wrap. It passed against core C SHA-256
`24aefe6de2e3c564ef80e4368c814c243372c047b2f926abf3d9234abf41c2e6`.

Final manager MSVC Debug validation passed 2,124 focused checks and 3,773
natural integration checks, including exact partial-arithmetic outcomes and
full-machine propagation of an explicit child transfer. All 413 asset-free
CTest tests passed (37.93 seconds); progress, recovery, instruction and roster
freshness checks passed. Native input verification captured 126 frames in
`.local/verification/team_select/game-entry-20260906-150735-4b46d757`.
The independent native verifier reconstructs all 10,402 PCs, 2,079 accesses,
and both full CPU callback snapshots; its receipt is
`frames/gameload_entry_verified.json`. Before/after pixel SHA-256 is
`42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7`.
The inspected after frame remains User Setup. Gameplay shown: BLOCKED.
Production InitHeap transport, GAMELOAD main and the live frontend lifecycle
remain required; the synthetic capture is not a running match.

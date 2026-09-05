# FELOAD startup entry recovery

`nba97_feload_entry` owns FELOAD `0x801E1410..0x801E14B7` inclusive: 168
bytes and 42 MIPS instructions. A repository search found only inventory,
decompilation configuration, and caller references for `0x801E1410`; no prior
native owner existed.

The source boundary comes from the fresh read-only Ghidra listing
`.local/ghidra/feload_entry_801e1410.txt`. Hashing its 42 displayed instruction
words as 168 bytes produces SHA-256
`22bb7caff6b8fd97b13608b31ea7af7515c565dac67a989c418496c1818b0716`.
The listing was cross-checked with Capstone. Ghidra's decompiler incorrectly
flows beyond `BREAK 1` into the next routine, so the instruction listing and
inclusive `0x801E14B7` boundary are authoritative.

The owner clears exactly 2,067 words in ascending address order from
`0x801E903C` through `0x801EB084`. It then reads the configured memory top at
`0x801E8B70`, executes the signed trapping subtract-eight at `0x801E1440`, and
maps the result into KSEG0 for `sp`. It reads the reserve at `0x801E8B6C`, uses
wrapping `SUBU` arithmetic to calculate the heap span, stores that span at
`0x801E8B50`, and stores heap base `0x801EB088` at `0x801E8B4C`. The incoming
live `ra` is written over the first cleared word at `0x801E903C`; `gp` becomes
that address and `s8` becomes the replacement `sp`.

The unresolved `0x801E1590` child is called at `0x801E1498`. Its trapping
`ADDI a0,+4` delay slot completes first, so the child observes heap base
`0x801EB08C`. The callback receives and returns a complete 32-register file
with per-register knownness. Register changes from that child remain live. The
owner rereads the mutable saved-`ra` word at `0x801E14A4`; an unknown value is
preserved without stopping because the JAL at `0x801E14AC` replaces `ra` before
the value is consumed. The unresolved `0x801E136C` child then receives the
remaining live state. A nonreturning transfer completes the native boundary;
a return executes the source `BREAK 1` at `0x801E14B4`.

Guest addresses remain 32-bit values and every little-endian word access goes
through validated retained-memory regions. Optional access observations expose
the exact PC, guest address, direction, value, and byte-knownness in source
order. The operation budget counts each attempted guest access and child call,
retaining the completed prefix on a limit, mapping failure, unknown required
input, callback refusal, or trap.

The focused test generates all memory at runtime. It checks both terminal
outcomes, both child boundaries, callback refusal and malformed results, full
register forwarding, delay-slot state, saved-`ra` mutation and unknownness,
signed-ADDI trap boundaries, wrapping heap arithmetic, mapping validation,
native-backing misalignment, unknown bytes, exact access order, and every BSS
clear budget prefix. The separate integration test invokes the recovered
GAMEONLY `0x80029994` main and recovered `0x800AA468` memory-copy owner. A
generated four-byte overlay header is copied from guest heap memory to
`0x801E0000`; GAMEONLY reads `0x801E1410` from that copied header and naturally
dispatches this owner. The integration covers a transferred second child and a
returning second child that reaches `BREAK 1`. No retail bytes or binary fixture
are used.

The existing native input-driven visual verifier now dispatches this owner
from GAMEONLY main at `0x80029BA8`. `src/feload_entry_capture.cpp` keeps its
fixture services and receipt generation outside the application loop. The
preceding 5,136-byte synthetic copy supplies the real entry address in its
header; separate generated globals supply memory top `0x00200000` and reserve
`0x4000`. The owner clears a sentinel-filled BSS, publishes heap span `0x10F70`,
and reaches two explicitly synthetic child callbacks. This is CPU reachability
evidence, not a retail overlay payload or working loader implementation.

The native run emits `feload_entry_trace.json` and immediate before/after
`feload-entry-*.ppm` captures into its ignored run directory. The verifier
checks 2,075 operations, 2,070 stores, three reads, both call PCs and delay-slot
registers, the changed BSS, saved/restored return address, and pixel-identical
scanout; it writes the matching SHA-256 values to `feload_entry_verified.json`.
The routine classification is `Gameplay shown: NO - no direct visual effect`.

# GAME504A8 retained name-UV tail

`nba97_game_body_names` recovers the110 original instructions at
`800505A0..80050754`, stopping at50758 before the function epilogue. It consumes
already normalized ordinary body resources. It does not execute the loader,
CRC, texture upload, name-text generation, GTE/GPU, camera or complete frame.
There are no external callee or successful callback placeholders.

## State and integration

The mutable state reuses `Nba97GameBodyBuffer` and the allocation-relative
reference cells from the frozen body geometry owner. Supply the **whole
ten-player context root F0ED8**, rather than F0ED4's last-side loader cursor.
Each physical context isBCC bytes. Its part9 header reference is+5E4; the
header's+8/+C references select bank0/1. The two name packet references are
bankBase+40 and bankBase+80, with32-bit modular offset arithmetic.

The two fixed sidecars have ten rows and four entries per row. For player`p`,
bank`b`, packet`q`, entry`2*b+q` maps to source polygon word
`FEBFC + p*10 + b*8 + q*4` and center word
`FEDF0 + p*10 + b*8 + q*4` (offsets hexadecimal). Polygon entries retain
allocation identity and offset; they never become numeric host or fabricated
PS1 pointers. The C leaf does not need the source heap addresses.

Only each old **even-index center word** must initially be known. It is a full
raw32-bit input, with knownness supplied by its actual producer. The incoming
odd center, polygon outputs and middle-U bytes need not be known. No source
zero or width is inferred from an uninitialized native object. Center known0
payloads are ignored, and output stores establish new known values. Reference
unknowns retain the canonical`{0,0,0}` representation from the geometry owner.

Clone the retained byte/known/reference-cell arrays together with both
sidecars, rebuild the C buffer views and invoke the tail. Publish the candidate
only on `NBA97_BODY_OK` with completed progress. The C operation itself is not
atomic: refusal leaves its exact supported prefix and diagnostic journal.
There is no resumable cursor. The caller must keep registry/storage metadata
fixed during the call; the native sidecars and byte allocations are disjoint.
Packet/header/context aliases use one canonical allocation identity.

## Source order and preserved bugs

Players run0..9; each player runs bank0 then bank1. The source reloads F0ED8
each bank and rereads the live context+5E4 header after publishing packet0.
All endpoint reads observe the current retained bytes. Shared or shifted bank
views and shared same-side/cross-side headers therefore observe earlier UV
stores; detached precomputation changes the result.

For one pair, the source first publishes both polygon references. It then
reads packet0's unsigned U2 byte(+1C), unsigned U0 byte(+C), and the **old first
center word** at50624. The old word is captured exactly once and retained for
all six later byte expressions. The incoming second center is never consumed.
This source quirk is deliberately preserved and commented in the C code.

Midpoints use the exact floor expression
`U0 + floor((U2-U0)/2)`. Because endpoints are unsigned bytes, `(U0+U2)>>1`
is equivalent. C signed division on a negative odd difference is not. The
first midpoint is stored before reading packet1's endpoints; then the second
midpoint is stored before the six UV writes. Subsequent center and polygon
loads remain in source order, rather than using detached packet values.

| PC | Store |
| --- | --- |
| 505E4,50610 | Packet0 and packet1 references |
| 50648,50684 | Full-word midpoint0 and midpoint1 |
| 50688,506A8,506CC | Packet0+C/+14/+1C: low8(mid0-old), low8(mid0-old), low8(mid0+old-1) |
| 506F0,50714,50738 | Packet1+C/+14/+1C: low8(mid1+old-1), low8(mid1+old-1), low8(mid1-old) |

The saved old first center is used for **both packets**, including the last
three expressions. Every add/subtract wraps at32 bits before byte truncation.
For actual endpoints56/155 and155/56, both centers become105. With old7,
packet0 becomes98/98/111 and packet1 becomes111/111/98. Replacing old7 with a
new midpoint or the incoming second word would be a port defect.

If bank1 aliases bank0, it reads those modified endpoints. With bank1 old10,
the next centers become104 and its output becomes94/94/113 and113/113/94.
The normalizer's natural aliases and explicitly supplied additional aliases
are retained; this module does not invent new loader aliases.

## Refusals and journal

A complete call emits200 stores:40 references,40 center words,120 UV bytes.
`Nba97GameBodyNameWrite.kind` identifies polygon, center or byte storage;
`player/index` identify the sidecar entry, and byte events additionally carry
the exact retained destination. Each event carries its original PC. Capacity
is checked before the corresponding mutation; the journal never silently
drops a performed store. Progress records completed banks/players and the
reached stop PC/location. It is a bounded tail boundary, not a function return.

The shared `NBA97_BODY_*` results distinguish missing known inputs, retained
storage bounds, alignment provenance/traps, malformed metadata, missing
reference cells, unavailable numeric address bits and journal capacity.
The following native boundaries are not attributed to original game checks:

- An unknown old first center stops at50624, after the two polygon stores and
  before either center store. The original unknown-value experiment can still
  calculate midpoints but leaves six UV outputs unknown; it does not justify
  inventing an old value to complete this known-output operation.
- Unknown packet0 endpoints stop at50614/50618 after two stores. Unknown
  packet1 endpoints stop at5064C/50650 after three stores, retaining midpoint0.
- Reached LW accesses require four owned bytes, original word alignment and
  canonical metadata across all four bytes. Reached byte reads/writes check
  only their byte span. Byte accesses do not impose a CPU alignment condition.
  Original base-mod4 must nevertheless be known to locate reference cells;
  lacking it is a native representation boundary, not an LBU/SB hardware trap.
- A byte access into a tagged pointer word refuses with `ADDRESS_REQUIRED`
  (or `UNKNOWN` for an unknown reference). A partial pointer store cannot
  recover the remaining original address bytes from unused raw zero metadata.
- Polygon publication does not dereference or prevalidate the entire32-byte
  packet. Only reached endpoint/store bytes are checked. Stored one-past or
  outside references are not repaired. Unknown reference copies stay unknown.
- Noncanonical metadata is rejected only where reached. Unknown middle-U
  bytes can be overwritten, and untouched bytes retain their old knownness.

## Verification

Public synthetic tests cover all256 low bytes of the old full-word input,
signed-floor midpoints, incoming second-center independence, exact store PCs,
per-byte knownness, unknown reference publication, wrapped offsets, unaligned
byte packets, source aliases and every capacity0..200. Both strict MSVC
`/W4 /WX` Debug and optimized Release configurations pass93,315 checks.

Private `body_names/verify.py` compares the portable C owner against independently
executed original instructions, with branch/load delay behavior. The ordinary
TLST/VATL/WBOS and TLST/VBOS/WATL inputs are normalized first by original50768
and independently checked against the frozen native normalizer. The tail
comparison projects retained references using explicit fixture bases solely
for evidence; these bases never enter public code or claim natural heap state.

Per build,540 completed cases cover all110 tail PCs,1,140,480 original tail
instructions and108,000 ordered stores. Cases sweep all old-center low bytes,
full-word/high-bit variants, endpoint extremes, second-center independence,
shared/shifted banks, same-side/cross-side headers and initially unknown output
fields. Another602 cases cover every pair's missing old-center and endpoint
inputs and all201 journal capacities on both real pairs. Final bytes,
knownness, unchanged reference cells, sidecars and refusal prefixes are
checked against the original ordered writes.

Private source/evidence hashes and exact public-file freeze hashes live in
`.local/verification/native_completion/body_names/freeze.json`. The original
tail SHA256 is
`584a0dcb02fac02e06eaa8eef2ed58b0e5e9eb7f9eba5515eccc978310b984df`.
This task adds no shared build, CMake, Git, host, device or Ghidra changes.

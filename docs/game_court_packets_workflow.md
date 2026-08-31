# Native court packet construction

The native court pass now implements complete GAME `4AC68`, its matrix loads
`55F18/55F44`, quad builders `54D4C/54ED8/54E50`, and tag linker `56914`.
These seven routines total 507 original instructions. The C caller uses the
native C++ integer geometry backend; projected coordinates are calculated,
not supplied by a screenshot fixture or a frontend preview camera.

This is the court portion of the renderer. It does not yet connect the main
executable's User Setup handoff to a match, draw players, advance gameplay, or
rasterize the resulting packets. A passing court build is not a gameplay frame.

## Source order and integration contract

`nba97_game_court_frame` first loads the retained matrix at `F9FD8`, then reads
the court root at `FEBE4`. It publishes the live textured/flat record cursors,
processes the first textured groups into the small ordering table, processes
the final `10 + DCF10` textured groups into depth buckets, then processes flat
groups and their edge packets. Nonzero `DCF10` adds two final special packets.
All counts, bank selectors, visibility bits and cursor reads follow source order.

The access adapter must resolve proven source allocation addresses against
retained native resources. Resources and globals may alias: do not detach
packet arrays, cache a visibility mask for the whole pass, or preflight values
that the source has not yet read. Ordinary aligned accesses use widths 1, 2
or 4; width 3 means only an aligned tag's low 24 bits. Unaligned tag operations
are outside this API's supported domain, not claimed original CPU traps.

The enclosing frame owner must supply the actual court resources, both packet
banks, flat-edge tables, ordering tables and initialized geometry controls.
`4AC68` loads rotation/translation but does not produce the camera or initialize
OFX, OFY, H, DQA, DQB or ZSF4. The backend does not mark unknown controls known
merely because a matrix was loaded. The upstream camera path includes `2DC88`
and `51098/4EA88`; startup camera literals are not evidence of a live camera.

The backend retains vertex inputs, screen/depth FIFOs, accumulators, IR values,
depth bucket and flags across operations. Its projection path is the court's
sf=1/lm=0 integer path, including 44-bit intermediate wrapping, signed clamps,
the existing native UNR reciprocal, fractional screen offsets and FIFO shifts.
Clip determinants and depth averages preserve wide arithmetic before their
documented result truncation. The public hardware register reference is
[PSX-SPX's geometry transformation engine description](https://psx-spx.consoledev.net/geometrytransformationenginegte/).
This is a named arithmetic interface, not a production instruction interpreter.

## Original behavior preserved

- All three quad loops execute once for zero and most negative counts; unsigned
  decrement followed by a signed branch is preserved. A native operation budget
  bounds corrupt or extremely large inputs without repairing the source count.
- `54D4C` prefetches six words of the next vertex record, including after the
  final quad and before culling. Missing ownership refuses; no zero padding is
  invented. Prefetched words survive aliased packet writes.
- The source reads **data register 31 (LZCR)**, not control register 31 (FLAG),
  for its signed rejection check. The port retains this original register choice
  instead of substituting projection error flags.
- Rejected textured packets retain stale coordinates/tags. Flat packets write
  their first three coordinates and read the old tag before the rejection test.
- Perimeter vertices become packet slots `0,1,3,2`. Fixed textured links write
  the ordering-table tag before the packet tag; `56914` writes the packet first.
  Low-24 writes preserve high bytes; the depth/flat full-word stores do not.
- `4AC68` reads the unused fifth flat-helper argument, rereads flat packet and
  edge references after projection, and copies the source-selected diagonal's
  four halfwords before linking each edge. These reads and writes are retained.

Refusal preserves the reached memory and geometry prefix. It is neither atomic
nor resumable. Clone the whole state owner before publishing if atomicity is
required. Unknown or malformed values must not become invented zeros.

## Verification

The public asset-free test covers native projection, all packet types, culling,
tag high bytes, aliases, every helper budget boundary, camera refusal, live
visibility changes, the complete court caller and flat-edge output.

Private Debug and Release verification each compares 1,760 executions against
actual GAME instruction words plus a separate arithmetic reference: 115,753
ordered retained-memory events, all 507 owned instruction addresses, all retained
bytes/knownness and all 64 geometry words agree. There are 662 unknown-input
refusals; each keeps the original reached prefix. Another 86,620 arithmetic
comparisons per build cover wide/edge inputs and exact original court vertices.

The arithmetic reference is isolated in ignored private validation files. No
reference source, game binary, original asset, interpreter or emulator library
is linked into the port or distributed. Reused RAM camera values are explicitly
historical fixtures, not fresh device captures or a natural native match launch.

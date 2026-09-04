# Ordinary player body projection

`game_player_projection.c` owns complete GAME `525AC`, its `54660`, `5483C`,
`54A18`, `54ADC` packet helpers, and `55F0C/55F18/55F44` memory/register loaders.
These are 635 original instructions. It uses actual `50768` normalized body
references and the matrices produced by `5200C/55368`; no six-clip preview model,
default camera, synthetic allocation address, or primitive-success callback is
used. `54608` was inspected but is not called by this owner and is not included.

This closes a packet producer, not `52914`, the frame camera controller, GPU
submission, rasterization, or a naturally entered gameplay frame. The initial
`67468` frame precedes the tipoff/release transition.

## Inputs and retained state

The six input references identify **live source slots**. They are read again at
the original points, so earlier packet writes can change later decisions.

| Input | Source role |
| --- | --- |
| `context_f0ed4` | Current `BCC`-byte player context pointer |
| `bank_1ede8` | Raw packet bank; `(bank << 2)` wraps, with no bank clamp |
| `ordering_102924` | Current ordering-table pointer |
| `mask_1f80000c` | Scratchpad visibility mask read through `55F0C` |
| `index_1029b0` | Physical player index; mask shift uses the low five bits |
| `suppress_dcf10` | Additional alternate-pass suppression word |

The bank and suppression addresses include the signed low immediates of their
original `lui/lw` pairs: **`8001EDE8` and `800DCF10`**, not `8002EDE8/800ECF10`.
Buffers and reference cells use the existing allocation-plus-offset body model.
This preserves shared XYZ, matrix parents, packet banks, descriptor depth words,
and assembled-corner aliases without casting a game pointer to a host pointer.

`addresses[allocation]`, when known, supplies the actual numeric source base.
Only a reached accepted-polygon link needs the packet's numeric low 24 bits.
Missing bases refuse `ADDRESS_REQUIRED`; culled polygons do not require them.
Reached numeric bases must agree with their buffers' source-alignment metadata.
The array is not a guessed heap layout or an automatic raw-pointer decoder.
As in the preceding body owners, consuming raw numeric fields as references
requires explicit reference cells. Partial stores overlapping such a cell
refuse rather than fabricate the preserved address bits.

The caller supplies the retained projection controls and `ZSF3` in
`GamePlayerProjectionGeometry`, whose `root` field is the existing
`GamePlayerRootGeometry`. Copy the actual preceding root state into this field
and copy it back after success **or refusal**. Additional state is
`extra_vertex[4]` (`V1XY/Z`, `V2XY/Z`), `average_scale3`, and `order_depth` (`OTZ`).
All start unknown. The wrapper reuses the court's native RTPT/NCLIP arithmetic
and adds AVSZ3; it does not interpret source opcodes.

For court composition, rotation/translation and OFX/OFY/H/DQA/DQB correspond to
the same court camera fields; root screen/depth arrays correspond to SXY/SZ;
root MAC0/IR0 and vector MAC1..3/IR1..3/FLAG correspond directly; both vertex
arrays and OTZ must also be carried. Court LZCR and ZSF4 are untouched here.
ZSF3 is independent of ZSF4. Raw source `56690/56694` loads `0155` into ZSF3;
the following initialization loads `0100` into ZSF4. That observed initializer
is now owned by `game_gte_initialize.c`; the geometry API still requires its
resulting retained state as provenance instead of silently installing either
value for callers that have not executed that startup boundary.

## Source order and preserved quirks

`525AC` skips primary parts `0,4,10,12,16`. Part 9 projects its first six
triangles through `5483C`, subtracting 12 from OTZ before applying mask `FFF`
and shifting by two. Its remaining triangles go through `54660`. Other selected
parts use `54660` directly. The depth cursor advances by the original raw count.

When not suppressed, the alternate pass visits parts `1,2,3,5,6,7`, loading
their `+44` matrices, alternate parents, and six alternate packet headers.
The primary assembled bank always follows; the mask/index/suppression slots
are read again before deciding whether to assemble the alternate bank.

Confirmed original indexing, count and write quirks are retained and commented:

- `54660/5483C/54ADC` subtract two before their first loop test. Counts zero
  and one execute three triangles; two executes two. Negative/wrapped counts
  retain their actual signed branch behavior. The native owner does not repair
  malformed counts into empty geometry.
- `54660` preloads the next XYZ record before packet stores. `54A18` prefetches
  all three next corner references even on the final iteration. Dead pointer
  contents may be unknown, but the reached spans and metadata are checked.
- `54A18` count zero executes one triangle. It writes the first XY even when
  culling rejects the other two XY stores and the ordering link.
- Primary/biased/assembled links accept NCLIP MAC0 `<= 0`; alternate links
  accept `> 0`. These use MAC0, not FLAG or the court helper's LZCR quirk.
- Primary and assembled links read the ordering entry's low 24 bits, write
  the table's low 24 bits, then write the packet's low 24 bits. Both high bytes,
  including their knowledge, survive.
- Biased and alternate links instead read the full ordering word, write the
  full packet tag (`07000000 | old_low24`), then write the full ordering word
  (`packet_address & FFFFFF`). Refusal at address encoding preserves the
  already-written packet tag. Reversing these writes would break aliases.
- AVSZ3 stores a wrapped MAC0 but clamps OTZ from the full signed product,
  not from the wrapped MAC0. It preserves the original overflow and saturation
  flags, signed ZSF3 and negative rounding.

These are source behaviors, not a claim that every quirk causes a visible game
bug. No inferred rendering correction is applied.

## Native safety and lifetime

Only reached memory spans are checked. Knownness bytes must be canonical zero
or one; the entire reached span is checked even if an earlier byte is unknown.
LW/LWC2 of Z/padded rotation words still checks all four bytes, but only the
consumed low half must be known. Low-24 accesses touch three bytes, so an
unvisited high tag byte is neither required nor rewritten.
The biased/alternate full-word ordering reads still validate all four bytes,
but the following source mask permits an unknown high-byte value.

Ordinary word accesses require original word alignment. This bounded native
low-24 implementation additionally requires the LWL/SWL base to be word aligned;
an unaligned base reports `UNSUPPORTED_ALIGNMENT`, not a claimed original CPU
trap. Numeric reference recovery, arbitrary stack aliases and unsupported
partial reference-cell stores remain explicit boundaries. The source stack is
private and must not alias these retained allocations.

Journal capacity bounds visible stores. Every refusal leaves earlier stores
and geometry changes intact. Clone/rebind **both** memory and geometry to stage
an atomic frame; do not retry a partially executed call in place. Math callbacks
may update geometry only and must not mutate the retained buffers.

## Verification and integration

Private evidence is under
`.local/verification/native_completion/player_projection/`. The raw GAME bytes
are SHA-256 `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
The fresh `525AC` Ghidra export and independent complete raw listings are kept
separately; undefined `54608` was not manufactured into the shared project.

Strict MSVC Debug/Release and GCC C99/C++17 tests cover actual projection values,
count boundaries, exact store refusal prefixes, split XYZ/corner resource
extents, missing camera/ZSF3/address inputs, partial high-tag knowledge,
opposite winding, and wide AVSZ3 overflow. Independent private arithmetic
compares RTPT/NCLIP/AVSZ3 across all 64 retained register words. The raw CPU
comparison executes every one of the 635 owned instructions without callee
hooks and checks complete buffers, ordered stores, register state and refused
prefixes, including live visibility and ordering aliases.

The frozen results are 247 public checks in each strict build, 1,128 original
CPU comparisons per Windows configuration (650 refused prefixes, all 635 PCs),
60,000 independent arithmetic cases and 13 additional knowledge probes per
configuration. The 80 actual-resource cases compare 111,648 projection stores
per configuration.

The actual-resource comparison normalizes all four VATL/VBOS/WATL/WBOS files
through original `50768`, verifies actual `640D8/530FC` motion poses, obtains
root/part matrices through original `5200C/55368`, then compares native and
original `525AC` for both packet banks. Camera, scale, addresses and retained
OT state are explicit fixture inputs; actual ZSF3 `0155` is used. This is source
and arithmetic evidence, not a fresh console capture or a natural frame.

The new C/C++ files and standalone test are registered in CMake, linking the
existing player root/geometry and court geometry arithmetic. Both the core
tests and Windows application compile them; live match entry is unchanged.
The next caller
work is `52914`'s context/entity/view setup and the actual camera/control/ordering
producers. The resulting body packets must join the same ordering table and
retained renderer state as the court; they must not be drawn as preview meshes.

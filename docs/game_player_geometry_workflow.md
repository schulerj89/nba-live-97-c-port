# Original player part matrices and hand endpoints

`game_player_geometry.c` owns complete GAMEONLY `55368..55EFC`: 742 original
instructions, with no external CPU callee. `GamePlayerGeometry` supplies its
named fixed-point rotation and translation operations. This is the actual
twenty-part player path called by `52914` at `52B24`, after `5200C`. It does not
project body vertices or claim a rendered or playable match.

The hand positions are endpoint-chain results, **not projected vertices**.
`55368` writes signed X/height/Z halfwords into `FAA04` and `FED20` through
the live `F0FB4` and `FC62C` references. The recovered `2D37C` consumer selects
between these tables using entity `9A` bit0, shifts the values left five, and
adds entity height. These outputs therefore connect the actual ordinary body
pose to the existing tipoff and ball-release owners. Do not substitute preview
six-clip poses, projected screen coordinates, or inferred skeleton feet.

## Retained inputs and ordering

The C input uses existing `Nba97GameBodyBuffer`, reference cells, and allocation
identities from the ordinary `50768` body owner. It needs no invented original
heap addresses. A field named after a global is a reference to the **slot**,
not a snapshot of that slot's value. This preserves aliases and source read
ordering. The actual `52914` mapping for physical player `p` is:

| Slot | Stored reference |
| --- | --- |
| `F0ED4` | Current model context, advancing by `BCC` |
| `10292C` | `103FD8 + p*32`, camera-composed actor rotation |
| `F1C4C` | `FB010 + p*32`, first lower-limb chain |
| `F9CF8` | `FB2E4 + p*32`, second lower-limb chain |
| `F9C54` | `FB17C + p*32`, first upper-limb chain |
| `F9D00` | `FB480 + p*32`, actor world matrix/second upper-limb chain |
| `F9D04` | `FB430 + p*8`, first foot endpoint |
| `FEA38` | `1028C8 + p*8`, second foot endpoint |
| `F0FB4` | `FAA04 + p*8`, first hand endpoint |
| `FC62C` | `FED20 + p*8`, second hand endpoint |

`103EDC` is the actual extra part11 angle. `52914` derives it from signed entity
halfword `+98` at the physical `FC654 + p*F4` record, multiplied by four. Its
position and yaw reads use the separate `FC650[p]` reference table. Those two
sources must not be collapsed. Context `+4/+C/+8` holds X/height/Z; context
`+BBC/+BC0` holds the primary and secondary pose references supplied by the
actual `530FC` sampler. `55368` starts with secondary `+4`, switches to primary
at part8, and consumes six angle bytes per eight-byte entry. Marker bytes and
the secondary root header remain separate incoming data.

For each part, the owner constructs the local Euler matrix from actual packed
GAME `B3254` trig words, combines it with the camera-composed actor rotation,
and stores five words at context `+24 + part*94`. It also computes a temporary
world rotation against `FB480`. Primary parent translations come from the
`50768` reference at context `+A4 + part*94`; pivots come from `+AC`. The first
eight parts also use the alternate parent at `+A8` and write the mirrored
rotation at `+44`. Source endpoint positions use full MAC1/2/3, not saturated
IR values. Parts3/7 publish foot offsets; parts15/19 publish hand offsets.

The trig input is a retained view whose offset zero is original `B3254`.
The normal resource contains4096 packed little-endian words (16384bytes).
Only reached indices are checked; no synthetic sine table or whole-resource
magic-value preflight replaces the original reads.

## Preserved source behavior and native guards

* `55448..5567C` uses unsigned multiply/low32 then arithmetic shift12, followed
  by halfword truncation. Part11 adds full `103EDC` and then truncates its first
  angle. It does not normalize to a preferred angle range.
* `55988..559A8` negates exactly local matrix elements3,4,8 for the mirrored
  first eight parts. It is not a general mirrored skeleton algorithm.
* `5570C..55744` and `55A80..55ABC` store matrix words in offset order
  `0,C,4,8,10`. The last SWC2 writes a full sign-extended IR3 word, overwriting
  matrix padding. Endpoint stores use full32-bit MAC output separately.
* `55DCC..55DE4` writes the part9 endpoint to the second upper-limb chain and
  then the first. Aliased chains retain that exact order. Hotpoint subtraction
  wraps32 bits and its stores truncate to signed16; there is no range clamp.
* The original indexing quirk at `5594C` prefetches the next parent even after
  completing part19. A context-only `BCC` allocation does not contain that
  extra slot. The native owner refuses at that reached read, with all293 prior
  stores retained; it does not silently remove the access. The loaded pointer
  is dead on the last iteration, so unknown/raw pointer contents are allowed
  after its bounds, alignment, and canonical metadata checks.

The new guards protect native storage; they are not repairs to original game
logic. Whole reached spans require canonical0/1 byte-knownness even on writes.
Unknown raw bytes are never zero-filled. An LW followed by CTC2 rotationword4,
or an LWC2 to V0Z, validates the whole aligned four-byte span but requires only
the consumed low16 bits to be known. Their discarded high halves remain
unknown. Unread padding is not preflighted. A scalar read of a reference cell
requires actual original encoded address bits. A halfword store into a reference
cell likewise refuses `ADDRESS_REQUIRED`, because a native allocation id cannot
reconstruct the preserved encoded half. These refused domains are explicit.

Unknown or unresolved pointer values survive their load until dereferenced.
The API preserves all completed memory stores and math effects on failure;
`parts_completed` does not mean an atomic or resumable transaction. Journal
capacity is checked before each visible store. Callers requiring atomicity must
clone both memory/cells and geometry state. Native stack scratch is disjoint
from the owned allocations; source stack aliases require a different entry.

The C++ geometry state retains exactly five rotation words, three translation
words, V0XY/Z, IR1..3, MAC1..3, and FLAG, with explicit knownness. MVMVA uses
signed44-bit intermediate wrapping, shift12, full32-bit MAC storage, signed16
IR saturation, and original overflow/summary flags. It changes no other GTE
controls, screen/depth FIFOs, IR0, MAC0, or LZCR. Import/export this touched
subset when sharing geometry with the court owner; do not reset the other
state or mark the whole camera known merely because a rotation was loaded.
The callback contract permits geometry-state changes only, not hidden mutation
of input buffers or fabricated GPU success.

## Verification

Private evidence is under `.local/verification/native_completion/player_geometry/`.
The original GAME image SHA256 is
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
The742-instruction range SHA256 is
`711e1a9ee669d2dafdc5c256e487cb0f06ec5532c7d8b7f3de5738775de6640c`.
A fresh read-only Ghidra export covers55368,5200C,51F18; every prior55368
instruction word was independently checked against the raw image, and the
fresh export contains the same full instruction span. No writable analysis
project or original debugger execution was used for this proof.

Both strict MSVC Debug/Release and standalone GCC11.4 builds pass43,476 public
checks. These include all293 journal cutoffs, exact mirrored/endpoints, deferred
unknown pointers, a short context's final prefetch, discarded high-half
knownness, canonical metadata poison, aliases, and unsupported partial encoded
pointer stores. The tests require no original assets.

Each MSVC configuration additionally passes:

* 734 unhooked original55368 comparisons, including473 refusal prefixes,
  145,214 visible stores, and all742 source PCs. Every retained byte and all64
  geometry register words match. Pointer numeric projection is proof-only.
* 40,000 independent fixed-point arithmetic cases against the pre-existing
  private DuckStation implementation, with all64 register words compared.
  No reference code is linked into production.
* 32 additional partial-known probes: original retained output stays identical
  with discarded matrix/pivot high halves unknown, while later noncanonical
  bytes in those reached spans refuse before the corresponding geometry load.
* 80 compositions using all five players from actual `ZDOMVATL`, `ZDOMVBOS`,
  `ZDOMWATL`, and `ZDOMWBOS` resources, plus original ZMOCAP clips77/78/79/0
  and alternative blends. Original640D8 normalizes the resource, original530FC
  and its callees produce the poses, the native sampler matches40 poses, and
  original55368 matches all23,440 composed stores and geometry state. Explicit
  root/work/camera inputs remain fixtures; this is not a natural frame capture.

## Integration and remaining visible-player path

CMake now includes the C and C++ sources in the native application and the
asset-free `game_player_geometry_tests.cpp` target. They include the existing
body/period type headers but do not need to link the body normalizer itself.
Use `GamePlayerGeometry::callback` and a retained `GamePlayerGeometry` instance
as the input math implementation. The production path never executes source
CPU opcodes.

This closes55368, not the whole52914 caller. Its smallest immediate missing
producer is complete `5200C` (134instructions), which builds actor world/root
matrices and center projection before55368. It requires `56080` Euler creation,
`51F18` scale, `562CC` matrix composition, `56650` vector transform, `51F04`
mirror, `56624` projection, and the already-owned `55F18/55F44` control loads.
Its inputs include actual camera `F9FD8`, per-actor scale `105F48`, context yaw,
context position, and secondary root height. These must be produced or passed
with explicit provenance, never replaced with frontend preview defaults.

After55368,52914 tests the projected actor center and calls525AC to project
body vertices, then additional body packet/overlay owners. Court projection
alone does not close those routines. The initial67468 frame runs this render
path before the tipoff release transition, so closing the remaining actor root
and body projection owners is required for an honest initial player frame.

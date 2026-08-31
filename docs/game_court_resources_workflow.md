# Original court geometry resource setup

`game_court_resources.c` owns the after-load `48A4C..48D28` tail of GAME
`479B8`. It consumes the actual resource address returned by `29BFC`, relocates
the court headers and packet banks, allocates both edge-list banks, and creates
their line packets. It calls the shared native `90160/901EC` allocation wrapper
and implements the reached zero-argument arm of `9C274`.

The tail has 183 instructions, the allocation wrapper and lock helpers have 50,
and the reached flag-clear arm has seven. All 240 are exercised by private
original-instruction comparisons. This entry does not own preceding file
selection/loading, full479B8, court textures, a live camera or a connected
gameplay frame. The [texture loop](game_court_textures_workflow.md) is separate.

## Actual allocations and source order

Inputs use the existing retained source-memory model. Source addresses require
provenance; a native pointer or diagnostic address does not establish the
original heap layout. Region descriptors stay fixed, source regions do not
overlap, and byte knownness remains explicit. Native backing storage may alias
under the existing memory contract. Only reached bytes are checked.

The owner publishes `FEBE4`, fills resource header offsets `0C/10` with group
tables, walks both packet banks for every group, and publishes the vertex stream
at header `08`. Group headers are 16 bytes; textured packets are 40 bytes and
flat packets 24 bytes. It retains live `102C84/FC964` cursors and the original
`10B60C` group-count calculation, including wrapped addition of `DCF10 + 1`.

The two `FEDA0/FEDA4` pointer lists are allocated separately. Every flat group
then receives a separate 16-byte line-packet array per bank. Allocation runs
the concrete `90160` wrapper, including its live lock-pointer rereads and
unguarded descriptor-word dereference. Court allocations pass flags 0; the text
caller passes 20. The shared wrapper now exposes that argument explicitly.

The `9027C` callback must perform actual allocation effects. Tests and private
comparisons compose its existing native owner directly, including heap search,
free descriptors, serial numbers, list insertion and names. Returning a guessed
pointer or merely calling host malloc does not fulfill this contract. Payload
bytes are never implicitly cleared. BIOS name copying and reclaim/service
behavior retain their own boundaries.

## Preserved behavior and refusal

The source reads each group count twice before its first relocation store;
both bank extents retain those captured values. Later counts, cursors, lists
and resource globals are reread at the original points. Signed loop tests and
32-bit wrapping remain unchanged; malformed counts are not repaired.

Even zero flat-group counts still reach both list allocations. Selected groups
reach both per-bank allocations before deciding whether to initialize lines.
Only tag length 3 and command/RGB bytes are initialized; low tag links, XY and
unused bytes remain incoming. Each RGB read precedes its individual write,
preserving overlapping source/destination effects. The blue store is a call
delay slot. `9C274` then rereads command byte 7 and clears bit 1 in its return
delay slot. These source quirks are not inferred gameplay fixes.

The journal includes tail and allocation-wrapper writes/callback events; the
allocator retains its own internal journal. Refusal keeps preceding effects,
including relocations before allocation failure. The owner is not resumable or
transactional. Atomic callers must clone complete memory and allocator state.
Journal/access budgets are native bounds, not original limits. Code and source
stack aliases remain outside this adapter's supported domain.

## Verification

Evidence is private under
`.local/verification/native_completion/court_geometry_resources/`. A fresh
read-only Ghidra export matches original GAME image SHA256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
Strict MSVC Debug/Release and GCC builds pass 2,161 public checks covering real
heap composition, forward allocation, both banks, store prefixes, changed
counts, missing data, source alignment and untouched payload knownness.

Each MSVC configuration passes 217 original CPU comparisons covering all 240
tail/wrapper/clear-arm PCs and 379 distinct PCs through the reached heap chain.
They compare 30,232 ordered events and every retained byte/knownness value
across 393,300 instructions. Cases include randomized groups, every journal
cutoff for a complete fixture, mutable allocation state, unknown fields,
signed/wrapped counts and actual `ZDOMYATL` and `ZDOMYNEU` resources. Only BIOS
`strncpy` uses an external fixture; tail, allocation wrapper and heap allocator
execute their original instructions without successful-call substitutions.

Both real courts complete six allocations. Atlanta has 3,736 tail/wrapper
events and the neutral court 2,359, each with another 72 heap events. These
counts include callbacks, not just stores. Normalized resource and heap-region
snapshots preserve exact knownness for subsequent native packet generation.
Heap layout and service state are explicit fixtures, not cold-entry evidence.

The text-pool caller is separately replayed after exposing the wrapper to
verify unchanged flags20 behavior. The standalone heap regression is also
retained; strict-GCC formatting changes do not alter allocation semantics.

The remaining integration needs real resource loading and heap initialization,
camera/frame setup, player/ball rendering, simulation and actual match entry.
Rendering a supplied-state diagnostic does not establish those callers.

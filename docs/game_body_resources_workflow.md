# Retained ordinary player-body resources

`GameBodyResources` owns the ordinary TLST context payload and both V/W team
payloads through `GameRenderMemory`. It binds the actual two50768 normalizer
calls: home physical players0..4, then away5..9. The source behavior stays in
`game_body_geometry.c`; the C++ layer manages allocation lifetime, typed views
and publication. It does not decode the FATL Create Player preview format.

## Inputs and ownership

Inputs must be the accepted logical loader payloads, excluding disc trailers.
The caller supplies each allocation's original base modulo4, or explicitly
leaves it unknown. Native heap alignment cannot establish original alignment.
This boundary does not execute29BFC, verify the resource checksum, or establish
the natural game's heap placement. It reads each side's two count words just
before the corresponding50768 call; it does not cache away counts before home.

The five retained allocations are contexts, home data, away data, and the two
320-byte root arrays. The arrays have known identities but unknown contents;
50768 stores references to them without initializing their matrices. A camera
or pose owner must establish those contents before consuming them. No default
identity matrix or plausible coordinates are inserted.

Every allocation owns its bytes, byte-knownness mask, and parallel reference
cells. A reference contains an allocation ID and a32-bit offset. Pointer cells
carry that reference; their raw zero bytes are unused metadata, never source
NULL pointers. Both sides retain shared vertex data for their five players
and separate polygon banks. The source's use of bank A's captured group for
both banks is preserved, including disagreement with B's encoded group.

`buffer()` rebuilds a C view from its owner. `referenceAt()` requires a tagged
reference cell and never promotes a serialized numeric word to a pointer.
`knownBuffer()` supports existing fully-known byte consumers: it rejects
unknown bytes and every overlap with a reference cell, including a partial
word. Caller-held views must be rebuilt after copying or moving an owner.

Copies duplicate bytes, masks and reference cells while keeping allocation IDs
and all internal aliases consistent. Copy assignment first constructs a full
candidate, then moves it into place, so allocation failure cannot mix bytes
from one generation with reference cells from another. Moved-from owners are
not valid view producers. There is no fake32-bit host-pointer conversion or
emulated RAM allocation.

## Refusal and publication

The native entry contract requires canonical0/1 knownness metadata throughout
each input. `GameRenderMemory::add` rejects malformed markers before either C
call. This allocation-construction precondition is distinct from the C owner's
reached-span checks; source-prefix equivalence is not claimed for malformed
metadata rejected at entry. Canonical unknown0 bytes remain supported and
cause a refusal only if a required read reaches them.

One caller-supplied capacity bounds the combined ordered-write journal. A
complete candidate is published only after both C calls complete. Refusal
retains the diagnostic journal of the executed prefix and exposes no partial
resource owner. This native publication policy is not source rollback: the
original normalizer mutates its allocations in place, and the C owner retains
those effects within the unpublished candidate.

Native ownership errors must not become gameplay corrections. Original count
wrapping, signed loops, unbounded group offsets and stale fields are left to
the recovered C behavior. A required reference outside retained storage is an
explicit native boundary, not evidence the original would reject the input.

## Verification and remaining work

Public tests pass649 checks in strict MSVC Debug and optimized Release builds.
Independent review adds14 ownership checks per build: clones outlive their
original, deep masks/cells remain distinct, copy/move assignment preserves
aliases, and invalid views refuse.

For each build,36 independent original-instruction comparisons execute both
50768 calls using the actual C++ owner. They include both real TLST/VATL/WBOS
and TLST/VBOS/WATL pairs, unknown context bytes, and32 synthetic pairs. The
comparison checks247,740 ordered stores and3,670,696 projected bytes and
semantic byte-knownness states after1,922,890 original instructions. Fifteen
additional cases check capacity limits, unavailable inputs and publication.
Private fixtures use declared address projections solely for comparison;
these are not production heap addresses.

Evidence and reviewed file hashes are in
`.local/verification/native_completion/body_resources_review/`. The underlying
C normalizer has its own wider instruction, alias and corner-case proof in
`game_body_geometry_workflow.md`.

504A8's name-UV tail, original resource loading, animation/camera integration,
GTE processing, GPU ordering and rasterization remain separate owners. Linking
this resource owner into the host does not establish a visible court, a player
frame, or a playable match.

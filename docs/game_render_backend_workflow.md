# Native render transfers and retained allocations

`game_render_backend.cpp` supplies native C++ storage and synchronous transfer
operations for the recovered render owners. Header walking, split uploads,
coordinate writes, temporary-height changes and the upload-pending word remain
in `game_image_upload.c`. No emulator is linked into the application.

This backend is not yet wired to a playable match or a rasterizer. It closes
the raw storage operations needed by player texture setup and the head cache,
without treating a callback receipt as proof that pixels were transferred.

## Storage and staging

`GameVramWords` owns 1024 by512 raw16-bit words and one knownness flag per word.
An initially zero-valued allocation represents unknown VRAM, not a source
clear. Uploads preserve every bit, including bit15. Readback serializes actual
words as little endian bytes. Disjoint moves copy both words and knownness.
Reading unknown words refuses; moving them never turns them into known zeros.

`GameRenderMemory` owns fixed-size byte allocations. Each allocation retains
optional per-byte knownness and explicit original address-modulo4 provenance.
Native pointer alignment is not evidence about original MIPS alignment.
Enclosing allocations are retained so a signed image-header link can reach
earlier bytes. A subview remains restricted to its declared envelope.

Many buffers may alias the same allocation. A copied registry deep-copies each
allocation once. `rebind()` redirects original views into that corresponding
copy and preserves overlap. Per-allocation identities prevent independent
post-fork additions from being confused just because their numeric IDs or sizes
match. No allocation can be resized or freed while a recovered owner uses it.

The older render C structs do not carry byte-knownness metadata. Their resource
views must come from `knownBuffer()` or an equivalent whole-span proof; exposing
an unknown resource directly to those owners is not supported. Readback uses a
mutable descriptor and establishes knownness only for bytes actually written.

Copy a `GameRenderBackend` to stage its VRAM, CPU allocations and upload state
together. Also copy the recovered C state and explicitly rebind every borrowed
resource view before running it. A copied C struct alone still points into the
original memory and is not a transaction. Only publish the candidate after the
complete caller succeeds. Refused candidates retain their source execution
prefixes for diagnostics; they are not resumable cursors.

The service callback and context are external. They are not cloned, rolled back
or made transactional by copying the backend. The caller must supply any
appropriate candidate service context and serialize access. After publication,
rebuild old borrowed views rather than retaining pointers into retired storage.

## Supported SDK transfer domain

The bridge supports positive, in-range word rectangles under explicitly proven
unmasked GPU transfer state. This is not a default assumption. Mask-bit effects,
GPU wrapping and overlapping moves, including exact self-copy, remain explicit
unsupported domains until independently verified.

GAME9971C and99780 dispatch to SDK handlers9AC7C and9AED0. Those handlers clamp
dimensions against the signed16 limits C55C4/C55C6. The bridge requires known
positive limits and a requested rectangle that does not need that clamp. It
refuses a request exceeding them instead of silently assuming1024/512 or
implementing an unverified malformed-rectangle behavior. The raw word-plane
class itself is a bounded storage primitive; these SDK requirements belong to
the render bridge.

The handlers consume or write `ceil(width*height/2)` CPU32-bit words. An odd
pixel count therefore consumes a padding halfword on upload. The backend checks
its bounds, alignment and knownness but does not invent an additional VRAM
pixel. The value of an odd readback's extra halfword is not yet established;
that domain refuses. Actual head-cache readbacks38x48 and256x1 are even.
The handlers process a remainder with CPU word loads/stores and blocks of16
words through DMA. Misaligned remainder accesses are source CPU alignment
traps; misaligned transfers consisting entirely of DMA blocks remain an
unsupported domain, without falsely identifying them as CPU traps.

GAME997E4 packs both destination coordinates to their low16 bits. Its source
rectangle remains raw signed16. Either zero dimension returns before GPU
dispatch, and the recovered render callers ignore that SDK return; the native
bridge completes that no-write boundary. Nonzero unsupported rectangles and
overlap explicitly refuse. This preserves the wrapper's zero case without
guessing the GPU's zero-dimension command encoding.

All accepted native transfers consume their inputs synchronously. The native
sync boundary therefore has no queued transfers to drain. It does not write
D7B14: source994F4 does not clear that flag. The separate9446C wrapper owns that
clear. Original debug callbacks, SDK timeout/queue bookkeeping and physical GPU
timing are not claimed as reproduced by this synchronous storage backend.

SERVICE8892C is the CD/service boundary, not GPU synchronization. It requires a
real external callback and refuses when none is supplied. A successful upload
callback means actual CPU words were copied into native VRAM. A successful
store callback means the owned destination was filled from known VRAM.

## Original quirks and verification

The implementation never repairs source image headers on failure. For example,
if946B8 completes its two tail uploads, decrements the image height, uploads the
main region, and then reaches an unowned next header, the temporary height and
all preceding VRAM writes remain in the candidate. Source restoration occurs
only on its later instruction. A whole-candidate refusal leaves live state
unchanged without misrepresenting source execution as atomic.

Public tests compose the actual38A18/3875C head-cache owner with the actual
946B8 upload owner and native readback/move/upload operations. Every word of two
38x48 head regions and their256-color palettes is checked after the exchange,
including the source6AF7 overwrite of palette entry255. The CD callback in that
test is an explicit fixture, not a production service implementation.

Additional tests cover little endian words and bit15, edge rectangles, CPU
padding and original alignment, unknown data, allocation alias topology,
independent fork additions, signed backward palette links, split-upload pixel
placement, pending-flag ordering, source prefixes on failure, SDK-limit refusal,
and128 scattered rectangular transfers against an absolute-coordinate oracle.
These tests do not establish retail cold-entry provenance, texture sampling,
geometry, frame output, input, possession or a complete playable match.

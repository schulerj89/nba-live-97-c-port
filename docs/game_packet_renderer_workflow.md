# Native packet drawing

`GamePacketRenderer` draws retained GP0 packet words into `GameVramWords`, the
same explicit 1024 by 512 word plane used by recovered image uploads. It walks
actual low24 ordering-table links through a caller-supplied allocation reader.
No CPU, recompilation runtime, bus, timer, or emulator dependency is embedded.
The implementation is project-authored; private reference implementations are
used only for verification and are not distributed or linked into the port.

This is a drawing component, not a connected match renderer. `UserConfirmed`
still defers match entry. Actual resource uploads, initialized ordering tables,
live camera controls, player projection and the complete frame caller remain
integration requirements. The court composition test uses explicit camera and
resource fixtures, not a natural gameplay capture.

## Supported component behavior

The backend consumes flat and Gouraud triangles/quads, single lines, textured
and untextured rectangles, drawing-environment words E1 through E6, fill, NOP
and cache-clear commands. Quads use the original strip order. Triangle coverage
uses integer edges; color and UV gradients retain twelve fractional bits and
the initial half-unit bias. Drawing offsets, inclusive drawing-area clipping,
display exclusion and explicit interlace field state are retained.

Texture reads use actual packed 4-bit, 8-bit or direct 15-bit VRAM words,
texture pages, texture windows and CLUT coordinates. Zero texels skip drawing;
the texture mask bit remains separate from transparent zero. Modulation,
dithering, four transparency modes and destination mask checks operate before
the final word is published. Half transparency halves the combined channel
sum once; independently halving two odd channels is a native rounding defect.
`drawWord` establishes exactly one known VRAM word without implicit clearing.

All six environment words and display state must be explicitly known before
ordinary drawing. Missing packet words, texture data or required destination
data refuse. Unsupported commands and 2MiB modes refuse. Unknown memory is
never treated as black, a replacement texture or an invented packet terminator.
Fill retains its independent alignment, wrapping and environment behavior.

Commands may cross linked packet boundaries. Link and pixel budgets bound
native execution without repairing a cyclic source list. Failure retains prior
pixels and state; a successful prefix is not a completed frame. Atomic callers
must clone VRAM and construct a renderer bound to that clone, then copy retained
draw state. Copying a renderer object alone does not rebind its VRAM reference.

## Verification and remaining limits

Private evidence is under `.local/verification/native_completion/packet_renderer/`.
Both strict MSVC configurations independently match:

* 104,096 scalar shading cases, including every pair of 5-bit channel values
  in all four blend modes plus randomized masks, modulation and dithering.
* 8,000 flat, Gouraud, raw-textured and modulated-textured triangle cases, with
  clipped coordinates, both windings and per-pixel output comparisons.

The comparisons caught and corrected native blend rounding and unquantized
attribute interpolation defects. Reference bodies remain private and their
hashes are recorded. The asset-free public tests cover explicit refusal,
packet ordering, masks, textures, gradient regression cases and native limits.
The court tests compose recovered projection and link writes directly with
this renderer for both a flat and a textured quad, checking every output pixel.

These are bounded software-reference comparisons, not complete hardware or
gameplay fidelity acceptance. Arbitrary single-line tie breaking, extreme
coordinate/offset wrapping, all texture-window/cache interactions, overlapping
render-to-texture, and hardware timing remain separate gates. Polylines and
transfer packets are unsupported here; recovered transfer owners remain the
separate upload/readback path. Texture or CLUT reads from a word written earlier
in the same batch explicitly refuse until cache behavior is recovered. Starting
a new batch is not evidence that the original texture cache was invalidated;
callers must not claim feedback parity across batches. Raster failure traversal
order is a native diagnostic contract, not a GPU cycle trace.

The packet fields are documented in the [PSX GPU reference](https://psx-spx.consoledev.net/graphicsprocessingunitgpu/).

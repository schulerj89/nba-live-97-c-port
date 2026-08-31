# Original court texture loop

`game_court_textures.c` owns the `487B8..48894` texture loop inside GAMEONLY
`479B8`, with `A3FE0/A3FEC` count and image lookup. It starts with an already
loaded SHPP container and stops before the subsequent `994F4` synchronization
and `90698` free. It does **not** own complete479B8, file selection, loading,
court geometry relocation, a natural camera, or a connected gameplay frame.

Each image reaches the existing complete `946B8` image owner and its actual
transfer callback. The CMake test and Windows application compile the new C
owner; the host still defers match entry after User Setup. No emulator,
recompilation runtime, fallback atlas or replacement texture is introduced.

## Retained state and source order

The caller supplies the actual enclosing image allocation, its byte knownness
and original address-modulo4 provenance. Native pointer alignment does not
establish original MIPS alignment. The returned state retains the upload
pending word and the actual palette reference published at global `FED1C`.
Numeric heap addresses are not invented to represent that pointer.

The source rereads the signed image count at each loop condition, then rereads
it in the unsigned entry lookup. A nonpositive count exits without reading an
image. Entry offsets come from the original eight-byte table records. Callbacks
may change retained image contents; later count/header reads observe those
changes rather than a precomputed image list.

For four-bit images, the palette uses the header's signed24 relative link. The
source reads its signed16 width before publishing the palette pointer. Widths
greater than16 are clamped to16; negative widths are not repaired. Publication
and any width change precede the image's X/Y reads. Palette placement starts at
512,252, advances X by16, and moves one row upward after each16 entries. Other
formats use a separate cursor starting at512,240 and advancing downward.
Image X retains its low14 bits and image Y is the original byte.

These ordering and arithmetic quirks are preserved and commented. The native
guards refuse unknown reached bytes, missing storage, unsupported alignment or
upload domains. They do not preflight an unreached resource tail. A refusal
retains prior palette/header writes and actual transfers, including nested
image-owner effects. Image and header budgets bound native execution without
claiming that the original game imposed those limits.

This entry is not resumable or atomic. For atomic publication, clone the entire
resource allocation, C state and native backend, rebind borrowed views, and
publish only after the complete caller succeeds. Descriptor lifetimes remain
fixed. State/progress/metadata cannot overlap resource byte arrays; aliases to
the original stack/global slots and address-space-wrapping allocation views
are outside this adapter's domain. These exclusions are not original guards.

## Actual resource comparison

Private evidence lives in
`.local/verification/native_completion/court_resources/`. A fresh read-only
Ghidra export was checked against original GAME image SHA256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
The loop and lookup span68 original instructions. The comparisons visit66;
the two lookup NULL-return instructions are unreachable from this synchronous
entry with fixed metadata: the preceding signed loop condition establishes a
nonnegative index below a positive count, and no mutation boundary lies between
that read and the lookup's reread. This is not a claim that every entry into
`A3FEC` can never return NULL.

Strict MSVC Debug and Release each pass27 public checks and223 private original
CPU comparisons, including count mutation, image-budget cutoffs, unknown bytes,
transfer refusals, signed widths and palette cursor wrap. Each private run
compares4,280 ordered transfer events, all retained resource bytes, palette and
pending state, and complete VRAM words/knownness. Only the transfer boundary is
external to the original CPU owner chain; image handling executes original
instructions through all its owned callees.

The actual `ZDOMXATL` logical container contains84 images. All84 complete with
246 transfer events,82 palette publications, no width clamps,23,556 known VRAM
words, pending1 and the final palette at resource offset50,440. Original CPU
execution into a separate flat word plane, the native C owner into that plane,
and the C owner with the real `GameRenderBackend` produce identical image
bytes, VRAM and knownness in both configurations. The explicit fixture provides
unmasked transfers and SDK limits1024 by512; these are not claimed as recovered
cold-entry device state. Unknown VRAM outside actual writes stays unknown.

One actual texture requests a zero-width upload. Original SDK `9AC7C` clamps
nonpositive dimensions to zero and returnsFFFFFFFF before pixel/GPU access;
the image caller ignores this return. The backend now handles this no-data
boundary without inventing a transfer. Another729 original-instruction cases
verify that path; deadline initialization, queue timing and hardware operation
remain outside the synchronous contract. See the
[backend workflow](game_render_backend_workflow.md).

These results establish the texture component, not a rendered natural match.
The remaining court path needs the real geometry resource normalizer, frame
ordering tables, live camera/state producers and the complete original frame
caller. A diagnostic camera or successful image upload cannot replace them.

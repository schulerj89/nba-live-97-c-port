# Native body names and texture binding

`game_body_names.cpp` connects the retained ordinary player resources to the
original504A8 name-UV tail and4E3CC name-texture owner. C++ owns the resource
views and knowledge of the saved center words; the portable C owners still
perform all original UV, width, glyph and upload decisions.

## Original order

The original ordinary startup runs names before body geometry. In52C20,
52F68 calls4DC08. That owner setsF0F68=1 at4DC90, copies each name's image
header/placement intoFCD78, and calls4E3CC at4E27C. The bypass branch writes
the calculated halfwidth into all fourFEDF0 words for that player without
reading any polygon pointer. 4E394 clearsF0F68 on exit. Only later,53048
calls504A8; its post-loader tail consumes the even center words as widths.

After52C20 returns,48FB0 sets21498=1 and48FB4 calls63EDC. Its64098 call to
4D9EC refreshes the name textures with bypass cleared. These are different
uses of4E3CC, not interchangeable calls in an arbitrary order. The manager's
private `render_name_order_manager` evidence retains the source ranges.
The later refresh retains the last initial name template's scratch header;
the adapter does not reset it to a new default image between the two phases.
The adapter does not itself implement52C20/4DC08 or establishF0F68 provenance.

## Retained state and views

`GameBodyNameState` contains the40 allocation-relative polygon references
and40 center words with knownness. There are no cached native pointers in
this state. It must be paired with its own `GameBodyResources` or a matching
deep copy. The body owner preserves all within-allocation aliases.

`renderGameBodyName` allows a null body on the proven bypass path. Unknown
incoming widths are valid because4E3CC overwrites each center before reading
it. The new `nba97_game_render_name_tracked` entry adds a four-bit write
receipt after the actual center stores; it shares the entire implementation
with the original public entry. This receipt lets the adapter import only
executed stores after an early refusal. Native zero initialization does not
make the untouched widths known.

On a non-bypass call, the adapter rebuilds all four packet views from retained
body references. Each view covers the29 bytes through the last accessedU
coordinate. The legacy byte-only C consumer requires those views to be fully
known and contain no pointer cells. This is a native entry-domain check, not
an added original branch or a claim about source execution prefixes for
unsupported packets. Other glyph/player/scratch resources are caller-owned
and must satisfy the original C interface's fully-known storage contract.

Once bound, source writes modify the actual retained packet bytes. A later
pair observes earlier changes through any alias. The adapter copies the
receipt-proven center writes back after the call, including a failed upload.
A callback exception is converted to an I/O refusal before crossing the C
boundary, preserving the already executed writes. Callbacks cannot replace
storage, corrupt metadata, or separately edit these sidecars during the call.

The texture struct's packet pointers are borrowed. They must not outlive the
body, and each later non-bypass adapter call rebuilds them. Copying the body
and name state is insufficient for copying unrelated texture images; callers
must also clone/rebind the image backend and other external state.

## Body-tail execution and failures

`recenterGameBodyNames` builds the five C buffer views and invokes the exact
504A8 tail using the wholeF0ED8 context root. It imports both sidecars on
success and on refusal, retaining the C owner's ordered-store journal. All
200 stores occur in original player/bank order. The saved old first center
is used for both packets; old second-center contents are not substituted.

Both operations are in place, not resumable and not atomic transactions. To
stage a whole frame or load, copy the body, names, image/backend state and
all external effects together, then publish only the completed candidate.
A native refusal does not prove the original game would have refused.

## Verification and limits

The public binding test passes144 checks in strict Debug and Release builds.
It runs actual bypass width calculation, the C image uploader and native
VRAM transfer, both50768 calls, the504A8 tail, and a later non-bypass name
redraw. It checks actual glyph nibbles in the word plane, retained bank
aliases, clone rebinding, unavailable center state, exact journal limits,
early/late refusal, and a callback exception. The tracked C entry is also
checked after only the first pair's center writes have executed.

These tests use explicit synthetic resources and source-state inputs, not
an observed cold game session. The C tail and font producer have separate
original-instruction evidence; independent binding/source review is recorded
in `.local/verification/native_completion/body_names_backend_review/`.
No complete52C20 loader, font pool allocator, camera, GPU draw list,
rasterizer, visible player, possession, or playable match is established here.

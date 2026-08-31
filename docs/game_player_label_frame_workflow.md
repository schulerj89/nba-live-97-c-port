# Actor-linked text-object frame pass

`game_player_label_frame.c` owns the complete GAME `35BEC` pass and its `2F10C`
packet-allocation bitmap release, plus the `55F0C` scratch-word read. It
composes the existing native `56914` packet linker in its established aligned
tag domain. The pass needs no IO, math or child callback and can run on the
same `Nba97PlayerFrameContext` retained memory as the other frame owners.

The original function traverses ten actor slots, following each slot's signed
text-object indices through the actual style, object pool and list heads.
It positions active objects and their glyph packets or unlinks expired objects.
It does not create labels, choose replacement fonts, initialize pools or
decrement a lifetime counter.

## Preserved source behavior

The pass captures the initial style pointer at `B2048` for later unlinking,
while most traversals and rendering operations reload `B2048`. These are not
collapsed into one style snapshot. It resolves each current object from the
live pool pointer and its signed index times 64, with wrapped arithmetic.

Active objects start with an off-screen offset derived from -20 and their
existing base/translation halfwords. The original force flag, scratch word
`1F80000C`, display option, possession, controller assignment, team flags and
selected actor ID determine whether screen coordinates at `FEA94` replace
that offset. Screen positioning retains the original vertical addition of
three. Existing translations are stored in source order before glyph work,
including when the glyph count is zero.

The bank byte is read live from style `+53`; the opposite bank is the byte
XOR one. The pass does not clamp malformed banks to zero or one. Every glyph
copies coordinates from that opposite packet bank, adds wrapped offsets,
and stores the original eight halfword positions in their original order.
The source count is signed and the loop ends only when its decrement reaches
zero. Negative counts are not silently skipped or repaired.

Each glyph reloads its actor depth word at `106038` and the ordering-table
pointer at `102924`, then links by the depth's low twelve bits. The native
`56914` owner retains packet-store-before-table-store ordering and preserves
the high tag bytes, including self aliases. Its unsupported unaligned tag
domain remains a native restriction, not a claimed original LW trap.

Expired objects first store `-1` to their lifetime field in the original call
delay slot, then release packet-allocation bitmap bytes through `2F10C`.
That helper computes its index with the original wrapped multiply/add/negate
and arithmetic-shift sequence. It does not subtract host pointers or replace
the calculation with ordinary unbounded division. The signed low halfword of
the count controls how many allocation bytes are cleared; zero and negative
counts retain their original behavior.

After release, the pass updates the style's global first/last links, neighboring
objects, the current actor's head or predecessor link, and any successor's
back-link. All reads occur after earlier stores as in the source. The final
next-object halfword is reloaded from the current object; aliases can therefore
affect subsequent traversal. No automatic cleanup or list repair occurs after
a failed release, link or memory access.

## Ownership and verification

The caller must provide the actual style, object pool, actor lists, packet
allocations, bitmap, screen/depth arrays and ordering state. Canonical reached
metadata, original address provenance and retained aliases use the existing
player-frame contract. Private ABI stack/code cannot alias visible allocations.
Unknown inputs, unavailable spans and operation limits retain the completed
source mutation prefix. Cyclic lists and negative glyph counts terminate only
at an explicit native bound or another reached ownership refusal. The result
is diagnostic, not resumable; stage the whole retained state before running
if publication must be atomic.

Progress `actors` counts completed outer slots, `indicators` counts visited
text objects and `links` counts completed glyph links. A standalone
`nba97_game_player_label_frame_release_packets` entry exposes the complete bitmap
release for independent testing without constructing a text-list fixture.

Private evidence is in `.local/verification/native_completion/player_label_frame/`.
All 313 words across `35BEC`, `55F0C`, `56914` and `2F10C` are checked against
raw GAME SHA256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0` and executed
by the private source comparison with original branch and load delay slots.

Strict MSVC Debug/Release and GCC C99/C++17 builds each pass 61 public checks.
Each private Debug/Release comparison runs 411 cases, compares 145,592 ordered
stores and all reached memory-access ordering, persistent bytes and byte
knowledge across 1,134,921 original instructions, and includes 118 matching
refusal prefixes. Cases cover both packet banks and malformed banks, visibility
routes, retirement and neighbor links, all signed-count classes, wrapped bitmap
indexing, negative glyph counts, cyclic object chains, unavailable data, and
packet/style/list-head/bitmap aliases plus an ordering self link.

These fixtures establish the bounded text-frame owner. They do not establish
the original pool/font producer, natural frame entry, hardware text rendering
or a complete match.

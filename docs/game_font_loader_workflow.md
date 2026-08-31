# Original gameplay font producer

`game_font_loader.c` owns the 394-instruction GAME `2E528` font producer, its `29BFC` retry loop, numeric decoder `2E468`, and the pure SHPP and packet helpers it consumes. It uses the retained original-address regions defined in `game_text_objects.h`. This is the original font/map/descriptor producer; it does not substitute a system font, invent packet addresses, or claim that labels have reached the GPU.

The caller must already own the current-style pointer at `800B2048`, the style allocation, its descriptor array at `+8`, and its map array at `+C`. The source loader assumes those allocations exist. It writes the two team-color words for the special overlay font, spacing/kerning bytes, selected map entries, twenty-byte glyph descriptors, and the style's glyph/font cursors at `+24/+26`. It does not initialize the whole style, fill missing map entries, or allocate the object, packet, and bitmap pools used by `30D18`.

The original initialization audit identifies `2E200` as the preceding style/pool producer, followed by five calls to this loader: `ZKERN12`, `ZKERN10`, `ZKERN22`, `ZOVLFONT`, then `ZBLACK12`. Player-label owner `35A44` selects font `100` hexadecimal, so it consumes the second font's map (`ZKERN10`), not the overlay font alone. `2E200` and its actual allocations remain separate prerequisites. The private font-family comparison uses all five original resources with their actual per-font selector, spacing, kerning and palette arguments, but does not claim to execute that preceding allocator or the whole cold initialization sequence.

The six source arguments are filename address, spacing, kerning, CLUT x, CLUT y, and recolor. The last three are consumed as low signed16, low signed16, and low unsigned16 respectively. Filenames and published references have explicit original addresses. The filename comparison models the case-sensitive byte-string result of the `9CB5C` BIOS strcmp boundary; it does not claim to reproduce the BIOS ROM's precise memory-read footprint.

## Resource and backend boundaries

`Nba97GameFontIo` must perform the actual operation or refuse. There is no successful fallback.

- `LOAD_ATTEMPT_941C8` consumes the original filename and flags `20`. `29BFC` repeats attempts returning NULL, without changing the request. The native step budget bounds the source's otherwise unbounded retry; it does not convert NULL into a successful empty resource.
- `UPLOAD_CHAIN_94540` and `UPLOAD_946B8` consume the original image address and coordinate arguments. Compose the frozen image-upload owner over the enclosing retained allocation and a real transfer backend. Header mutation, signed links, temporary heights, SDK uploads and pending-word writes belong to that operation; copying pixels alone is insufficient.
- `RELEASE_90698` requires the actual resource-release semantics. It is an explicit allocator boundary, not an unconditional free/no-op invented by this owner. Retained storage and region metadata stay alive and fixed until this call returns so the completed prefix remains inspectable.

Callbacks may change retained bytes and knownness synchronously, including aliased font/style/map/descriptor storage. They may not replace region metadata or mutate the event, scratch, or progress. The loader preserves source reloads and cached addresses rather than re-resolving every pointer after a callback. In particular, it caches B2048 before loading the font, but the two early logo-color stores reload B2048 separately. Final cursor updates reread the cached style after the release callbacks.

## Original data and quirks

The original ISO contains `ZOVLFONT.PSH` at LBA 250970, 87,652 bytes, SHA256 `0bf8812786d38f92227d6e9022b73d0deb3e567085a8bfc0fe0c5057f10cb3ef`, and `ZLOGOS.PSH` at LBA 249370, 99,848 bytes, SHA256 `6e888fa9908d8419e614af138d5678c35b8f3a5d7201465e9d2a61bb9818edea`. They contain 78 and 62 directory entries. The source reads raw count/offset words; the owner does not add signature checks, require forward offsets, or normalize duplicate references.

For filename `zovlfont.psh`, the loader first loads logos. Team words `80021D74/78` select entries `team+31` for two palette-derived colors. Directory names ending in `54` or `55` then replace the font image with the corresponding logo entry `team`, copying the original font coordinates into that logo header. Palette uploads are deduplicated by the first two name nibbles. When recolor is nonzero and the first nibble is at least six, source `2E884` overwrites palette halfword `+20E` with `6AF7` before upload.

The following source behavior is preserved rather than repaired:

- `2E468` recognizes `1..9`, `a..f`, and `A..F` after byte masking; all other characters, including `0`, contribute zero. It is not a validating hex parser.
- `2E964` overwrites the image's entire first word with its format byte before `946B8`, clearing the signed header link. `2E978..994` rebuilds a link only for a replaced logo and only when the image address is unsigned-less-than the last palette/image address. Repeated palette names can therefore affect which address is retained. No unconditional link restoration is added.
- `9C328` writes only packet bytes 3 and 7. `2EA80` copies the packet's whole first word into the glyph descriptor: the untouched low24 bits come from incoming stack data, not a manufactured `FFFFFF` terminator. `Nba97GameFontScratch` retains the forty packet bytes, four name bytes, and their knownness. The copied unknown low24 bits remain unknown in a destination that supports knownness. A destination declared entirely known cannot silently accept them.
- An out-of-range live SHPP index returns NULL from `A3FEC`; `A4014` writes a zero name word. Its branch targets `A4038`, then falls through the store at `A403C` and the return at `A4040/44`. The function-table extent ending at `A4038` omits those three reachable instructions; the owner includes them. The source's cached signed loop count and fresh unsigned directory-count tests remain distinct.
- Descriptor dimensions/UV values truncate to bytes, coordinates and cursor updates wrap to halfwords, rotated glyphs swap width/height, and glyph vertical offset is the negated header byte `+A`. Values are not clamped.

`993DC` reads the actual mode byte at `800C55C0`. `9BF98` uses its two original mode branches to compute the texture-page halfword; `9C060` computes the CLUT halfword. Mode and palette positions must have provenance, not assumed platform defaults.

## Safety and scope

Every reached region read/write checks bounds, original-address alignment and canonical knownness. Unknown reads refuse; known writes establish knowledge. Opaque packet-tag copies preserve per-byte knowledge. Whole allocations are not preflighted for byte knownness: a late refusal retains earlier source writes and completed callbacks. Region metadata errors at entry leave progress unchanged. Stage all retained resources and the backend together if a host transaction needs atomic publication.

The explicit scratch represents source stack `+18..3F` and `+40..43` after the loader's prologue; its metadata and storage must be disjoint from retained regions. No source stack address is invented or published. Resource aliases to the original call stack or executable code are outside this bounded contract. The local 256-byte palette-used grid is genuinely initialized by the original and is therefore zero-initialized by the owner.

The private original-instruction comparison executes original loader, numeric/SHPP/packet helpers, retry loop, and full `94540/946B8` CPU owners. Only resource-attempt, release, SDK-transfer, and BIOS strcmp semantic boundaries are hooked. The native side composes the frozen image-upload implementation. This proves the bounded CPU composition for explicit fixtures and the actual font/logo bytes; it does not establish heap-loader provenance, SDK DMA hardware behavior, natural cold-entry state, or final text visibility.

Both private strict Debug and Release builds pass 48 native checks, 168 composed loader cases (including 72 using actual resources), 12 direct SHPP cases, and all 65,536 decoder byte pairs. The composed cases compare 19,048 ordered outer/SDK events and 5,844 completed glyphs per configuration. All 394 loader instructions, all thirteen instructions of the complete name helper, and every texture-page mode branch are covered. The three instructions for setting `9C274`'s flag are outside this caller's always-zero argument domain. Independent review additionally exercised callback changes to B2048 and fifteen knownness/prefix checks per configuration without remaining findings.

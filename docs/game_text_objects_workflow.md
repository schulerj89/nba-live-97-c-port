# Player label objects and packet lists

`game_text_objects.c` owns the actual CPU operations required below the existing player-label owner: group reset `30758/30658`, object creation `30D18`, bitmap packet allocation `2EF88`, metrics `2EB50/2ECD4`, packet-reset wrapper `99960`, and low-24-bit packet linking `56914`. It also implements the aligned 40-byte copy path actually reached in `AA468`. It creates retained text objects and paired textured-quad packets; it does not manufacture a visible-label success result.

The source is GAMEONLY, loaded at `80015000`, SHA-256 `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`. Private raw dumps, comparison scripts and strict native builds are in `.local/verification/native_completion/text_objects/`. No shared build, host, frozen render file or Ghidra project was changed for this owner.

## Memory and integration contract

`Nba97GameTextMemory` maps explicitly supplied original address ranges to retained bytes and optional per-byte knownness. Source address ranges must not overlap; their native storage may alias. Original addresses must come from proven source state, not a native heap address or a synthetic address chosen to make a scene run. They are required because the style contains live pointer words and the packet lists physically encode low 24 address bits. Missing allocations or unknown consumed bytes refuse at the reached operation. Noncanonical knownness refuses before it can be erased by a write.

Ordinary pointer and numeric reads require known bytes. Original stores establish knownness. The opaque 40-byte packet copy and three-byte link transfer propagate unknownness without interpreting unknown payloads or making padding known. A destination without a knownness channel cannot accept unknown copied bytes. Allocation metadata and lifetimes stay fixed during a call; synchronous callbacks may mutate bytes and knownness through aliases. Self-modifying instructions or resource aliases with an original call stack are outside this persistent-resource contract.

The full-address creation entry preserves source text aliases. The byte-span entry shares the same owner but uses a relative text cursor. `30D18` never publishes the text pointer; its metrics use only cursor differences, truncated to 16 bits. A span therefore avoids inventing an original stack address for `35A44`'s temporary label string. Span bytes and knownness may alias region storage, and callback changes remain visible. Its metadata and lifetime must remain valid synchronously. Source object, font and packet pointer words still require original-address provenance. `stopped_in_text` distinguishes a span offset from a source-address diagnostic.

Native bounds and the operation budget retain completed prefixes. They do not roll back earlier allocator bytes, object list links, packet writes or external callbacks. Refusal leaves the output object reference unchanged; a completed source allocation failure returns a real null reference. Stage all retained regions and external backend effects together if the host requires atomic publication. The progress record is diagnostic and cannot resume a partially completed source call.

For `35A44` integration, reset group three, then invoke creation with the actual style after its source field writes, ID `246..255`, coordinates `-20,-20`, mode one, and the generated label span. Return a view into the actual retained 64-byte object when nonnull. The existing label owner then clears object fields `20/1E` and calls `99960` on object and object+4 again. Those repeated resets are source behavior; do not remove them because creation already reset and linked its packet heads.

## Required source state

`B2048` points to the current style. Creation and group reset save this pointer, while metrics and allocation load the current global at their own entries. Additional fresh global reloads occur during ID updates. Callback changes can therefore separate the saved style from a later helper's style; the owner preserves that ordering.

| Style field | Source meaning consumed here |
| --- | --- |
| `08` word | Twenty-byte glyph descriptor array |
| `0C` word | Font character maps, 256 signed halfword entries per font |
| `10` word | Sixty-four-byte text object array |
| `14` word | Signed halfword object-head array indexed by nonnegative ID |
| `18` word | Packet storage; each bitmap unit corresponds to 160 bytes |
| `1C` word | Allocation bitmap, scanned byte by byte |
| `20` signed half | Bitmap limit used by `2EF88` |
| `22` signed half | Object-slot limit |
| `26` signed half | Font/map offset selector; map byte displacement is twice this field |
| `28` half | Status copied into the new object's `12` field |
| `2A` signed half | Group selector; 1/2/3 select their lists, other values use group zero |
| `2C/2E` halves | Most recent nonnegative IDs below 100 / below 200 |
| `30..3E` halves | Four group head/tail pairs |
| `40` signed half | Cached object-search cursor |
| `42..49`, `4A..51` bytes | Space width and kerning, indexed by the signed high byte of `26` |
| `52` byte | Newline vertical advance |

Color control words are read from `B204C..B2058`. Packet reset additionally consumes `C55C2`, `C55BC` when diagnostics are enabled, and the current dispatch pointer `C55B8` plus table offset `2C`. None is inferred from a zero-initialized native object.

The loader `2E528` is a concrete remaining producer of the font maps/descriptors and related uploaded resources. Its raw call graph includes resource loading `29BFC`, SHPP access `A3FE0/A3FEC/A4014`, format conversion `2E468`, and image upload `94540/946B8`; it is not implemented by this text owner. The special-name literal at `24920` is `zovlfont.psh`, and `24930` is `zlogos.psh`. The existing original initialization audit calls `2E528(zovlfont.psh,8,0,300,F0,1)`, with numeric values shown in hexadecimal. Initial style/pool allocation and their original addresses likewise require source provenance. This recovery can consume a proven initialized pool; it does not substitute a native font or invented geometry resource.

## Preserved source behavior and bugs

`30758` sign-extends the incoming group halfword and follows object `1C` links from the saved style's selected group head. Each reached `30658` writes zero to object status `12`. It does not unlink the object, release its packet bitmap, or mark the slot negative. Creation's search considers only negative status free. Resetting and immediately recreating labels therefore does not imply immediate slot reuse.

`30D18` first scans from the cached cursor toward the signed slot limit, then from slot zero toward the original cursor. At `30E04` there is no final exhaustion failure check: it uses the reached slot even when neither scan found negative status. Reusing an already linked slot can create a self-link. This behavior is preserved and commented. A native cycle budget may later refuse a reset; it does not repair the list.

`2EF88` computes `need = arithmetic_shift_right(sign16(glyph_count) - 1, 1)` and searches its existing bitmap. A successful bitmap unit supplies 160 bytes, enough for two glyphs' paired 40-byte packets. Its comparison is strict `limit < offset`, so the single-unit path can write at offset equal to the limit. The larger-run path checks its end pointer. These inequalities are preserved. Zero glyphs yield `need = -1`, write no bitmap byte, and still return the real current packet-pool pointer. This is not silently converted into allocation failure. Actual failure returns null after the source cursor/count writes, and creation stores null into object `08` before returning.

The new object stores its packet pointer at `08`, glyph count at `0C`, low-half X/Y at `0E/10`, status at `12`, ID at `14`, ID-list links at `16/18`, group links at `1A/1C`, and source-cleared fields at `1E/20/2B/3B`. ID and group lists are updated in source order. Existing references may alias the new object; no separate detached object is returned.

Metrics process spaces, newlines and controls before glyph lookup. Creation first checks the font map and interprets those controls only when their map entry is negative. Thus an unusual map assigning a glyph to a control can disagree with the allocated glyph count. The implementation does not reconcile or silently resize that source mismatch; all reached writes remain bounded by actual owned storage.

Both paths preserve lowercase-to-uppercase fallback, then question-mark fallback, then space advance. Control `1F` advances X by the next unsigned byte; `1E` selects one of four color words; `1D` selects a font map using `(next_byte - 1) << 9`; `1C` moves Y by the next signed byte. Controls can skip a zero byte and continue reading; the owner does not impose an earlier C-string truncation. Center alignment uses signed halfword width with arithmetic shift; right alignment subtracts the raw unsigned halfword. Dot/slash alignment calls `2ECD4`. For multiline text, modes outside `0..4` keep the current X instead of inventing a default reset. Arithmetic and coordinate narrowing retain source wrapping.

Each rendered glyph reads the current twenty-byte descriptor and writes an actual 40-byte textured quad: source tag, texture/CLUT halves, command byte, U/V bytes, RGB, and four coordinate pairs. Descriptor byte eight is a signed Y offset; width/height bytes are unsigned. The second 40-byte packet is copied in the original reached order: two groups of four word loads before their stores, followed by two separate words. Descriptor storage may alias packet storage, so later reads may observe the writes just performed. Opaque packet padding is copied rather than initialized.

Finally, `56914` links the paired packet lists in reverse glyph order. Its `LWL/SWL` sequence copies only three address bytes and preserves each packet's high length byte. The two object words become the two list heads. These encoded references are why actual source-address provenance remains required.

## SDK packet-reset boundary

`99960` is not a visibility toggle. When `C55C2 >= 2`, it first calls the current diagnostic callback `C55BC` with the original object/count. It then rereads `C55B8` and invokes table offset `2C`, originally `9A97C`. After that call returns, `999DC` unconditionally writes `C567C & FFFFFF` to the object's first word. The native wrapper performs that store only after its external boundary reports completion; a refused callback is not treated as an original SDK failure code.

`C567C` is not itself the literal null-list sentinel. Its original static five words are `{04FFFFFF,0,0,0,0}`: a four-command packet whose next link is `FFFFFF`. A subsequent packet consumer must retain the current contents of this resource; it must not silently replace the reference with an empty list. This owner writes only the reference and does not establish whether that packet has changed during initialization.

The original `9A97C` programs DMA ordering-table clearing through pointers `C56B0/C56AC/C56A4/C56A8`, starts it with `11000002`, and polls completion with `9BAFC/9BB30` timeout handling. Those device and timeout effects are not supplied by a fake successful callback. The native event includes the actual loaded dispatch target, object address and count. A real backend must implement its supported domain or refuse it. Counts other than one remain relevant to actual scene ordering tables; this wrapper does not pretend that one object-word write completes those larger clears.

## Verification

The private oracle executes original GAME instructions for every CPU owner above, including `AA468` and `56914`. Only the original SDK DMA target and diagnostic callback are explicit external boundaries. It compares callback-visible memory and ordering, complete non-stack memory, output references and completed callback counts. Inputs include all group branches, allocation exhaustion, zero/negative glyph counts, lowercase/fallback/control parsing, multiline alignment, descriptor/packet aliases, and callback changes to current style and text bytes. DMA fixture behavior is explicitly synthetic and is not hardware proof.

Private Debug and Release each pass 45 public checks and 928 original-instruction cases, including 128 span cases and 1,931 ordered callback events. All 887 instructions across the eight direct CPU owners are covered, plus 42 instructions in the reached `AA468` copy path; the generic 200-instruction copy helper is not claimed complete. Exact coverage and final public hashes are recorded in the accompanying private freeze receipt. Public tests independently check packet coordinates and encoded links, bitmap allocation, deactivate-only reset, exhausted-slot reuse and resulting cycles, source allocation failure, refusal prefixes, unknown padding propagation, source alignment, metadata errors and native text-span bounds. Integrate only the new C owner and its test target; no font-loader, DMA backend, host entry or visible-scene completion is implied by these checks.

The partial-known label integration corrects one port validation at original
31344LW/3134CSW: glyph word0 is an opaque copy, not a numeric read requiring
all bits known. Its four bytes and their knowledge are snapshotted before the
store, including source/destination aliases; both accesses retain original
alignment and canonical-metadata checks. Unknown low24 font-tag bits from
2EA80 survive packet-bank copying until56914 replaces them. An all-known-only
destination refuses unknown copies rather than fabricating a terminator.
The new label_partial_known receipts supersede the earlier text_objects C/test
freeze identities for this correction, while preserving all earlier receipts.
Both strict configurations now pass50 text checks and repeat all928 original
cases/1931 callbacks, plus the real five-font producer/label composition.

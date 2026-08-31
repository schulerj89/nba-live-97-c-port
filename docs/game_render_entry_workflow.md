# Render-entry texture and label tails

The nonzero `21498` route of GAME `63EDC` calls `4D9EC`, `35A44`, then
`38A18(-1)`. These owners are needed after `2DB68 -> 48D5C` enables rendering;
the earlier flag-zero attributes route does not close natural startup.

There are repeated nonzero calls during natural startup. `48FA8` loads one,
`48FB0` stores it to halfword `21498`, and `48FB4` immediately calls `63EDC`
before `48FBC -> 56944`. This happens inside `2DB68/48D5C`, after the second
`65DB0` and before `66F88`, `79664`, and the third `65DB0`. `67478` calls
`63EDC` again after that third period initialization. Preserve both tail
sequences and the intervening mutable cache/resource state; they must not be
collapsed into one final attributes call.

The new C modules own the CPU work in these tails. They do **not** claim that
the complete renderer, CD wait, GPU backend, or text allocator is integrated.
No required operation may return fabricated success. Source addresses below
refer to the original GAME overlay, not frontend routines with similar names.

| Native entry | Original ownership | Remaining actual backend operations |
| --- | --- | --- |
| `nba97_game_render_bindings` | `4D38C` reference publication | Caller maps constant references and preserves copied reference knownness |
| `nba97_game_render_textures` | Ten-player `4D9EC` loop; `4E3CC`, `4D8C0`, `539FC`, `4DAD8`, `4D944`, `51ED8`, `50E40` ordering | Image upload `946B8`, VRAM move `997E4`, sync `994F4` |
| `nba97_game_player_labels` | `35A44`, `35A24`, `%d`/`%s`/string selection | Group reset `30758`, actual font/text-object creation `30D18`, SDK packet reset `99960` |
| `nba97_game_head_cache` | Full `38A18`, `3875C`, record-zero `A3FEC`, bounded disjoint/exact-alias copies equivalent to `AA468` | VRAM store `99780`, move `997E4`, upload `946B8`, sync `994F4`, CD/service wait `8892C` |

`50E6C` is literally `jr ra; nop`, so it has no callback. `50E40` calls
`946B8`, then `994F4(0)`; every occurrence retains that order. An upload event
supplies the actual resource header and all four signed argument words. Texture
coordinates use native VRAM word units. The backend must implement `946B8`'s
image-format semantics, not assume every resource is an eight-bit image.

## Required loaded state

`game_render_textures.h` exposes live resources and bindings without emulated
addresses. A buffer is a retained mutable allocation; an image additionally
retains its offset within that allocation. This allows signed header-relative
palette references to point backward within owned bytes. Do not replace image
offsets with unsigned relative offsets or detached records that lose the
preceding referenced bytes.

* Bind ten player records through the physical span fixed by `4D38C` at
  `FC654 = FDCEC`, then `+i*F4`. Each record supplies signed roster ID `+0`, raw
  jersey `+7`, height `+9`, skin selector `+B`, and NUL surname at `+29`.
* Supply the 26 glyph image references originally stored at `FECA8`, from
  `ZDOMLTRS.BIN`. The rasterizer reads signed width/height at image `+4/+6`,
  packed pixels at `+10`, and a source row rounded up to four pixels. It does
  not decode a frontend-generated font or infer stride from record length.
* Supply both ten-entry digit-reference tables `FAC24`/`FB154`, from the loaded
  team `ZDOME` resources. Number palette source pointers are `EBC38`/`F0F64`;
  their signed header word shifted right eight locates the palette header.
* Supply the actual `FCD78` name header and following 1500 scratch bytes,
  `109DA8` 1040-byte number scratch, and each player's four existing polygon
  references `FEBFC/FEC00`. These are real mutable polygon buffers, not
  generated geometry. Name preparation writes only their U bytes `C/14/1C`
  and the corresponding `FEDF0/FEDF4` center words. Preserve pointer aliases.
  `F0F68` controls the alternate center-only path and must have provenance.
* Supply mutable team palette templates `EBC48 + side*2940` and the owned skin
  source window at `FEEB8`. The source patch starts at
  `side*C60 + (player[B]>>1)*210 + 1B0`, contains 48 halfwords, and replaces
  entries 208..255 of the selected team template. The source window is not
  implicitly enlarged for corrupt selectors.
* Supply full-word name coordinates `FEA3C/FEA6C`, number coordinates
  `103F4C/103F50`, number CLUT coordinates `FABD0/FABD4`, 24 optional roster
  IDs at `10424C`, source/destination rectangles at `FCB18`, corresponding
  palette resources `D9294 + slot*210`, and CLUT coordinates `FE9D4/FE9D8`.
  The first matching signed ID wins. Unmatched players do not receive a patch.

`4D38C` also copies twelve native reference tokens from the arrays corresponding
to `20B8C` and `20BBC`, and publishes constant references listed in its header.
These constants include fields before the player span: negative byte offsets
must not be forced into an unsigned entity index. Reference values and their
knownness must be copied together by the host adapter. Only `render_first=0`
is a proven player-span assignment.

`game_head_cache.h` requires both actual team header counts and lineups, the
two twelve-word current-cache arrays `1046A4/1046E8`, both loaded bench image
allocations `1046DC/104740`, and the shared SHPP scratch allocation referenced
by `102930`. Every bench block is `107C` bytes. `3875C` resolves the scratch
container's record zero and overwrites its palette entry at relative `+20E`
with `6AF7` before upload. Neither the scratch header nor bench contents may be
constructed from guessed defaults. Readbacks store exactly 38x48 VRAM words at
block `+28` and 256 palette words at block `+E78`.

`game_player_labels.h` consumes ten separate entity-table references, not the
physical span. It requires the current style allocation `B2048`, original
position-name string references `B3058`, and the actual raw option byte
`21D83`. The `30D18` backend receives an ID 246..255, text, coordinates -20,-20,
and argument 1. It must return a real object with at least 34 bytes, or the
original allocator's null result. Backend refusal is distinct from allocation
failure. Successful objects have halfwords `+20`, then `+1E` cleared, followed
by `99960(object,1)` and `99960(object+4,1)`. That SDK routine performs function
pointer dispatch and writes a packet-chain reference; treating it as a
visibility boolean would change original behavior.

## Original quirks kept

* `4EA50` and `53C44/53EA8` replace the scratch image's entire first word with
  its low byte. Upper authored header bits are intentionally discarded.
* `4E51C/4E52C` advance the surname pointer and increment an eight-bit character
  count. The count wraps. Names are never truncated or replaced with an empty
  string. Pixel addressing and UV stores retain signed arithmetic and low-byte
  wrapping; a native allocation boundary refuses instead of writing unowned
  bytes. The native bound is the supplied allocation, not a fabricated clamp
  to a 100-pixel line.
* `539FC` sign-extends jersey byte `+7`. `FF` draws two zero glyphs. Other
  negatives are not converted into unsigned numbers; some negative multiples
  of ten select digit zero, while negative unowned indices refuse. Numbers
  100..127 display their final two decimal digits. If a palette has no zero
  entry, its scan reaches index16 and fills scratch with byte `10`, exactly as
  `53C00/53E64` do. It does not choose a repaired transparent index.
* `4DBB8` turns zero colors into `9084`, and other colors gain bit15. It mutates
  the shared team template in place for every player. Later uploads must see
  the latest contents, while earlier uploads retain what they consumed.
* The `4D9EC` height write uses loop index and `player[9]*624`. The preceding
  `63EDC` height write uses the entity's raw word ID; these are different calls.
* Label options 1/6 display raw jersey `FF` as `-1`, even though number textures
  display it as `00`. The position string lookup drops the top two index bits
  through the original left shift. Option2 preserves wrapped subtraction.
* `35A44` has a 32-byte local string. Its original library calls can overflow
  the stack. The native owner returns `TEXT_OVERFLOW` rather than silently
  truncating or reproducing host memory corruption; prior style writes remain.
* `38A18` searches caches without a source bound. Exhausting the twelve owned
  entries returns `SEARCH_OUTSIDE_OWNER`, never an invented successful skip.
  The count check after finding a source occurs **after** `8892C`; callbacks
  may change count before it is consumed. An already-correct negative-route
  entry skips service; a positive request for its existing slot still swaps.
* Home bench-to-bench swaps call `994F4` three times (`38DF4`, `38E40`, `38E74`).
  Away swaps omit all three. The source cache entry changes before the final
  scratch copy; the destination cache entry changes after the last home sync.

Partial allocation overlap in block copies is outside this bounded native
owner and explicitly refuses. Exact identical source/destination is allowed.
Other resource aliases, including repeated polygon references and palettes
inside a shared allocation, remain live.

## Integration and verification

The native build now links `game_render_textures.c`, `game_head_cache.c`, and
`game_player_labels.c`, with `tests/game_render_entry_tests.cpp` as a separate
test. All85 CTests pass in Debug and RelWithDebInfo.
No extra C++ library, emulator, SDK library, or frontend model decoder is a
dependency of these CPU owners.

At the nonzero attributes boundary, apply `63EDC`'s preceding effects, publish
the `4D38C` bindings, run texture setup, then player labels, then head-cache
update with argument -1. Do not mark the attributes call complete when one of
the required backends refuses. In particular, the actual text allocator and
CD-service state are still integration dependencies.

The repeated binding publication is an actual child call, not a native reset:
`63EDC` calls `4D9EC` at `64098`, which immediately calls `4D38C` at `4D9FC`.
The loader also calls `4D38C` independently at `48DF0`, before `48FB4`'s
attributes call. Every reached nonzero attributes tail must therefore repeat
the binding writes and copy the current `20B8C/20BBC` references; reusing only
the earlier loader bindings would skip original effects.

All operations expose source prefixes on refusal. Copying the C state struct
does not isolate its pointed-to resources: a transactional host must clone
the backing allocations while preserving their alias graph and stage the
texture/VRAM backend too. Every upload callback must consume/copy bytes before
return because the same scratch buffers and palettes are reused immediately.
Every store callback must produce the requested readback bytes. Merely logging
an operation and returning1 is not a complete backend.

Private verification is in `.local/verification/native_completion/render_entry`.
Debug and Release builds use `/W4 /WX`; 74 public checks cover refusals, signed
quirks, mutation prefixes, live count changes, label overflow, and asymmetric
syncs. Independent original MIPS execution compares 660 texture/head cases
per configuration, including 30 complete ten-player loops using actual
`ZDOMLTRS` and `ZDOMEATL` records, plus 240 label cases/6240 backend events.
It executes original CPU callees and treats named SDK, CD, text allocation and
C-library formatting calls as explicit boundaries. These tests do not claim
device execution, complete font-object rendering, or natural-entry closure.

Independent review verified1827 exported instructions against the original
GAME bytes and all frozen source hashes. Its additional144 head-cache cases
made108 callback mutations and matched1440 original/native events, cache
words, bench data and scratch bytes. Two port issues found during validation
were corrected: explicit signed low16 conversion avoids implementation-defined
C narrowing, and a missing palette is refused after the original two digit
uploads rather than prematurely discarding that prefix. No original behavior
was repaired. Original backend obligations above remain open.

The existing `ZdomfModel` retains packet offsets and UVs, and
`Ps1VramTextureAtlas` can consume decoded indexed uploads. An adapter must map
the actual name-polygon references to those packets and preserve the mutated
U bytes. The atlas currently lacks source-equivalent VRAM readback/move state;
it cannot satisfy `99780` by guessing a head from roster identity. Actual
geometry transformation, pose rendering, court assets and camera composition
remain owned by their separate paths, not by these texture tails.

# Original ball, shadow and arrow initialization

`game_player_marker_resources.c` owns GAMEONLY `8004D490`, `8004CAF4`,
`80050E40`, `80050E74`, `80050F88`, and their CPU-only SHPP, texture-page,
palette and packet helpers. These thirteen complete functions contain 1,077
original instructions. It also owns the aligned 16-, 32- and 528-byte domains
of `800AA468` used by this initializer. It does not load substitute textures,
allocate invented objects, render a frame, or establish natural startup state.

The access contract is shared with `game_player_frame.h`: each reached access
has its original address, PC, width, byte knowledge and optional normalized
reference identity. The source stack and code cannot alias visible inputs.
Reads are observational; writes establish exactly their reached bytes. Opaque
palette copies preserve unknown bytes and reference identity. An adapter must
validate every reached metadata byte before accepting either a read or a write;
unknown unread padding must remain unknown. No whole-allocation preflight is
required. Source memory, allocation lifetime and aliases are revalidated after
each synchronous external call. Failed calls preserve their mutation prefix.

## Required inputs and external owners

The caller must supply mapped source addresses, a finite operation budget and
an actual implementation of the four `Nba97PlayerMarkerIo` entries. The request
contains source-consumed arguments; unused slots are zero, not a reconstruction
of incidental MIPS register values.

| Entry | Required operation |
| --- | --- |
| `29BFC` | Load `zdomasdw.bin` from filename `80026174`, then `zdomball.bin` from `80026184`, both with flags zero. The callback must own the real loader/retry/allocator operation and return its original pointer. |
| `90698` | Release the original resource allocation. Do not turn this into a success-only notification or extend a source object's lifetime silently. |
| `946B8` | Run the existing `game_image_upload.c` owner on the retained mutable image and its signed header links, then the actual upload backend. |
| `994F4` | Complete the required SDK synchronization. It does not clear `D7B14`. |

`50E40` calls upload at `50E4C`, then synchronization at `50E54`. `4CAF4`
calls upload directly and has no intervening synchronization. Missing IO
returns `NBA97_MARKER_IO_REQUIRED` at the reached call. There is no dummy
loader, heap, upload, palette, GPU mode or completion value in this owner.

The existing `GameRenderBackend` can consume the upload converter's retained
raw 16-bit transfers. Its original-address alignment, live SDK limits and GPU
mask prerequisites remain necessary. A staged host operation must clone and
rebind source allocations and VRAM together; an external loader or release
callback is not automatically transactional. `D7B14` upload state must share
the caller's actual state, including any observation during callbacks.

The live graphics byte is `800C55C0`. `9BF98` calls `993DC` twice unless the
first result is 1; its alternative page encoding is selected by first result
1 or second result 2. No graphics mode is inferred from the native renderer.

## Actual asset layout and write order

The audited ASDW file is a 3,964-byte SHPP resource with ten directory entries:
`blob`, `CIRC`, and `arw1` through `arw8`. Its logical payload length is 3,952
bytes plus a 12-byte trailer. BALL is 47,308 bytes with thirty entries, each
1,568 bytes apart starting at offset 256; its logical payload is 47,296 bytes.
These original assets and their hashes remain in ignored private evidence.
Directory offsets are relative to the owning SHPP resource. Image-to-palette
links use the signed high 24 bits of the header word, relative to that header.

`4D490` performs these operations in source order:

1. Publish ASDW at `800DCE04`; load BALL. Upload even BALL entries 0 through
   28 in fifteen placements, synchronizing after every upload.
2. Publish BALL image zero's numeric pointer at `800FDB48`. Initialize ball
   packets `80103EE4` and `80103F0C`, reflection packets `8010B1F0` and
   `8010B218`, then four additional packets at `800F9C58 + i*40`.
3. Release BALL. Reload the live ASDW global for each named lookup. Publish
   `blob` at `8010B1EC` and `CIRC` at `80109B7C`.
4. Upload CIRC at `(736,160)` with CLUT `(512,226)`. Copy its linked palette
   header and first 32 palette bytes into ten 48-byte records at `800EBA50`.
   Copy blob's linked 528-byte palette record to `80109B90`.
5. Upload blob at `(720,160)` with CLUT `(512,227)`; initialize twenty player
   shadow packets at `800D8F14 + player*80 + bank*40`.
6. Upload the same mutable blob again at `(512,256)` with CLUT `(512,225)`;
   initialize two ball-shadow packets at `800D9234 + bank*40`.
7. Run `4CAF4`: upload eight arrows and initialize templates at
   `800FAA54 + direction*40`. The X placements are 672, 678, 684, 690, 696,
   704, 710, 716, all at Y=80 and CLUT `(512,236)`. Release ASDW afterward.

The initializer establishes packet tags, colors where the source writes them,
UVs, texture pages and CLUTs. It does not establish packet XY coordinates or
low-24 ordering links. Arrow templates also retain their incoming color bytes.
The later player/ball frame owners supply those fields before consumption.

## Preserved source quirks and native limits

- `4D5A4..4D64C` repeats both ball and reflection page calculations and stores,
  with fresh format/mode reads. These are not consolidated.
- `50E74` uses two times the low six X bits for U; `4CAF4` uses four times
  those bits. Both use low-byte width/height, subtract one and wrap byte stores.
  `50F88` reverses V coordinates and sets transparency. There is no clipping
  or dimension correction in this owner.
- `A547C` reads the four-byte query even when the directory count is zero.
  On a failed nonempty search, `A548C` prefetches the name beyond the final
  directory entry before `A5494` tests the exhausted count. Bounds refusal
  preserves that exact prefix; this confirmed indexing quirk is not repaired.
- `50F54`/`51064` turn a missing image link into address zero and then perform
  halfword reads at offsets 12 and 14. A native unowned-address refusal is not
  a fabricated default CLUT. `A9C44`, separately, returns zero for a null chain.
- `4D810` mutates the same blob header after player-shadow packets have copied
  its old coordinates. Ball-shadow packets receive the newer coordinates.
- `AA468` copies in load/store groups, not a whole-span snapshot. Overlapping
  copies preserve forward/backward ordering and the signed ADD overflow traps
  at `AA65C`/`AA670`. Unaligned and other-length copy domains explicitly refuse
  as unsupported native domains, not claimed original CPU alignment traps.
- Original globals still contain numeric image addresses after resource
  release. These are not retained native owners or permission to dereference
  freed objects. Packet and VRAM outputs have separate lifetimes.
- The operation budget bounds unbounded malformed header chains and directory
  walks. Exhaustion retains completed writes; it neither repairs a cycle nor
  provides a resumable original PC.

## Verification and integration

Private evidence is in `.local/verification/native_completion/player_marker_resources/`.
All 1,077 instructions of the thirteen complete functions were checked against
raw GAME bytes and visited by the independent CPU comparisons. Each strict
MSVC Debug/Release build and the GCC C99/C++17 build passes 7,557 public checks.
Each original comparison configuration passes 326 cases, 11,953 ordered visible
CPU stores, 191,420 original instructions and 711 transfers through the current
native raw-word VRAM backend. Comparisons include whole real-asset initialization,
every required load/release/sync refusal position, malformed SHPP searches,
zero palette links, packet/image aliases and overlapping palette copies.

The private harness executes original `946B8` and its conversion callees;
only `9971C` is joined to the native word-plane backend. It compares all visible
marker reads/stores, source bytes outside the private ABI stack, transfer
payloads and VRAM words/knownness. Loader, release and SDK-sync boundaries use
explicit test fixtures. This proves the bounded composition, not original
heap/CD execution, GPU hardware timing or a naturally reached gameplay frame.

Integration requires adding only `src/recovered/game_player_marker_resources.c`
and its public test target to CMake. The core has no link dependency on a
frontend six-clip sampler or a new renderer. Bind the existing retained-memory
accessor and real IO owners, then supply the resulting packets and VRAM to
the separate player and ball frame owners. CMake and host wiring remain with
the integration owner until this four-file change is reviewed and frozen.

# Live actor-marker palette and packet update

`game_player_marker_update.c` owns all 777 instructions of GAME `4A044`,
including its native clock-read, texture-page and CLUT helper behavior. This
pass selects and edits a marker palette, uploads the selected image, and
updates the live actor's banked marker packet. It does not move the actor or
simulate possession. Incoming argument registers are unused.

The original entry first increments the word at `FAC20`, wrapping to 32 bits,
and stores it. It skips all remaining work unless the result is zero. On the
zero route it reads `FDB58` and writes `FAC20 = -1` before selecting a palette.
This unusual gate is preserved; it is not replaced with a conventional frame
counter or unconditional update.

## Preserved selection and mutation

The controller route resolves the live `1029B0` index through `FC650` and the
actor's signed controller halfword. Selection observes the actual pointer at
`FC634`. Palette addresses use actor ID times 48 at `EBA50`; no native color
table or default actor is substituted. The selected route retains two distinct
loads from the same actor-table slot before reading the actor ID and stats.

Palette intensity comes from the signed upper stat bits, then its low byte.
Actor bytes `DE` and `DF` select the multiplier at `B72D4`. Multiplication
wraps before the logical shift and final low-byte use. Controller routes clamp
a zero intensity byte to one; the computer route deliberately has no such
clamp. Intensity is packed by multiplication by 1057 and stored as a halfword,
without repairing out-of-range color components. Selected and unselected
routes preserve their different palette stores and ordering.

The computer route uses the physical actor base `FC654`, the selected-actor
pointer chain `FC65C`, and the original conditional flags and IDs. Both selected
routes retain the phase-dependent blink test. `A5810` is the actual word read
at `D7A70`; bit 5 is tested without inventing or advancing a clock. The fallback
image is the original `109B90` resource and receives its original packet colors.

Every packet address uses wrapped `D8F14 + index*80 + bank*40`. Color, page,
CLUT and UV stores reload index, bank and actor data at the original points.
The pointer written to `FED1C` remains a normalized source reference through
`WRITE_POINTER`. A completed palette upload or sync may mutate live memory;
later CLUT and UV writes observe those changes. The upload's arguments remain
the values captured before it, including CLUT Y `index + E2`.

`9BF98` samples graphics byte `C55C0` twice unless its first value is one.
The two original page encodings and `9C060` CLUT calculation are retained.
The final UV selection compares a live packet page against a newly computed
page: equal selects U=32, unequal selects U=128. The eight component stores
retain their individual index/bank reloads and original 31-pixel increments.

## Integration and ownership

The C entry uses the existing `Nba97PlayerMarkerContext` memory and IO contract.
Its only external calls are the actual `946B8(image,x,y,clut_x,clut_y)` and
`994F4(0)` operations, reached in `50E40` order. The upload callback must run
the existing full `game_image_upload.c` owner over retained mutable image
memory and an actual VRAM transfer backend. A callback that merely acknowledges
the request is not a completed production upload.

`GamePlayerMarkerUpdate` borrows only `Nba97PlayerFrameContext.access` and
`user`; it does not borrow or replace frame math/child callbacks. Its separate
IO callback forwards the full original five upload arguments unchanged. This
avoids truncating image placement through a narrower outer-frame call event.
The adapter does not allocate or clone memory, establish addresses, initialize
a palette or select device state.

All original CPU writes and completed callback effects survive refusal.
Unknown consumed bytes, malformed reached metadata, missing ownership, source
alignment and operation limits are native refusal boundaries. Unvisited IO
is not required on the counter-gated route. Private ABI stack/code may not
alias visible inputs. A failed call is not resumable; callers needing atomic
publication must stage memory and VRAM together before running it.

## Verification

Private evidence is retained under
`.local/verification/native_completion/player_marker_update/`. All 852 words
of `4A044`, `A5810`, `9BF98`, `993DC`, `9C060` and `50E40` are checked against
raw GAME SHA256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
The image converter remains the existing independently verified native owner.

Strict MSVC Debug/Release and GCC C99/C++17 builds each pass 58 public checks.
The tests compose the actual image converter and `GameRenderBackend`, checking
uploaded palette words, placement, packet fields, the wrapped gate, selected
and fallback branches, the computer intensity quirk, upload/sync refusals,
post-sync mutation, and adapter argument forwarding.

Each Debug/Release original comparison covers 775 cases, 10,484 ordered visible
caller/helper stores, 536 actual transfers and 314,875 original instructions,
including 299 matching retained refusal prefixes. It compares ordered memory
accesses, full IO arguments, transfer rectangles and consumed source words,
persistent RAM and byte knowledge, and resulting VRAM words and knownness.
All 777 instructions of `4A044` and all 75 helper instructions are exercised.
The reference executes original branch and load delays and the actual image
converter instructions; only synchronous sync and SDK transfer boundaries are
declared externally. The native side composes the existing image C owner and
real native VRAM backend. This proves the bounded source composition for
explicit retained actor/palette inputs, not natural match initialization,
hardware timing, a complete frame, or gameplay.

The same 775 cases also pass through the actual `GamePlayerMarkerUpdate` C++
adapter in both configurations, with identical memory, transfer, VRAM and
refusal-prefix comparisons. The adapter forwards access and complete IO
requests; it does not silently drop the upload's fifth argument.

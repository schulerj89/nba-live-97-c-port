# Gameplay image upload owner

`game_image_upload.c` owns the CPU path `946B8 -> 94540 -> 944F4 -> 94440`, plus the unambiguous branches of `A3BF8`. It emits the actual `9971C` transfer boundary. It does not decode an indexed image into colors, invent a texture, or pretend to execute the complete PlayStation SDK. The enclosing mutable allocation is retained so signed backward header links and overlapping views keep their original meaning.

The source is GAMEONLY, loaded at `80015000`, SHA-256 `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`. Private raw disassembly, per-function byte hashes, the native DLL build and comparison scripts are under `.local/verification/native_completion/image_upload/`. No writable Ghidra project or device was used for this recovery.

## Preserved CPU behavior

At `946B8`, signed width and the format's bits per pixel produce the wrapped rounded row size. If the row occupies an odd number of 16-bit words and height is even, the source performs two direct `9971C` tail uploads before the main image. Their source offsets are `image + (saved_height - 1) * row_bytes + 14` and the same product plus `18`. These direct calls do not set `D7B14`. After both callbacks, `947A8` rereads the current header height, subtracts one, and calls `94540`. Only a normal return restores the original saved height. A callback may change the current height: the decrement sees that change while restoration still uses the saved value. Native refusal retains this exact completed prefix; it does not run invented cleanup.

`94540` classifies the header byte with mask `F7`. Image formats `40..43` write Y, write `X | (old_header_X & C000)`, and set bit `08` in byte zero before reading the width and height for upload. Signed negative width rounding is the source's wrapped product plus `30`, not a repaired positive width or an unsigned ceiling division. A palette header `23` writes both CLUT coordinates and the flag, then reads width even when it suppresses the upload. Suppression requires both full 32-bit CLUT arguments to equal zero; arguments whose low halves are zero but upper halves are nonzero still upload.

The next link is read from the header word after callbacks and header mutations. Its signed upper 24 bits are a byte displacement relative to the current header. Backward links, aliases and callback changes are preserved. The caller's header budget bounds an otherwise unbounded or cyclic source chain; it is a native safety guard, not an original terminator. A null chain returns normally at `94540`; the outer `946B8` still requires its initial format-byte read.

`94440` ORs height with one when width is odd, including negative raw halfword values. `944F4` calls it, invokes `9971C`, then stores one to `D7B14` at `94524`. The store occurs only after a successful native boundary callback. Existing malformed dimensions are not silently fixed.

## A3BF8 unresolved delay-slot domain

Mask `77` gives these unambiguous results: `23 -> 16`, `40 -> 4`, `41 -> 8`, `42 -> 16`, `43 -> 24`, `44 -> 1`. Including ignored bits `80` and `08`, these cover 24 raw byte values. The other 232 raw values reach this actual instruction sequence:

```text
800A3C24  10620004  beq  v1,v0,800A3C38  ; v1=44
800A3C28  34030043  ori  v1,zero,43
800A3C2C  1062000A  beq  v1,v0,800A3C58
800A3C30  34030072  ori  v1,zero,72
800A3C34  10620004  beq  v1,v0,800A3C48
800A3C38  03E00008  jr   ra              ; control transfer in delay slot
800A3C3C  34020001  ori  v0,zero,1
800A3C40  03E00008  jr   ra
800A3C44  34020004  ori  v0,zero,4
800A3C48  03E00008  jr   ra
800A3C4C  34020008  ori  v0,zero,8
```

The branch at `A3C34` and `JR` in its delay slot cannot be resolved merely by treating the function as intended pseudocode. The private interpreter is a custom generic MIPS runner; its nested-delay result is not an independent PS1 CPU or hardware proof. No such result is labeled an original bug. `nba97_game_image_bits` returns `NBA97_IMAGE_FORMAT_UNRESOLVED` and leaves its output unchanged for this domain. The comparison stops before `A3C34`. Direct format `44` branches to `A3C38` normally and remains supported. `94540` can skip an unrecognized header body without calling `A3BF8`; that distinct source behavior is also preserved.

## Retained memory and callback contract

Each `Nba97GameImageMemory` describes one owned envelope, with optional per-byte knownness and independently proven original address modulo four. A native heap pointer is not original-address provenance. Unknown bytes may be overwritten by original stores; reads require known bytes. Every reached read or write validates its entire metadata span: a value greater than one is an argument error, even if an earlier byte is unknown. This validation neither preflights the whole allocation nor erases earlier source writes. Callback-created malformed metadata is rejected when subsequently reached.

Metadata and allocation lifetimes remain fixed during the call. Synchronous callbacks may mutate contents, knownness and upload state through aliases. They may not retain stack requests or alter reference metadata and progress counters. Resource, alignment, unknownness, budget and callback refusals retain completed effects. A host transaction must stage image allocations and VRAM together; the progress record is diagnostic, not resumable.

The transfer descriptor contains raw signed halfword coordinates and dimensions. For a positive pre-SDK rectangle, `pixel_words = width * height` and `cpu_words = ceil(pixel_words / 2)`. A backend must consume that many known little-endian 32-bit CPU words, including the extra halfword when the pixel count is odd. The extra halfword is padding, not an extra VRAM pixel. Zero or negative dimensions leave the descriptor footprint unknown rather than inventing an empty operation. An outer caller must prove the supported SDK domain or refuse it.

Return one means the native callback actually consumed the request; other callback returns mean native refusal, not the original SDK's numeric return value. `D7B14` is stored only after that acknowledgment on the wrapped path. No default GPU, CD-service or geometry callback is supplied.

## SDK boundary evidence

The original static dispatch pointer `C55B8` is `C5578`. The table routes `9971C` through `9B298` with transfer handler `9AC7C`, `99780` with handler `9AED0`, and the move packet with `9B1F8`. A changed dispatch table is a separate provenance boundary.

`9AC7C` and `9AED0` clamp negative rectangle width or height to zero, and clamp larger dimensions against mutable `C55C4/C55C6`. These limits are **signed 16-bit values**: the source uses `LHU`, then `SLL 16` / `SRA 16` before signed comparisons, and reloads the low half for a store. Upload width evidence is `9ACC0..9ACE8`, height `9ACFC..9AD2C`; readback width `9AF10..9AF38`, height `9AF54..9AF7C`. Thus the descriptor above describes the positive rectangle before SDK clamping. A backend that does not recover clamping must require known positive limits and refuse rectangles exceeding them. Do not infer the limits from native VRAM dimensions.

At `9AD34..9AD58` and `9AF84..9AFA8`, positive pixel counts are rounded to 32-bit CPU word counts. CPU remainder transfers use `LW` or `SW`; an exact multiple of 16 CPU words skips these instructions and uses DMA. A misaligned remainder access is an original CPU alignment trap. Misaligned DMA-only transfers are an unproved device domain, not evidence of a CPU trap. Odd readback writes an extra CPU halfword whose GPU value is not proven here; refusing it is a native support boundary.

The reviewed handlers issue command-buffer reset `01000000`, then upload `A0000000`, readback `C0000000`, or move `80000000`. They do not issue an `E6` mask-mode command. This recovery does not prove the incoming GPU mask state. Require explicit known unmasked operation for upload/move until a broader mask implementation and its state producer are owned. Wrapped coordinates, negative dimensions, zero-sized GPU transfers, overlap behavior, DMA timing and device failures are not silently emulated.

`997E4` first calls diagnostics `99560`, then reads signed width at `99814`; zero returns minus one without dispatch. Height is read at `99824` and zero also returns minus one. Render callers discard that return. For nonzero dimensions, the destination word at `C5674` is exactly `((uint32_t)y << 16) | (x & FFFF)`, assembled at `99830/9983C/99840/99860`. The raw source XY word is copied to `C5670`, and raw WH to `C5678`. The dispatch packet is 20 bytes beginning `C5668`, initially `{04FFFFFF,80000000,0,0,0}`. The wrapper does not normalize destination coordinates or resolve GPU overlap. A bounded backend may support disjoint in-range rectangles after preserving this low-16-bit packing, and explicitly refuse other domains.

`99560` never writes the rectangle. Diagnostic mode `C55C2 == 1` performs signed extent checks against `C55C4/C55C6` and logs invalid inputs; mode two logs every request; other modes return. Its diagnostic callback `C55BC` is not an owned arbitrary-callback implementation.

`994F4` contains no non-stack store. It optionally invokes `C55BC` when `C55C2 >= 2`, then calls dispatch-table offset `3C`, originally `9B9B4`. It does **not** clear `D7B14`. That clear belongs to a different conditional helper, `9446C`, at `9448C`. The SDK sync implementation has timeout/queue bookkeeping through `9BAFC/9B57C`; a synchronous native word-plane drain is not a claim to reproduce SDK timeout globals or diagnostics. Service `8892C`, text allocation `30758/30D18`, and packet reset `99960` remain distinct owners.

## Verification and integration

Private strict MSVC Debug and Release builds each pass 56 public checks and 4,640 original-instruction comparison cases, including 4,168 ordered transfer events, 256 callback-mutating cases, 32 reached header-alignment traps, 16 backward links leaving the owned envelope, and 144 actual glyph/number asset cases. The source comparison covers all 11 instructions of `94440`, all 19 of `944F4`, all 89 of `94540`, all 92 of `946B8`, and 25 of 26 in `A3BF8`; the sole excluded instruction is the explicitly unresolved `A3C34` branch. All 256 raw format bytes are classified, not all asserted supported.

The comparison executes original GAME bytes and replaces only `9971C` with an ordered transfer observation/refusal boundary. It checks image bytes, source offsets, raw rectangles, callback-visible pending values and completed progress. It does not prove GPU hardware semantics. Actual fixtures use retained bytes from all 26 `ZDOMLTRS` and ten `ZDOMEATL` records, with their original relative links.

Integration requires compiling `src/recovered/game_image_upload.c`, exposing `src` to `tests/game_image_upload_tests.cpp`, and linking the test with this owner. The C++ render adapter must retain enclosing memory, pass original alignment/knownness, implement the supported raw transfer contract, and stage VRAM together with image mutations. No existing render owner, host entry, CMake file or public build was changed by this recovery.

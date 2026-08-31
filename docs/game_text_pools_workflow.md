# Original text-pool initialization

`game_text_pools.c` owns the complete 142-instruction GAME2E200 initializer, the nine-instruction 90160 allocation wrapper, the 36-instruction 901EC lock wrapper, and the three/two-instruction A405C/A4088 lock setters. These 192 original instructions are covered by independent execution of the original GAME bytes. The 231-instruction 9027C heap allocator remains an explicit, synchronous boundary. This module does not establish an original heap, load fonts, create labels, or render text.

The original in-game call supplies `(0x10, 500, 5, 200, 0x100, original_name, 400, 400)`. `original_name` refers to the caller's actual `"in-game font"` string. The seventh argument is unused. The eighth supplies the packet capacity. The ordinary call requests 90,360 bytes.

## Retained memory and allocation

The API reuses `Nba97GameTextMemory`: retained byte buffers mapped to explicit original addresses, with optional per-byte canonical knownness. It never converts native pointers to original addresses. Mapping metadata and lifetimes remain fixed across callbacks; mutations to the mapped bytes and knownness are synchronous. Distinct source regions cannot overlap. Native storage aliases follow the existing text-memory contract. Code/stack aliases are outside this boundary; the eight incoming argument values are supplied explicitly.

The only I/O event requests actual `9027C(original_name, wrapped_size, 0x20, 1)`. An implementation must perform that allocator's heap search, descriptor/free-list, name-copy and required eviction/relocation effects in retained ownership, and return its original descriptor address. A native `malloc` plus an invented address is not that operation. No callback, or an unacknowledged callback, returns `NBA97_TEXT_IO_REFUSED`. The test allocator is explicitly a fixture boundary, not a recovered heap implementation.

The closed wrappers read the live lock pointer at 1029C0, write one through it if nonzero, invoke the allocator, reread 1029C0, and write zero through the newly read pointer if nonzero. They then read word zero of the returned descriptor to obtain the style address. A callback can change the lock pointer, including making the unlock overwrite the returned descriptor. The implementation preserves that read/store order.

The source allocator's flag `0x20` selects its reverse heap search; it does not clear the allocation. Closing 9027C requires its heap state at 103D50 and its actual A7098, 90D40, 9D93C, A3074 and A54BC dependencies. Those dependencies are not silently acknowledged here.

## Layout and exact initialization

All size and pointer arithmetic wraps at 32 bits. Glyph, ID and packet capacities use signed low 16 bits; font and text counts use unsigned low eight bits. The spans are `glyphs*20`, `fonts*512`, `texts*64`, `IDs*2`, and `packets*160`. The total is these spans plus `0x58` and the signed packet count for its byte bitmap. Negative spans remain wrapped values, not repaired counts.

The initializer writes these six style-relative pointer words in order:

| Offset | Original destination |
| --- | --- |
| +18 | style + 58: packet pool |
| +08 | packet pool + packet span: glyph descriptors |
| +10 | glyph descriptors + glyph span: 64-byte text objects |
| +0C | text objects + text span: per-font 256-halfword maps |
| +14 | font maps + font span: ID map |
| +1C | ID map + ID span: packet bitmap |

It next writes halfword `7FFF` at +28, then halfword `FFFF` at +2C, +2E, +3C, +38, +34, +30, +3E, +3A, +36 and +32, in that order. It writes zero halves at +26, +2A and +24; low-eight-bit mode at +52; low-eight-bit text capacity as a half at +22; zero byte at +53; and zero half at +40.

It fills each font-map half with `FFFF`, each positive-count ID-map half with `FFFF`, and only halfword +12 of each 64-byte text object with `FFFF`. The base pointer is reread from the style on every iteration, so aliases can redirect later writes. Finally it publishes the style to global B2048, writes the raw low halfword packet capacity at style+20, and clears the positive-count packet bitmap byte by byte, again rereading its base each time.

For the ordinary call with no lock, this is 2,162 CPU stores and one allocator event. Pool contents not written by those operations keep their incoming bytes and knownness. In particular, style colors +0/+4, spacing/kerning +42..+51, header padding, glyph descriptors, packet contents and all text-object bytes except +12/+13 are untouched. An unknown allocation must not become implicitly zero-filled or wholly known.

## Original quirks and native refusal

The implementation deliberately preserves these source behaviors:

- 90160 dereferences a NULL descriptor without checking it. A NULL style is likewise used by 2E200 without a fallback. A missing retained region produces a native resource refusal at the reached access, not an invented successful allocation.
- Argument six is ignored, and mixed signed/unsigned widths can produce negative or wrapped spans while some initialization loops still execute.
- The lock pointer is read again after the allocator. Unlocking a different target, or the returned descriptor, is possible.
- B2048 becomes visible before the last capacity and bitmap stores. A later refusal retains that publication and all earlier stores.
- All four initialization-loop bases are live. A directed alias fixture places style+1C at B2048: publication changes the bitmap base, then its byte-28 clear changes the pointer again. This surprising source behavior is preserved rather than replaced with a detached fill.
- The incidental terminal `v0` is zero for a positive packet count and its sign-extended low halfword otherwise. It is not the style pointer; use `progress.style` for that value.

Only reached bytes have their knownness validated. An invalid knownness value above one is an argument error, including later bytes in an access whose earlier bytes are unknown. Unknown write destinations are allowed and become known for precisely the bytes stored. Unknown reads, unowned addresses, source alignment traps and exhausted journal capacity stop before that operation and retain the earlier prefix. Journal capacity is checked before losing a store or callback event. A stopped invocation is not resumable or automatically rolled back.

Host publication must stage the entire mapped CPU memory and allocator state together, then publish only a complete invocation. Callback-internal effects belong to the callback owner's receipt; acknowledging its outer event does not claim those effects were implemented by this module.

## Font and label integration

The actual next font loads are ZKERN12, ZKERN10, ZKERN22, ZOVLFONT and ZBLACK12. The frozen font loader can consume this initializer's actual B2048/style+8/style+C producers when its own resource and upload boundaries are fulfilled. No font or upload work is claimed by the pool initializer.

The older restricted player-label bridge preflighted all 0x54 style bytes and all 64 returned text-object bytes as known. Those requirements are incompatible with the actual partial initialization above: unused spacing/kerning entries and untouched object fields remain unknown. Integration must validate each reached read and import actual write receipts; it must not clear pools to satisfy that older preflight. Public tests explicitly retain unknown style+0, style+42, packet bytes and object+0 after a successful ordinary initialization. The separate label-owner work closes that bridge limitation.

## Verification

`tests/game_text_pools_tests.cpp` passes 2,326 checks in each strict MSVC `/W4 /WX` Debug and Release build. It covers the ordinary allocation/layout and unknown-byte footprint, raw widths, unused argument, NULL results, changed lock targets, missing/refused callbacks, alias-driven pointer changes, reached knownness/alignment errors and journal prefixes.

Private `text_pools/verify.py` independently executes original R3000 instructions, including load and branch delays, and compares the portable owner in both configurations. Each runs 571 cases, 214,876 ordered store/callback events and 1,511,215 original instructions, covering all 192 owned instruction addresses. Comparisons include all mapped final bytes and knownness, return value, completed prefixes and refusal PC/address. Cases include 512 mixed-width randomized inputs, ordinary lock variants, explicit mutable allocator outcomes, journal limits and the B2048 alias case. Only 9027C is an explicit fixture callback; the proof does not claim its heap implementation or a natural cold entry.

Private source exports, build logs, `proof.json`, `HANDOFF.md` and `freeze.json` are under `.local/verification/native_completion/text_pools/`. No retail code or data is embedded in the public tests.

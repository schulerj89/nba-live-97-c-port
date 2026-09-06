# GAMEONLY speech resource initializer recovery

`nba97_game_speech_initialize` owns the complete GAMEONLY routine at
`0x8007FD40..0x800800F7` (952 bytes, 238 instructions). The boundary is based
on fresh Ghidra evidence with SHA-256
`d1ffbb2ca744f160f0a0b7f37955b936b64cbb7adb0117a5f70a6a70843d9a94`.
The recovered match initializer reaches it at call PC `0x8002DBD8`.

The owner reads the language before forming its `0x38`-byte frame and selects
the language-specific auxiliary event and non-event names. It publishes those
two loader results at `0x800FE9C8` and `0x800FEBDC`, rereads the live language,
loads the corresponding speech index with flags `0x20`, installs callback
address `0x8007FB24`, and opens both auxiliary payloads.

It then issues the original 100 lookup calls into 12-byte records beginning at
`0x80102FE0`: four team records, four records for each of twelve home/away
roster slots, and 48 category-two records. Team IDs use unsigned halfword
loads. Roster IDs use signed halfword loads and the byte at roster offset `+7`
uses a signed byte load. Records 100 through 109 receive the original `-1`
sentinel in field `+4`.

The first 100 records are scanned in address order. A non-null field `+0`
causes field `+4` to be added to the wrapping 32-bit allocation size. The
unchecked allocation result is published at `0x800FEABC`. Each non-null record
is copied to the current destination, its destination is published in field
`+0`, its length is reloaded after the copy, and the conversion result replaces
field `+4`; the destination increment occurs in the conversion call's delay
slot. The field-`+4` cursor is initialized once at `0x80080074`; later
iterations use only its `+12` branch-delay increment, so a copy or conversion
child may relocate it independently of the record cursor. The live saved index
payload is released last.

All 32 GPRs carry one knownness bit per guest byte. Pure arithmetic retains
partial knownness until an address, branch, or `JR` consumes it. Mapped reads
and writes enforce guest alignment, little-endian order, 32-bit wrapping, and
the supplied knownness plane. Every child receives the full register file after
`JAL` and its delay slot and may mutate every GPR, mapped bytes, saved stack
words, and loop cursors. Refusal and operation-budget results expose the exact
completed prefix; no partial run is resumable.

The narrow adapter routes compatible `0x80029BFC` calls through the complete
recovered retry-loader owner with the same full-register mapping used by the
audio initializer. It does not duplicate the retry algorithm. Missing or
incompatible loader state falls back to the caller's typed full-GPR service.
All other original children remain explicit typed dependencies.

The asset-free focused test generates all memory at runtime and covers all
three language choices, a language change between the two source reads, the
12/48/10/100 loops, full and sparse tables, signed IDs and category bytes,
every operation-budget prefix, callback refusal, partial stack/language
knownness, alignment, malformed knownness, overlap, and final register restore.
Every child-call budget prefix compares all 32 GPR words and knownness masks
with the successful run's source call boundary, and each of the ten epilogue
load prefixes compares the full register file while restoring a relocated
stack in the original `ra,s8..s0` order.
It relocates the field-`+4` cursor from the first conversion callback and proves
the next length read, its full-GPR prefix, and its budget-stopped prefix use the
relocated address plus twelve.
The integration test composes the recovered retry loader and invokes the owner
through the natural recovered match initializer with its production zero-fill
adapter. Both tests compile with MSVC `/W4 /WX` in Debug-compatible and
optimized configurations.

Visual classification: `Gameplay shown: NO - no direct visual effect`. This
routine changes retained CPU speech-resource state. Synthetic loader, allocator,
copy, conversion, and release services prove orchestration only; they do not
claim audible speech or an advancing rendered match.

Manager review corrected the packing-loop cursor placement: `s1=s0+4` at
`0x80080074` runs once; subsequent back-edges preserve a callback-relocated
`s1` and apply the `0x800800B8` increment. Regression tests cover that relocated
length table and its failure prefix, full-GPR call budgets and epilogue loads.
Manager verification passed 2,134,687 direct checks, 3,458 natural integration
checks, 219/219 asset-free CTests and 5,008 private original-instruction
comparisons covering all 238 instructions, full memory/GPRs, child-entry
registers, all budget prefixes, three languages and sparse/full packing.

The native input-driven run `game-entry-20260905-190426-f44f7601` reaches this
owner through the retained match initializer and its existing roster bindings.
`speech_initialize_verified.json` records 684 operations (416 reads, 91 stores,
177 calls), three recovered retry-loader wrappers, 100 lookup records, 34
checked copies/conversions, a 340-byte allocation, ten sentinels and release of
the index pointer. All payloads and service responses are generated fixtures;
no retail speech or audible output is claimed. Before/after scanout SHA-256:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d` (identical).

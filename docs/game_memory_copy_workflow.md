# GAMEONLY optimized memory-copy recovery

`src/recovered/game_memory_copy.c` owns all 200 instructions at GAMEONLY
`0x800AA468..0x800AA787`. A fresh read-only Ghidra export identifies 71 direct
references and hashes the 800 source bytes as
`2d9ed18f5de6fe3edc1fab9996769b418452b1c32eb3fd2cce7ed1f2b0c2350d`.
The function is an optimized, overlap-safe byte copy, but it is not replaced
with host `memmove`: source-visible access order and register behavior are part
of this recovery.

Main reaches the newly composed owner at call PC `0x80029B94`:

```c
/* Original addresses identify offsets in retained PS1 memory, not host
 * pointers. The adapter maps them into owned native byte buffers. */
Nba97GameMemoryCopyContext copy = {
    memory,
    3000,
    loaded_feload,       /* 0x80123400 in the diagnostic */
    0x801E0000,
    loaded_feload_size  /* 0x1410 / 5136 bytes */
};
nba97_game_memory_copy(&copy, &memory_copy_progress);
```

The successful diagnostic file service owns a deterministic 5,136-byte
FELOAD payload whose first word is entry `0x801E1410`. The preceding recovered
heap-size query obtains `0x1410` from the retained allocation descriptor.
`AA468` then performs 1,284 reads and 1,284 stores—2,568 mapped accesses—and
main reads the entry from the destination rather than receiving another
hard-coded result at the copy boundary. This is required for the native C port's
recovered startup composition: the addresses remain source provenance while
all dereferences occur through validated native storage.

The owner preserves the complete source schedule:

- signed address comparisons select forward or backward copying;
- trapping signed `ADD` at `0x800AA65C`, `0x800AA66C`, and `0x800AA670`
  calculates overlap endpoints;
- the aligned forward path copies 64-byte groups as eight loads/eight stores
  twice, then uses 16-, four-, and one-byte tiers;
- unaligned words execute the original little-endian `LWL`/`LWR` and
  `SWL`/`SWR` pairs as separately observable accesses;
- backward 16-byte groups load four words before writing four words, and even
  an aligned backward tail redundantly reads and stores each word twice through
  the partial-word instructions;
- unknown bytes propagate byte-for-byte when the destination exposes a
  knownness array; completed prefixes survive mapping, metadata, or budget
  refusal; and
- `v0` returns `(source | destination) & 3` for a forward copy, or the same
  alignment bits from the advanced end pointers for a backward copy. It does
  not return the destination like standard C `memmove`.

Original edge behavior is deliberately retained. Count arithmetic wraps at 32
bits and loop tests are signed. In particular, a direct forward call with
length `INT_MIN` subtracts 64, wraps to a large positive count, and attempts an
enormous copy. The native operation budget exposes an exact prefix without
pretending the original terminates. Signed endpoint overflow remains an
explicit arithmetic-trap result rather than undefined C behavior or a repaired
copy. A zero length performs no memory access and returns the alignment bits.

`tests/game_memory_copy_tests.cpp` performs 43 public checks covering the
natural FELOAD copy, every direction/alignment family, grouped snapshot order,
overlaps, exact partial-word traffic, per-byte knowledge, every failure class,
zero length, both signed endpoint traps, and a bounded `INT_MIN` prefix.
`tests/game_main_tests.cpp` composes the owner at `0x80029B94`, verifies all
5,136 destination bytes, and proves the following indirect call reads
`0x801E1410`. A private differential sweep executes every one of the 200 source
instructions across 1,369 cases and compares 159,814 ordered source accesses,
results, working registers, and complete retained-memory outcomes with the C
owner.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup using the test's recovered native input handlers—never computer-control
clicks—then reaches recovered main. It writes the exact memory-copy receipt
into `game_entry_trace.json`, plus
`feload-memory-copy-before.ppm` and `feload-memory-copy-after.ppm`. Those images
must be pixel-identical because this function changes CPU overlay memory, not
VRAM or scanout; in-memory byte snapshots must differ before/after, the receipt
records the changed entry word, and the final destination must match the source.
The complete 140-frame run proves native
reachability and absence of a direct visual effect, not a retail FELOAD asset,
production filesystem loader, playable court, or possession.

# GAME50768 ordinary body geometry

`game_body_geometry.c` recovers complete GAME50768: all408 instructions from
`80050768` through `80050DC4`. It normalizes five consecutive player contexts
from the actual ordinary V/W format. It has no external callee boundaries.
This is separate from the FATL preview decoder, the GAME504A8 name-polygon/UV
tail, any allocator, and GPU ordering tags or rendering.

## Retained references and caller integration

The caller supplies a fixed registry of retained allocations, byte storage,
optional per-byte knownness, and one tagged reference cell per original word.
Registry identities are **zero-based**; `{allocation=0,offset=0,known=1}` is a
valid reference. Canonical unknown is `{0,0,0}` and is not a source null pointer.
Aliases must use the same allocation identity. Registry entries and backing
arrays remain fixed during a call; deep copies copy both bytes and reference
cells and then rebind the buffer pointers.

For the ordinary GAME504A8 caller, retain the ten-context TLST allocation,
home V allocation, away W allocation, and the two ten-matrix root allocations
identified by source globals103FD8 and10B2B8. Each context isBCC bytes and each
root matrix32 bytes. Root contents may remain unknown:50768 only stores their
references. It does not read the matrices or claim that they were initialized.
The normalizer accepts already owned logical buffers; it does not validate a
loader trailer, CRC, resource name or natural heap placement.

1. Supply home-context offset0, home-body cursor offset8, the actual body+0/+4
   count globals10423C/FC618, and physical-base FEBE0=0. Run50768.
2. On the same candidate allocations, supply context offset`5*BCC`, away-body
   cursor offset8, refreshed away counts and physical-base5. Run50768 again.
3. Publish only when both calls completed. Keep the source write journals for
   diagnostics. A failed call preserves its exact supported prefix; progress
   is not a resumable cursor. Discard or inspect the failed candidate.

These globals are separate typed source values, not inferred by the C leaf
from convenient body bytes. The bridge must obtain them at the actual caller
boundary. The input's scalar-global slots and the retained byte allocations
are disjoint; aliases into those scalar slots are outside this native domain.
The source's returnv0 is0. The final a1 cursor is reported separately.

## Exact stores and source quirks

Each side's first player consumes XYZ data and writes its header+4 reference.
The next four players alias that same side-player's corresponding XYZ data;
they do not consume another XYZ block. Both packet banks remain distinct.
The six alternate groups share XYZ from primary parts1,2,3,5,6,7.

All20 parent references in both banks preserve the original tree. The three
roots are parts0,4,8. Root index arithmetic is the wrapped32-bit
`(physical_base + side_player) << 5`; no index clamp substitutes another root.
The part1 header/group reads at50B54/50B58 remain before the last six alternate
parent stores, as in the source.

Descriptor fields08/0C refer to the two scratch-word tables;10/14 hold primary
and alternate scratch indices;18/1C and20/24 hold their A/B corner tables;
28/2C/30/34 refer to four assembled-packet banks. A scratch index is shifted
left2 modulo32 bits before adding the retained table offset.

Original corner relocation first stores `A & FFFFFF`, then `B & FFFFFF`, then
stores relocated A, relocated B, and the scratch-word reference. These five
events remain separately ordered. **Both banks use A's captured high-byte
group**, even when B names a different group. That source quirk is intentional
and commented in code; interpreting B independently would be a port defect.

The source reads counts again after stores. Cursor increments and count scales
wrap at32 bits. Relocation tests the signed value of wrapped`3*count`, both at
entry and after each iteration; it is not an unsigned count loop. For example,
raw count80000000 skips relocation while its12/32-byte cursor scales wrap to0.
No count, group, offset, sentinel or previous normalization is silently fixed.
Skipped marker words are never read. There is no invented20/6 group bound:
an unchecked group offset can reach another owned word, where the actual
value/tag determines whether execution can continue.

Relocated pointer words live in reference cells, never in raw numeric host or
fabricated PS1 pointers. Pointer stores clear the four raw bytes to unused
metadata, marking them unknown if a knownness array exists. Scalar stores
clear the tag and write known little-endian bytes. Consuming raw bytes while
ignoring their reference cell is an integration defect.

## Supported-domain refusals

The original50768 performs no buffer or metadata validation. Native guards
check only reached LW/SW accesses, preserving the already executed prefix:

| Result | Native boundary |
| --- | --- |
| `UNKNOWN` | A required scalar or dereferenced identity is unavailable. |
| `BOUNDS` | Reached allocation, byte span or cell storage is not retained. |
| `ALIGNMENT_UNKNOWN` / `ALIGNMENT_TRAP` | Original allocation-base modulo4 is unknown / a reached word is unaligned. Native heap alignment proves neither. |
| `REFERENCE_REQUIRED` | A reached pointer load has only raw scalar bytes; serialized pointer-looking values are not promoted to references. |
| `ADDRESS_REQUIRED` | A reached scalar load observes a tagged known reference and needs its unavailable numeric original address. This includes re-normalizing already relocated corner words. |
| `ARGUMENT` | Reached metadata is noncanonical or entry object pointers are invalid. Every reached four-byte knownness span is scanned, including writes. |
| `JOURNAL_LIMIT` | The next store would exceed the caller's event capacity. No source store is performed without its event. |

Offset arithmetic wraps before a reached bounds check. A stored reference can
be one-past or outside its target allocation:50768 does not dereference those
targets merely because it stores them. Target contents and alignment are
therefore not preflighted. Unknown reference copies remain unknown. These
restrictions preserve an explicit native boundary; none is described as a
new validation branch from the original game.

## Evidence

Public synthetic tests cover both banks, all parents, side-player XYZ sharing,
B-group disagreement, one-past targets, wrapped/signed counts and indices,
unknown roots, allocation aliases, a leading partial alignment cell, reached
whole-span knownness validation, repeated normalization and journal prefixes.
Strict MSVC `/W4 /WX` builds run in Debug and optimized Release.
Each build passes32,495 public checks.

Private evidence is in
`.local/verification/native_completion/body_geometry/`. `verify.py` executes
the original GAMEONLY bytes through the independent instruction interpreter,
then compares every native journal event's PC, destination, size, value and
order and every final allocation byte after declared reference projection.
The fixture addresses are explicit isolated inputs used only by that proof;
they are not production addresses or observed game heap placements.

For each build: four real VATL/WBOS and VBOS/WATL resources cover all408 PCs,
746,624 original instructions and90,160 exact stores. An additional sequential
home/away candidate comparison checks45,080 stores. There are256 synthetic
original-instruction comparisons (1,735,648 instructions,271,735 stores),
including raw signed/wrapped counts, arbitrary corner offsets/scratch indices,
divergent B groups and physical-root indices. Twenty-four journal limits also
compare complete prefix buffers against original stores. No SDK, GTE, GPU,
allocator or device callback is installed by this proof.
An additional complete original comparison places context and body views in
one retained allocation identity, checking their reference aliases.

The original function byte SHA256 is
`f89c7434367d08b1d737e0e4d3000b3f32ed29eff4fd056fa0f5076bf1f341d1`.
The private freeze receipt records the public file and proof hashes. Root's
integration adds this C owner and its test to CMake; this task makes no shared
build, Git, host or UI changes.

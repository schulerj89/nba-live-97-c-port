# Gameplay player bindings: GAME646A8

2026-08-31. `src/recovered/game_player_bindings.*` implements the direct writes
of646A8 and records its four ordered tail-call requests. This is a bounded native
owner, not a complete period initializer or playable match. The tail helpers are
not replaced with no-ops in native gameplay; the caller must implement and run
them before declaring the646A8 boundary complete.

| Source owner | Full instructions | Native scope | Still required |
|---|---:|---|---|
|800646A8..80064910 |155 |Active references, inverse lineups, entity links/ratios, exact divide-trap prefix and tail requests |Caller integration and actual tail execution |
|8006459C..800646A4 |67 |Request only |Opponent classification, header+61,64388 sort, inherited-register quirks |
|800644FC..80064598 |40 |Request only |Header+A6/+A8 classification and inherited-register quirks |

The155-instruction denominator includes three unreachable signed-overflow
diagnostic instructions at648A4/648A8/648AC. They cannot execute because the
divisor is loaded withLBU, never-1. The zero-divisor BREAK at64894 is reachable
and is represented, not silently removed. Instruction coverage is evidence of
the original oracle, not an instruction-identical native binary claim.

## Consumed data and ownership

The input owns a copy of each current12-halfword lineup (header+16..2D), the two
12-entry player alias tables (20B8C/20BBC), the actual player-record+9 bytes, ten
entity-table references (20BEC[0..9]), and the consumed entity fields. Entity
identity, table position and entity word+0 are distinct. The core supports exact
entity-table aliases and arbitrary valid binding indices; it does not assume a
permutation or silently substitute the loop index for entity word+0.

Player references are indices into caller-owned `player_byte9` storage. Values
must come from the actual resolved records; ordinary `PlayerRecord::height_inches`
represents byte+9, while accepted roster aliases/order still need to be preserved.
Do not synthesize a record for an unresolved player or re-read stock rosters.
Shared player references do not merge the separate per-side status identities.

Native statuses use indices0..23: home0..11, away12..23. These correspond to the
original bases1F7EC/1F984 and stride22(hex), without converting original addresses
to host pointers. Caller-owned entities similarly use indices0..9. The C module
does not own full original records or erase fields it has not recovered.

## Ordered direct effects

1. For local slots4..0, home then away, read the first five signed lineup values
   and bind the selected player/status references. Output bindings0..4 are home,
   5..9 away. Original pointer arrays areFDC98 (players) andFDC70 (statuses).
2. Clear both12-entry inverse maps at header+80..97 toFFFF. Traverse the complete
   lineups0..11, home then away. Negative signed halfwords are skipped; each
   nonnegative slot writes its traversal index into that slot's inverse entry.
   **Original quirk:** duplicate slots are allowed and the last occurrence wins.
3. Traverse source entity-table slots9..0. Resolve the actual owned entity and
   use its word+0 to index the active binding arrays. Its direct fields are:

| Destination | Value / source operation |
|---|---|
|Entity+38,u16 |Low16 of word+0 minus unsigned byte+D9; preserve underflow |
|Entity+1C,reference |Status binding indexed by word+0 |
|Entity+20,reference |Player binding indexed by word+0 |
|Entity+C6,u16 |`floor(player_byte9 * 256 / 78)` |
|Entity+C8,u16 |`floor(19968 / player_byte9)`, only after nonzero check |
|Resolved opponent+CC,u16 |Low16 of current entity word+0; opponent comes from signed+D6 through the actual entity table |

The C6 formula replaces only the compiler's signed multiply-high sequence
usingD20D20D3, add and arithmetic shift6. It is verified over all256 byte inputs.
No clamping or floating-point conversion is used. TheCC store is the648DC branch
delay-slot write; repeated destinations keep the final write in descending
table traversal. Entity-table aliases are retained by identity.

Each native entity effect has a write mask. Unset members are absent effects,
not values with which the caller should clear original/native state. This matters
on both aliased tables and source traps. The core does not reset claims, player
positions, animation, clocks, controllers, status payloads or other header bytes.

## Source divide trap and native guards

For player byte+9 equal0, original64888 executesDIV and64894 executesBREAK1C00.
The source has already written+38,+1C,+20 and+C6=0 for the current entity, all
earlier visited entity effects, and the complete active/inverse binding maps.
It has not written the current visit's+C8 or opponent+CC, and none of the four
tail helpers run. `NBA97_PLAYER_BINDINGS_DIVIDE_TRAP` publishes that exact prefix
with the trapping table slot and visit count. This outcome must remain visible;
do not report successful initialization, insert a default height, or replace the
reciprocal with zero. Earlier opponent visits may already have writtenCC on the
trapping entity, which is retained by its write mask.

Native guard failures are different: they leave the output unchanged. The guarded
input domain requires actual first-five lineup values0..11, remaining lineup
values0..11 or any signed-negative halfword, bounded active player references,
and consumed table/entity/opponent indices0..9. Original unchecked accesses
beyond those arrays are not given invented values or interpreted as host memory.
Only reached entity/opponent references are checked; a source zero-divisor trap
stops before later/opponent references are consumed. First-five alias references
are validated while constructing the complete bounded active catalogue.

All writes are confined to the effect output after the consumed input has been
read. Input/output overlap and output/byte9-storage overlap are supported. The
caller must still provide valid typed storage and extents; the core performs no
allocation and reads no files.

## Tail calls and integration boundaries

On `NBA97_PLAYER_BINDINGS_READY`, apply the direct effects and execute exactly:
6459C(0),6459C(5),644FC(0),644FC(5). The request array is a dependency contract,
not evidence those callees have run. No such requests are emitted after a trap.

The first6459C call receives originalt6=8001F984 from this owner's earlier status
address setup. Its zero-rating fallback ultimately writes low byte84 to header+61
if all five own player+17 values arezero. The output records that byte explicitly.
**Do not repair this source register-leak quirk by selecting player0.** Later
6459C/64388 and644FC inherited-register dependencies must be recovered in the
actual callee chain, not initialized to fabricated defaults here.

646A8 is called repeatedly during initial/period preparation and later lineup
changes. Use the caller's current lineup and live entity word+0/+D6/+D9 fields
on every call. In particular,649D8 substitution calls646A8 before later65070
iterations inspect the updated inverse map; do not postpone that map write until
the whole lineup operation returns. Preserve earlier655B0/65328 snapshot receipts
instead of overwriting them with this later stage.

Build integration: add `src/recovered/game_player_bindings.c` to the relevant
target. The independent public test target consists of
`tests/game_player_bindings_tests.cpp` plus that C file, using C99/C++17 and
include directory `src`. No changes to the frontend model/mocap parser are needed.

## Verification and remaining limits

The public tests use invented records only:16089 checks cover both sides,
all256 divisor bytes at each of ten visits, zero-trap prefixes, signed lineup
values, duplicate inverse writes, player aliases, entity-table aliases and
permutations, opponent collisions, native guard failures and overlapping buffers.

Private `.local/verification/native_completion/player_bindings/compare_original.py`
executes the original GAMEONLY bytes and compares the compiled native core in
2819 cases, including19 zero-divisor traps. The full152 reachable owner PCs run.
The comparison checks the exact original nonstack write footprint and every
nonstack RAM byte after projecting native reference indices to fixture addresses;
this includes untouched player/status/entity bytes and the ball entity. Tail
helpers are only recorded at their original call boundaries in this oracle.
They are not counted as implemented. Debug and RelWithDebInfo receipts include
source hashes, named trap cases, instruction coverage and explicit scope limits.

No current result establishes complete65DB0/659F0 execution, original live-game
timing, a complete gameplay roster bridge, a native match scene or a possession.
Those remain separate integration and verification gates.

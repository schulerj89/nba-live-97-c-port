# GAMEONLY substitution candidate selection recovery

## Boundary and evidence

This recovery owns only `0x80064DBC..0x8006506F`, 692 bytes and 173
instructions. The fresh Ghidra listing is
`.local/evidence/tipoff-recovery/game_80064dbc.txt`, with instruction SHA-256
`38ae4643f2ff42c752636fec94c44376539e8fb575b2d697ff81500a7c8ad277`.
Known callers are `0x800659C4`, `0x80066CCC`, and `0x80066D48`. Repository
search found no complete owner. The existing substitution module owns the
different `0x800649D8` routine and remains unchanged.

## Source behavior retained

The owner performs five scans in source order. The first compares the unsigned
player byte with full `a1`. The next two compare it with signed rank-table bytes,
so ranks `0x80..0xFF` cannot equal the corresponding unsigned player value. The
fourth scan ignores player identity when the signed injury is below `0x2000`.
The fifth runs only for negative injury and accepts a nonnegative bench status.

Each pass retains its exact count load and latch. The fifth pass rereads count
after every rejected candidate. Inverse-lineup and status thresholds use signed
halfwords, counts remain unsigned 16-bit values up to 65535, and all pointer
increments and delay-slot shifts wrap as 32-bit operations.

On a hit, the candidate halfword becomes `a2`, JAL sets `ra=0x80065040`, and
the incoming `a3` retained in `t5` is stored at live `sp+0x10` in the delay slot.
The typed `0x800649D8` child sees the full machine and five semantic arguments.
After acceptance, the source forces `v0=1`, then reloads `ra` through
callback-live `sp` and releases the 0x48-byte frame. No-hit exits return zero.

## Verification

The BR adapter claims the exact `0x800659C4` call, delay `0x800659C8`, entry
`0x80064DBC`, invocation one, four register arguments, and `ra=0x800659CC`.
Any assigned identifier triggers full validation; wholly unrelated BR children
go to the typed fallback.

Focused tests cover every pass hit and no-hit exit, home/away side selection,
full player identifiers, signed ranks/statuses/injuries, counts 0/1/12/65535,
fifth-pass count reloads, all hit-path budgets, callback/refusal prefixes,
fifth-argument store ordering, callback-live GPR/HI/LO/SP, saved-RA masks and
alignment, malformed atomic loads/stores, mapping, overlap, stack wrap, full
untouched-register masks, and deterministic memory/knownness/machine results.
The natural test composes the actual team-strategy owner with this owner and a
typed `0x800649D8` child, including the branch that never invokes BU, nested
budget failure, and malformed assigned-event guards.

The manager's independent original-instruction differential passed 19,840
cases across all 173 PCs, all 34 CPU words and masks, the full 2 MiB RAM image,
and every callback entry machine after the V0/delay-slot corrections. Receipt:
`.local/evidence/tipoff-recovery/substitution_candidate_select_differential.json`.

Gameplay shown: NO - no direct visual effect.

Manager verification: 479 focused checks, 25 actual-strategy integration checks,
and all 337 asset-free CTests passed. Required caller support also corrects
the existing strategy owner's known misaligned JR trap, observed only after
its count store, S0 reload, and stack release; the natural test passes a
malformed outer return address through the accepted substitution child.

Native input-API run `game-entry-20260906-054555-b62ec245` completed 98 frames.
The same retained initializer/reset/header/strategy memory reaches selection
with count 3, injury 0xFFFE, and the still-cleared inverse lineup. It returns
zero after 27 operations (26 reads, one stack store), without fabricating a
candidate or calling the unresolved substitution bridge. The strategy still
decrements count to two, preserving the source's no-hit quirk.
Both CPU proof frames have SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The visible screen remains User Setup; no advancing native match is claimed.

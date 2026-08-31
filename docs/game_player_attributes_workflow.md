# Gameplay player attributes

`game_player_attributes.c` recovers all127 instructions of63EDC and the11-instruction51ED8 height writer. It closes the route where actual flag21498 equals zero. Nonzero flag21498 requests three still-external tails in source order:4D9EC,35A44,38A18(FFFFFFFF). `TAILS_REQUIRED` is not completion; apply the preceding effects before executing those callees.

The source reads20BEC[0] once, then visits ten contiguous F4-stride records. The bounded native physical pool contains eleven entities, including the ball, so starting at0 or1 can be represented without inventing separate table lookups. A later out-of-pool visit stops with its exact earlier effects retained. Each entity consumes its existing word00 and bound player reference. Neither is generated from its physical identity or table slot.

51ED8 stores player byte09 times624 into the height table at165F48. The source shifts raw word00 left2 before address addition, so high-bit wrap is preserved:80000000,40000000,andC0000000 all alias index0. Native ownership limits this table to eleven represented entries. Repeated IDs retain the last write.

The entity attribute writes occur in this order:

| Field | Source arithmetic |
|---|---|
|3A|unsigned player20 shifted left3|
|44|`(546 / (((player1E - 50) * 24) / 50 + 12)) * 36 / signed FDB64`|
|3C|arithmetic right shift of`99 - player1B` by1, plus44|
|3E|arithmetic right shift of`99 - player14` by1, plus32|
|40|`(player1C - 50) * 15 / 47`, truncating toward zero|
|42|`(player15 - 50) * 15 / 47`, truncating toward zero|

All fields store the low16 bits. Negative values, arithmetic-shift rounding, raw byte values above99, negative FDB64, and wrap are preserved. The first division really traps for player1E values23,24,25. A zero FDB64 also traps. Both retain the height write and field3A for the trapping visit, plus every preceding entity's effects. No clamp or replacement player is introduced. The original INT_MIN/-1 diagnostic traps cannot occur here: the first numerator is546, and the second is bounded to plus/minus19656.

Inputs carry knownness for the actual first physical reference, entity word00/player reference, divisor, and render flag. Unknown payload zero is metadata. A needed unknown or out-of-storage reference publishes an explicit refusal prefix; it does not become source success. For example, a rating divide trap occurs before the source reads FDB64, so it remains a trap even when the native divisor is unknown. Unknown flag21498 is reached only after all ten entity visits. Invalid representations return an argument error without changing output.

Only masked entity fields and height-table entries are writes. Unwritten members do not initialize original fields. Publication follows all consumed input reads, permitting input/output/player overlap. The host must not continue a caller after a source trap or unresolved prefix. For an atomic scene transition, stage the candidate and publish only a completed supported route.

The source entry2D8D4 explicitly clears21498 at2D904 before2DB90. The later48D5C render initialization sets it to1. An owned entry bridge can use the zero-flag route only after proving that sequence and preserving any intervening writes; it must not assume the flag is always zero during gameplay.

Private original-instruction comparisons in `.local/verification/native_completion/player_attributes/` cover3,143 cases, including103 source divide traps, raw rating bytes0..255 at each physical visit, signed/extreme divisors, physical starts0/1, arbitrary owned player references, high-bit ID aliases, and all-zero entity IDs. Exact nonstack RAM and write footprints match in Debug and RelWithDebInfo. The full138-instruction denominator is retained:136 execute, while63F98 and63FD8 are the two impossible overflow BREAK instructions.169 public checks pass in both private warning-clean builds. Optional tails are recorded, not semantically substituted; this is not a full-period runtime or new emulator claim.

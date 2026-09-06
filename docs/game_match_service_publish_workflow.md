# GAMEONLY match service state publication recovery

`nba97_game_match_service_publish` owns GAMEONLY `0x8002DE34..0x8002DE73`,
64 bytes and 16 instructions. The fresh Ghidra listing and the corresponding
64-byte `GAMEONLY.BIN` slice at load base `0x80015000` both identify
instruction-byte SHA-256
`a1df6b4753d88d815358610c1599c22296c972ddd6874312d286c8d1f14a3bc8`.
The known callers are `0x80068D7C` in the recovered match tick and
`0x800448F4`. The sole child is still-unowned `0x8002A264`, called at
`0x8002DE5C` with no arguments and a NOP delay slot.

The owner loads unsigned status from `0x800F9FFE` and signed phase from
`0x800FDB90` before allocating its 0x18-byte stack frame. It then saves `ra`,
publishes the status halfword at `0x80015028`, publishes the sign-extended
phase word at `0x800170BC`, invokes the typed child, reloads `ra` through the
child-mutable live `sp`, advances `sp`, and returns. This order is observable:
native backing aliases can let the stack save overwrite source bytes only after
both values have been captured, while either publication can overwrite the
saved `ra` bytes that the epilogue later reloads.

The retained-memory interface uses 32-bit guest addresses and little-endian
byte accesses. It carries one knownness bit per byte. `LHU` makes the upper two
zero bytes known; `LH` makes its upper sign-extension bytes known exactly when
the source high byte is known. Unknown source bytes therefore continue through
the destination stores. A destination without a knownness bitmap refuses an
unknown store because it cannot represent the source state. Region mapping,
guest alignment, wrapping `ADDIU`, source order, access journal entries, the
seven-operation budget, and every failure prefix remain explicit.

The typed child receives all 32 GPRs plus HI/LO after JAL writes `ra` and the
NOP delay slot completes. Its raw `v0` and `v1`, mutable HI/LO, GPR changes,
memory changes, and live `sp` are preserved. An unknown live stack address
refuses before the epilogue load. An unknown reloaded `ra` refuses at JR only
after `sp += 0x18`, matching source order.

`nba97_game_match_service_publish_from_match_tick` is the production adapter
for the actual `0x80068D7C` match-tick service event. The legacy tick callback
does not carry a register file, stack pointer, or HI/LO, so composition requires
an independently supplied full entry machine and validates the natural JAL
`ra` instead of inferring missing ABI state. Other match-tick services remain
typed test fixtures; the child remains a refusing dependency unless a real
implementation is supplied.

The focused runtime-generated tests cover status and signed-phase boundaries,
exact widths and neighbors, all machine forwarding and mutations, HI/LO,
call metadata and delays, aliases, per-byte unknown propagation, absent known
bitmaps, wrapping and unaligned stack addresses, mapping and metadata failures,
child refusal, live-SP and JR failures, and every operation-budget prefix. The
integration test drives the recovered match tick to its actual `0x80068D7C`
event through the production adapter with an explicit machine. No retail asset
or checked-in binary fixture is used.

Visual classification: **no direct visual effect**. This CPU-only owner
publishes retained service state and dispatches a typed service. It does not
draw or alter presentation state, so no gameplay or UI frame change is claimed.

Manager integration carries the period-expiry machine through the actual
68D74 BNE/NOP fallthrough and 68D7C JAL/NOP. It preserves the prior phase and
supplies only additional status/publication-field fixtures. Three native cases
publish status FFFF and phases 0/81/0 before the typed audio-service child.
Each executes 7 operations (3 reads, 3 stores, 1 call) and restores RA=68D84.
The next unbound native service is the already recovered player update at
0x80068D84 -> 0x8006801C, which still needs a compatible runtime binding.

Validation: 431 focused checks, 16 natural-caller checks, strict C99, and all
245 asset-free CTests passed. Private original-byte differential comparison
passed 4,800 cases across all 16 PCs, full memory/GPR+HI/LO, child state, every
budget, signed phase, and stack/publication aliases. The worker additionally
passed ASan/UBSan. Native before/after diagnostic frames match SHA-256
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d;
receipts prove the publications. Separate User Setup menu frames are ignored
local proof only. Gameplay shown: NO - no direct visual effect.

# GAMEONLY six-word random-seed recovery

`nba97_game_random_seed` owns the complete GAMEONLY routine at
`0x80093694..0x80093733` (160 bytes, 40 instructions). The boundary comes from
the fresh private Ghidra listing `game_80093694.txt`, SHA-256
`cd1d036806a765a225bd599d954d5dda486165a0e9cc194a758658eb3920e725`.
Its sole known caller is the recovered scene random warm-up at call PC
`0x800802D0`; the caller's `0x800802D4` delay slot supplies `v0 & 0xFFFF` in
`a0`. Repository searches found consumers of the six-word state but no other
complete owner of this seed routine. The adjacent `0x800935C4` random-step
routine remains owned by the camera recovery and is not duplicated here.

The leaf forms `a1=0x800C4AE8`, constructs each source operand through the
original separate `v0` and `at` LUI/ORI instructions, adds the two operands,
then cumulatively adds that increment into live `a0`. It stores the six
cumulative values at `0x800C4AE8,+4,+8,+0xC,+0x10,+0x14` in source order. The
increments are `E45A0E56`, `2C081893`, `7BE6B646`, `81BAE76D`, `2E647AE1`, and
`A352FBE7`; they are individual increments, rather than the decompiler's
cumulative constants. The first operand add is signed `ADD`, whose fixed
negative operands cannot overflow; the cumulative `ADDU` operations wrap.

All 32 GPRs and one knownness bit per little-endian byte remain visible. An
unknown seed byte can leave a later carry known or unknown independently, so
the owner propagates carry knowledge one byte at a time and writes partial
knownness into retained memory. A destination without a knownness array
refuses a partial store. Regions are nonempty, nonwrapping, and disjoint in the
guest address space; disjoint regions may intentionally share native backing,
which preserves ordinary alias effects without guest-pointer casts. The six
fixed store addresses are naturally word aligned. Missing mappings, malformed
knownness, budget exhaustion, and an unknown return address preserve the exact
completed prefix. `JR ra` is validated only after all six stores; its delay
slot is a NOP.

`nba97_game_scene_random_warmup_with_random_seed` is the production composition
adapter. It replaces only the proven seed event with this owner, sharing the
caller's live GPRs and retained memory. Scene startup, both random-value calls,
and the `0x800935C4` warm-up steps remain explicit typed services.

The focused synthetic test covers seeds `0`, `1`, `FFFF`, high-bit and
`FFFFFFFF`, every 16-bit caller seed, exact store order, final and prefix GPRs,
all seed-byte knownness masks, carry boundaries, memory-map failures, native
backing aliases, unknown `ra`, malformed arguments, and every store budget.
The integration test drives the recovered natural warm-up caller and proves
the `0x800802D0/0x800802D4` event, low-16 seed argument, six retained words,
and explicit remaining child calls. No retail asset or binary fixture is used.

Classification: `Gameplay shown: NO - no direct visual effect`. This routine
changes retained CPU random state; manager-owned native capture supplies the
matching state and frame-hash receipt.

Manager integration also exposes `nba97_game_random_seed_from_warmup` for an
actual full-GPR warm-up event. Both the composed warm-up test and retained
native scene capture use this same adapter. The native scene path now runs the
recovered seed owner; startup/random/step children remain explicit fixtures.
Adjacent `0x800935C4` already has an owner inside `game_camera`; its current
API does not expose the full register/access-prefix contract needed here, and
its algorithm is neither copied nor claimed newly recovered.

Manager verification: 528,455 direct checks, 84 integration checks, 215/215
asset-free CTests, and 65,575 original-instruction differential cases covering
all 40 instructions and every 16-bit seed. Native input-driven evidence in
`game-entry-20260905-184828-1fc1e185/frames/random_seed_verified.json` proves
all six ordered words and full A0/A1/AT/V0 state for seed `0xCAFE`.
The six stores publish `E45AD954,1062F1E7,8C49A82D,0E048F9A,3C690A7B,DFBC0662`.
Before/after scanout SHA-256 is identical:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.

# GAMEONLY opponent contact ordering recovery

## Evidence and boundary

This owner recovers exactly GAMEONLY `0x8005F888..0x8005F947` (inclusive), 192 bytes and 48 instructions, from the fresh Ghidra listing `game_8005f888.txt`. Its instruction SHA-256 is `b5842ef025359b52db720caa919e28d7aa305df4eb97167bb8592f55239c20d5`. The sole observed caller is the recovered actor-contact eligibility routine at `0x8005FA2C`. The sole direct child is `0x8005F3BC`, called at `0x8005F92C` with two arguments and a NOP delay. Repository search found no prior complete owner.

The child remains a typed full-machine callback because no compatible complete owner exists. The recovery does not translate or approximate that second routine.

## Preserved behavior

The owner allocates the 0x18-byte frame, retains the first actor in `a2`, and saves `ra`. A nonzero first actor C2 value skips the second C2 read. When both C2 values are zero, the source bypasses both the option and phase gates. Otherwise option values 1 through 255 reject after the branch delay clears `v0`; phase uses signed `<129`, so `0xFFFF` passes while 129 rejects, again after a delay that clears `v0`.

The second actor's DA bit selects the first ordering branch. Its delay always restores the first actor to `a0`. When that bit is set, the first actor's DA branch always publishes the second actor in `a0`; a zero bit then completes the swap by moving the retained first actor to `a1`. With both bits set, the routine reads the signed-half owner before the first actor's full-word ID. Equality restores the original pair in the jump delay, while mismatch keeps the swap. The child may mutate every register, HI/LO, SP, RA, retained memory, and saved frame word. Its `v0` is truncated to the low byte before RA reload through the live SP.

## Composition

`nba97_game_opponent_contact_from_actor_contact_eligibility` accepts only the actual full-machine event `0x8005FA2C -> 0x8005F888`, with delay PC `0x8005FA30`, two arguments, and known `ra == 0x8005FA34`. The integration fixture runs the frozen actor-contact eligibility owner, composes its unequal-team `0x8007066C` geometry call through the existing selection-distance adapter, and routes its unequal-team event into this owner. No caller ABI is inferred.

## Verification

The asset-free focused executable runs 274 always-active checks. Runtime-generated mapped-memory fixtures cover both C2 reads and the first-read short circuit; both-zero option/phase bypass; option 0/1/255; phase 128/129/`0xFFFF`; signed C2 values; every DA-bit pair; equal and unequal signed-owner/full-ID values including `0xFFFFFFFF` versus `0x0000FFFF`; exact child PC, delay, entry, two arguments, `a2`, and owner-before-ID access order; child return values 0, 1, `0x100`, and `0xFFFFFFFF` under all 16 knownness masks; all saved-RA and HI/LO masks; unknown C2/option/phase/DA predicates and delay publication; every operation-budget prefix; full callback GPR/HI/LO mutation, refusal, invalid state, moved SP, `a2`, and saved RA; unknown JR; mapping, alignment, malformed known bytes, null-known partial-store, overlapping-region, and wrapping-stack cases.

The natural integration executable runs 12 checks over the actual parent call, original and swapped pair arguments, low-byte propagation, nested refusal, and malformed adapter events without machine clobber. The recovered source is compiled separately with strict C99 `-std=c99 -pedantic -Wall -Wextra -Werror`; both C++ tests use `-pedantic -Wall -Wextra -Werror` and create all bytes at runtime.

An independent original-instruction differential passed 2,036 cases across all 48 PCs. It compared the full 2 MiB memory image, all 32 GPRs plus HI/LO, callback entry machines, coverage-corpus budgets 0 through 12, signed phase/owner boundaries, ID/frame aliases, and callback SP relocation.

## Visual classification

Gameplay shown: NO - no direct visual effect. This CPU routine selects contact argument order. The synthetic fixtures prove machine, memory, and callback behavior; they do not claim a live child implementation or rendered gameplay.
Manager validation: 274 focused checks (including saved-RA/actor-ID alias and unchanged bytes after rejected partial store), 12 natural integration checks, strict C99, and all 275 asset-free CTests passed. Native input-driven run game-entry-20260906-001608-7281391d captured 98 driver states. Actual eligibility/distance composition with independent actors passed owner7 versus ID100 to reverse actor arguments, preserved low-byte CD, and recorded 9 operations/7 reads/1 store/1 typed collision callback; SP801FEFC8->801FEFE0, RA8005FA34. Before/after CPU frame SHA-256 both 391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d. The visible frontend remains User Setup; this fixture is not advancing gameplay.

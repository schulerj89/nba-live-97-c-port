# GAMEONLY GTE reference transform recovery

## Boundary and evidence

This owner translates only GAMEONLY `0x80056650..0x80056677`, 40 bytes and 10 instructions. The source is the fresh Ghidra listing `.local/evidence/tipoff-recovery/game_80056650.txt`; its instruction SHA-256 is `b821ac83a86822b7f7f86d144c71f923b2252469c4e5be70d445dcb90af2a68b`. Known callers are `0x8004C2BC`, `0x80051228`, `0x80052184`, `0x800521B0`, and `0x8005251C`.

The ownership audit found restricted fragments in older camera, net, and player owners. Those fragments use fixed buffers and omit either the general second full-word load and its alias behavior or the final FLAG store through arbitrary `a2`. No existing standalone owner covered the complete machine boundary.

## Source behavior

The routine loads GTE data 0 from `a0+0` and performs a full four-byte load from `a0+4`. The latter validates the complete reached span, then sign-extends its meaningful low half into GTE data 1. It executes `COP2 0x480012`, the `RT * V0 + TR` transform with `sf=12` and `lm=0`, and stores raw MAC1, MAC2, and MAC3 at `a1+0`, `a1+4`, and `a1+8` in order. `CFC2` copies raw FLAG into CPU `v0`. The final `SW v0,0(a2)` executes in the `JR ra` delay slot, so it remains visible before an unknown return address refuses.

The owner retains all 32 CPU GPRs, HI/LO, 32 GTE control registers, and 32 GTE data registers with one knownness bit per byte. Apart from CPU `v0`, data 0/1, MAC1..3, IR1..3, and FLAG, all state remains unchanged. Guest address additions wrap as `uint32_t`; sequential reads and writes preserve input/output/FLAG aliases. Two reads, one hardware operation, and four stores form the seven-operation complete path.

## Hardware composition

The typed hardware boundary carries only the exact PC, command, operation number, invocation number, and retained GTE bank. It has no CPU-machine or guest-memory parameter. The production adapter maps fully known rotation controls 0..3, the meaningful known low half of RT33/control 4, translations 5..7, packed V0XY/data 0, and the meaningful known low half of V0Z/data 1 into `nba97::GamePlayerGeometry`. It calls the existing `GamePlayerGeometry::apply(NBA97_PLAYER_TRANSFORM)` implementation, then publishes only MAC1..3, IR1..3, and FLAG back to the explicit GTE bank.

Missing consumed input knownness returns `NBA97_TEXT_UNKNOWN`; malformed masks return `NBA97_TEXT_ARGUMENT`. The adapter validates the whole bank before composition and preserves raw unconsumed controls, screen/depth/FIFO data, and other GTE fields. The second input word and RT33 may have unknown discarded upper bytes only after the reached memory metadata and masks have been validated.

## Natural camera composition

`nba97_game_gte_reference_transform_from_camera_frame` composes the recovered camera-frame owner at the exact `0x80051228` call, `0x8005122C` delay slot, `0x80056650` entry, three-argument boundary, and fully known `RA=0x80051230`. It claims malformed events when either the assigned kind or entry matches, preventing an accepting fallback from handling a malformed assigned boundary. The binding carries the same full machine and mapped memory plus the retained GTE bank. Owner failures publish their exact machine, GTE, memory, and access prefix; invalid incoming binding state is rejected before invocation without replacing the caller state.

The natural fixture runs the actual `nba97_game_camera_frame_transform` owner. Explicit typed matrix, rotation-install, and translation-install children prepare the runtime bank, then this adapter executes the existing hardware owner. The camera owner observes transformed reference values and FLAG and writes its final translated camera words.

## Validation

Asset-free, always-active focused tests cover both raw loads and data-1 normalization; exact hardware and access metadata; all four stores and the `JR` delay slot; CPU/GTE preservation; callback refusal and malformed output; unknown pointers and return address; alignment, unmapped memory, malformed late knownness, invalid machine/state masks, absent hardware, unknown stores with `known=NULL`, aliases, wrapping addresses, truncated journals, and every operation-budget cutoff. Hardware cases cover identity and general matrices, signed extrema, wide 44-bit overflow, FLAG, IR saturation versus raw MAC, and preservation of unrelated GTE state.

The independent manager differential compared all 10 instruction prefixes through the production hardware adapter across 8,192 cases. It compared all 98 CPU/GTE words and masks, mapped memory bytes and knownness, aliases, budgets 0 through 7, unknown return behavior, overflow, and saturation; all cases passed.

## Classification

Gameplay shown: **NO - no direct visual effect**. This routine changes the retained GTE and RAM reference result. A renderer or later packet owner must consume that state before it affects a frame.

Manager integration composes the production matrix builder, rotation installer,
translation installer, and this reference owner on the same retained GTE bank.
The runtime synthetic packed table produces reference MAC [3, -1, 7] and the
actual camera caller then writes [7, 4, 13] to its RAM translation words. GTE
translation controls remain zero because the RAM tail runs after installation.
The composed test also proves the final-store budget prefix. Native capture
checks these results, unrelated GTE preservation, and identical CPU-only frames.

Manager verification: 583 focused checks, 132 natural/composed checks, and all
297 asset-free CTests passed. Strict C99 and progress, recovery, instruction,
and roster freshness checks passed. Native run
`.local/verification/team_select/game-entry-20260906-022638-67ebdd3d`
recorded 98 scripted states and identical before/after frame SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The displayed frontend remains User Setup; no advancing match was shown.

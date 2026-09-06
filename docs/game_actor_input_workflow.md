# GAMEONLY actor input and action dispatcher recovery

## Evidence and ownership

This owner recovers exactly `0x800686B8..0x80068BF7` (inclusive), 1,344 bytes and 336 instructions, from the fresh GAMEONLY listing `game_800686b8.txt`. The decoded instruction stream has SHA-256 `3e5499e7557606eb376943d7e759430c9193de244fdd3db9165e21404fe35c8d`. The only observed caller is the existing match tick call at `0x80068E8C`; `0x80068BF8` remains owned by that separate routine.

Repository searches found no complete owner for this range. Existing references to controller polling and several action addresses expose narrower service contracts rather than the complete 32-GPR, HI/LO, byte-knownness machine required at these call boundaries. The recovered owner therefore exposes all 27 source JALs as typed full-machine callbacks. This preserves each link register and delay instruction without duplicating any child algorithm or inventing an ABI.

## Native boundary

`nba97_game_actor_input` executes readable source-annotated basic blocks over mapped `uint32_t` guest addresses. It validates every memory access, preserves partial byte knownness through arithmetic and shifts, records operation prefixes, and lets callbacks mutate the live machine and guest memory. The 21-way action dispatch always reads the guest table at `0x800275C4 + state * 4`; only then does its typed indirect boundary select the matching source case. Unknown, unaligned, unsupported, and unmapped prefixes remain distinguishable.

The match tick's existing service event identifies `0x80068E8C -> 0x800686B8` but contains no GPR, SP, HI, LO, or knownness state. `nba97_game_actor_input_from_match_tick` therefore requires an independently supplied entry machine with known `ra == 0x80068E94`. It does not infer registers from the narrow event. Production still needs a real controller backend and a live match-machine bridge before this recovery can claim native player input.

## Behavior retained

The implementation reads the option byte before allocating the 0x48-byte frame, performs the `s0` save in the option branch delay, and applies the signed countdown gate with halfword wrapping. It walks ten live actor pointers, publishes actor and team globals for both halves, resolves controller pointers and the `+0x3F/+0x47` mapping, and preserves all five input call prefixes including the four arguments at `0x80068808`. Special-mode, phase, owner, motion, state, height, and flag gates retain their original signedness and delay behavior. Positive B6 values subtract the unsigned global delta in the jump delay store. The loop rereads actor claims, writes the live `FDC4C` controller flag, and increments the callback-mutable cursor in the branch delay. Epilogue loads use the live SP before its wrapping `+0x48` and restored-RA jump.

## Verification

The asset-free focused executable generates its mapped RAM fixtures and all 21 runtime table words in memory. It runs 675 assertions covering the 21 table-selected cases and exact callback PCs, entries, delay PCs, argument counts, and arguments; ten actors across both team halves; negative claims; controller deduplication and live pointer writes; both mapping offsets; option and countdown zero, negative, decrement, and wrap inputs; phase and side direction inputs; motion `0x2B`, repeat, DA bit 4, and special-mode gates; state 6/7/20/21/255; signed-positive B6 subtraction and wrap; live table mutation plus table unknown, unaligned, unsupported, and unmapped failures; every operation budget from zero through a complete representative path; SLTI's `0xE` known mask after its delay; all 16 saved-register, HI/LO, and restored-RA knownness masks; full callback machine mutation and refusal; invalid known bytes and metadata; unknown and unaligned SP; live index, cursor, SP, table, and controller mutation; and wrapping stack regions.

The natural integration executable runs the existing match tick until its actual `0x80068E8C` service call, supplies the explicit source-entry machine, executes all ten actor action callbacks, returns through `0x80068E94`, and lets the tick finish. Its 17 assertions also cover adapter call identity, argument and RA guards, absent-machine rejection, and nested refusal propagation. Both tests compile the recovered source separately as strict C99 with `-std=c99 -pedantic -Wall -Wextra -Werror`; the C++ harnesses use `-Wall -Wextra -Werror`.

## Visual classification

Expected visual effect: none directly. This routine polls and dispatches CPU input/action state. Synthetic execution proves instruction and boundary behavior, not rendered gameplay.

Manager validation adds unknown option/claim delay-publication and unknown
store rejection regressions: 679 focused checks and 17 natural-caller checks
pass, as do all 269 asset-free CTests, strict C99, progress and metadata checks.
Private original-instruction comparison passes 15,586 cases and all 336 source
instructions, including full mapped memory/GPR/HI-LO/callback state and operation
budgets0..899 over the coverage corpus.

The native input-driven verifier records a separate ten-actor CPU fixture with
195 operations (146 reads, 34 stores, 15 typed calls). Countdown1 becomes0,
the selected controller flag becomes1, and final actor/team publications are
`0x80110900`/`0x8001EEB8`. The runtime table selects case`0x80068A7C`;
frame SP is`0x801FEFB8`, returned SP`0x801FF000`, restored RA`0x80068E94`.
Both CPU frame hashes equal
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
This remains independent of a live match-machine bridge and physical input;
the captured application still displays User Setup.

Gameplay shown: NO - no direct visual effect.

# GAMEONLY ball acquisition recovery

`nba97_game_ball_acquire` owns the complete GAMEONLY routine at
`0x8005D140..0x8005D9EF`: 2224 bytes and 556 instructions. The source is based
on fresh Ghidra evidence whose instruction bytes hash to
`18065d144bc5c545b3706ddd3178f77b57d4a72f28632ddd4ea978c2324f175e`.
The implementation is organized by the original possession-change,
same-team, statistics, velocity, cleanup, and epilogue basic blocks. It does
not translate any child routine.

The entry reads `0x800FA034` before allocating its `0x30`-byte frame. It saves
`s1`, assigns the actor from `a0`, and then saves `ra/s3/s2/s0`, including the
`s0` store in the first branch delay slot. The optional phase `0x82` rule uses
the unsigned low byte at `0x80021D95` and the low three bits returned by the
typed `0x8002AB70` boundary. Team selection deliberately uses the actor's
unsigned byte at `+0xD9`, so both `1` and `255` choose the second team block.

Ownership publication preserves its unusual prefix. Actor/team pointers and
the actor id are stored at `0x800FDC34`, `0x800FDC38`, and `0x800FDBCC` before
the nested descriptor is consumed. The descriptor byte is written to actor
`+0xCA` in the load branch delay slot, even when its zero/nonzero decision is
unknown. Position, history, controller, clock, rule, state, and possession
globals then follow the exact source access order. Signed actor and phase
tests remain signed; team-byte comparisons and the stat cap at 999 remain
unsigned. Actor byte counters and controller halfword counters wrap, while
individual stat halfwords stop incrementing at 999.

The change-team path caches the old possession halfword in live `s3`, resets
the possession clocks and flags, publishes the actor team, and calls
`0x80072C40`. It chooses 10000/20000 for `0x800295C8` from signed actor id and
then sends codes 1/2 to `0x80029590`. The remaining rule gates select the
turnover, steal, and related stat blocks. Their deliberately odd `s2` offsets
resolve the original pointer arrays at `0x800FDC50` and `0x800FDC70`; no host
pointer arithmetic replaces those guest addresses.

The same-team path stores the old team from the `0x8005D3C8` branch delay into
`0x800FDB96` before testing the remaining gates. Its corresponding delay and
effect codes are 10000/20000 and 3/4. When actor and team X signs agree, it
updates the claim count, checks animation `[44,61]`, halves both signed
velocity halfwords with arithmetic shifts, and calls `0x8005CE4C`. The second
velocity store is the JAL delay slot. A nonnegative controller index then
increments the controller counter after that callback.

Cleanup loads signed `0x800FDB96` into `v0` before writing
`0x800FDBCA/DBD4/DBB0/DBDA/DBD8`. This matters when caller-provided mappings
alias the return halfword with a cleanup destination. The epilogue reloads
all five saved registers through the current `sp`, so child changes to the
stack pointer, saved frame, `s0..s3`, other GPRs, and HI/LO remain live.

The production adapter composes all four calls to the existing recovered
`0x800295C8` full-machine leaf. The RNG routine at `0x8002AB70` has only a
narrow owner, so AK keeps it as an explicit typed event instead of inventing
a full-machine ABI. The same typed boundary remains for unowned `0x80072C40`,
`0x80029590`, `0x80035318`, and `0x8005CE4C`. The natural caller at
`0x8006089C` is composed through the actual separately recovered contact owner.
The bridge validates AH's source PC, delay PC, typed event, live `a0=s1`, and
link register before starting AK on the same retained memory and full machine.
It restores the contact callback after the run and preserves a runtime AK
argument-failure prefix once AK has published operations or a stopped PC.

The focused executable covers both team blocks, team byte `0/1/255`, the
phase-82 random rule values `0/1/8/255`, grounded resets, ownership publication
before an unknown descriptor decision, same-team velocity division, animation
bounds, stat caps, byte wrapping, callback refusal, validation, and every
operation-budget cutoff. Directed cases retain each change-team statistic
branch (`+0x0C`, `+0x16`, `+0x14`, and the other selected actor's `+0x18`),
999 caps, actor-byte and controller-halfword wrap, callback-mutated live saved
registers, provenance-free unknown-store rejection, and malformed child-machine
rejection. The integration executable drives the real AK owner through the
actual `0x800295C8` leaf at all four source call sites, then drives actual AH's
`0x8006089C` event into AK with shared memory and full-machine propagation. The
routine mutates CPU and gameplay ownership state and has no direct visual effect
or asset output.

Validation passes: 293 focused checks, 55 actual-caller/leaf integration checks,
all 267 asset-free CTests, strict C99 compilation, and progress/metadata freshness.
Private original-instruction comparison passes 12,385 cases and all 556 source
instructions, checking full mapped memory, GPRs, HI/LO, callback entry state,
operation cutoffs, and relocated stack frames.

The native input-driven verifier composes the complete contact caller's
`0x8006089C` acquisition event. This phase81 fixture executes 66 operations:
24 reads and 42 stores with no child calls on that selected path. It publishes
actor `0x80002000` and team `0x8001EDF4`, changes owner `0xFFFF` to zero, and
lets the caller advance phase129 to130 with delay3. Acquisition frame SP is
`0x801FEF90`, returned SP `0x801FEFC0`, and restored RA `0x800608A4`.
Before/after CPU frame hashes both equal
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The frontend remains User Setup and this independent fixture is not gameplay.

Gameplay shown: NO - no direct visual effect.

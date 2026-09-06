# GAMEONLY period expiry gate recovery

`nba97_game_period_expiry` owns the complete GAMEONLY routine at
`0x80067664..0x800677D7` inclusive: 372 bytes and 93 instructions. The source
boundary comes from fresh Ghidra evidence in ignored local storage,
`game_80067664.txt`, with instruction SHA-256
`8acbe5ca26dbf2ab85193413b716f84fbb6d6c4c2dfadded84f43aada11f5db4`.
Its sole caller is the recovered match tick at `0x80068D6C`. The only direct
child is still-unowned `0x800582DC`, called at `0x800676CC`; it remains an
explicit full-machine dependency.

The owner reads the main clock before allocating its 0x20-byte frame, saves
`s1`, clears live `s1`, saves `ra`, and spills `s0` in the main-clock branch
delay. A nonzero clock returns the live zero `s1`. At clock zero, a nonnegative
owner enables actor processing. Unsigned actor types 14 and 15 skip the child
through the source `SLTIU` gate; type 19 skips it through the following `BEQ`
but still publishes live actor `s0` to `a0` in that branch delay. Every other
type calls `0x800582DC` with `a0=live s0` and `a1=1`. The child sees JAL `ra`,
the completed delay slot, all GPRs and HI/LO, and may mutate all of them.

The post-child halfword 30 store deliberately uses live, callback-mutable
`s0`. The routine then stores owner -1, phase zero, and the captured ball
pointer in the evidenced order. The ball pointer is loaded again afterward.
Timer processing requires signed `(ball_height >> 8) < 49` and signed ball
velocity at least zero. The optional `0x800FA038=1` store additionally requires
nonnegative `0x800FA034`, signed phase 0x82, zero `0x800FE882`, and a nonzero
unsigned byte at `0x80021D95`.

The period timer subtracts unsigned halfword `0x800FDB6C` from unsigned
halfword `0x800FDB76` with 32-bit wrap, stores the low half, then applies the
source `SLL 16` signed test. Positive low halves return live `s1`; zero and
negative low halves first force `s1=1`. Consequently a child-mutated `s1` is
observable and an ordinary return is not constrained to Boolean values. The
epilogue reloads `ra/s1/s0` in order through live `sp`, advances `sp` by 0x20,
and consumes the possibly unknown `ra` only after that mutation.

All guest addresses are mapped `uint32_t` values. Reads and writes preserve
little-endian widths, alignment traps, access order, completed failure
prefixes, native-storage aliases, address wrap, and per-byte knownness. The
source contains no HI/LO instruction, so HI/LO pass through unchanged unless
the typed child mutates them. The owner never casts a guest address to a host
pointer.

`nba97_game_period_expiry_from_match_tick` binds only the natural
`0x80068D6C -> 0x80067664` event. The legacy tick callback contains no live
GPR, stack, or HI/LO state, so the adapter requires an independently proven
entry machine and verifies the JAL-produced `ra=0x80068D74`. Previous tick
children stay explicit fixtures in the focused natural-caller test; no other
routine is translated here.

The runtime-generated focused tests cover nonzero and zero clocks, the
pre-frame access prefix, signed owner branches, every one of the 256 actor type
bytes, exact call and delay state, refusal and malformed child results,
child-mutated `s0/s1/sp`, relocated saved words, HI/LO, ball height and velocity
boundaries, every period-flag gate, timer low-half wrap, unknown branch delay
publication, `SLTIU` upper-byte knownness, mapped/aligned/wrapped/null pointers,
native-storage aliases, and every operation-budget prefix. The integration test
executes the existing match tick through its actual `0x80068D6C` service event,
proves the returned value ends the tick period, and checks nested adapter
validation and failure propagation.

Visual classification: no direct visual effect. This routine changes retained
CPU actor, phase, pointer, flag, and timer state and returns an end-of-period
decision. It does not render court/player pixels or establish an advancing
match. Manager-owned shared capture wiring can verify pixel-identical frames
after integration.

Manager integration now carries the violation owner's complete machine output
through adjacent 0x80068D6C JAL/NOP without resetting memory. The three native
root fixtures retain nonzero clocks, so the period-expiry owner returns zero
with exactly 7 operations (4 reads, 3 stores), restores RA=0x80068D74 and reaches
0x80068D7C -> 0x8002DE34. Initial machine and earlier services are explicitly
synthetic; this is not live match-stack continuation from the frontend.

Validation: 1,422 focused checks, 13 natural integration checks, strict C99,
and all 243 asset-free CTests passed. Private original-instruction comparison
passed 9,180 cases across all 93 PCs, including all 256 actor types, full memory,
all 32 GPRs/HI/LO, callback state, mutable S1=61, and operation-budget prefixes.
Native before/after diagnostic frames match SHA-256
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d;
receipts prove the return decision and stack stores. The separate User Setup
menu screenshot remains ignored. Gameplay shown: NO - no direct visual effect.

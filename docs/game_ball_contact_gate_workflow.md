# Game ball contact gate recovery

`nba97_game_ball_contact_gate` owns the complete GAMEONLY routine at
`0x80060E8C..0x80060EF7` (108 bytes, 27 instructions). The translation was
made from the fresh Ghidra listing in `game_80060e8c.txt`, whose instruction
bytes have SHA-256
`ae854630bdf6eb03fe7e71319dc8a55c0bf174f1ce958b2fd20cd0480cb884e9`.

The routine saves `ra`, subtracts the first object's coordinate at offset 8
from the second object's coordinate at offset 8 with 32-bit wrapping, then
arithmetic-shifts the result by eight. Values from -32 through 32 dispatch the
typed `0x800602CC` child; all other values return zero without dispatch. When
the second object's identifier at offset 0 is 10, `a0` and `a1` are swapped
before the call. `a2` was computed before that swap and retains its original
sign. Every completed child result is replaced with one.

The owner exposes all 32 GPRs, HI/LO, per-byte knownness, retained memory, an
ordered access journal, and an operation budget. A child sees the full live
machine after `jal` has written `ra=0x80060EDC` and may mutate registers,
HI/LO, memory, and `sp`. The epilogue reloads `ra` through that live `sp`.
Unknown branch predicates stop only after their delay slot: the coordinate
gate exposes known `v0=10`, and the identifier comparison exposes `a0` as the
original first object.

Focused tests cover both completed paths, shifted boundaries, signed
fractions, subtraction wrap, identifier normalization, callback refusal and
mutation, every operation-budget prefix, unknown decisions and return
addresses, access order, mapping and alignment failures, stack aliasing,
32-bit stack wrap, metadata rejection, full-machine preservation, and
repeatability.

The native adapter composes the typed child boundary with the complete frozen
`0x800602CC..0x80060E8B` contact owner and preserves its full entry and result
machine. That child in turn composes its recovered `0x800295C8` and
`0x800582DC` dependencies and leaves every other child explicit through its
binding. The composition test drives the identifier-10 swap from actor/ball to
ball/actor, executes the actual contact owner, observes its recovered rule
delay child, restores both nested frames, and proves that this wrapper replaces
the completed contact return with one. It also covers gate skip, nested child
refusal, and child-budget failure. The natural callers at `0x80061070` and `0x800610C4` are now composed by the
[complete pair dispatcher](game_contact_dispatch_workflow.md).

This routine has no direct visual effect. A return value of one proves only
that the child dispatch completed; visible contact behavior belongs to the
child and its downstream state changes.

Manager verification: 140 focused assertions and 13 actual-contact composition
checks pass. The private original-instruction comparison passes 1,920 cases
covering all 27 PCs, full 2 MiB memory, 32 GPRs plus HI/LO, callback machines,
operation cutoffs, argument/stack aliases and child stack relocation.
The native input verifier now enters the phase81 contact probe through this
gate. It records six operations (four reads, one store, one child), the ID10
swap, both nested restored frames and return1. Typed geometry/acquisition/release
remain explicit. Matching before/after CPU frame hashes and the state receipt
`ball_contact_gate_verified.json` prove CPU composition without advancing play.
All 261 asset-free CTests pass.

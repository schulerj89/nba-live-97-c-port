# GAMEONLY match clocks recovery

`nba97_game_match_clocks` owns GAMEONLY `0x80067A60..0x80067D37`
(728 bytes, 182 instructions). The boundary comes from the fresh read-only
Ghidra listing `game_80067a60.txt`; its instruction-byte SHA-256 is
`c98700c14432e8a6f74f4b1abb90b9ad82cd02cff4618117a1748a29341a354f`.

The C99 owner retains every GPR with per-byte knownness and retains HI and LO
as independently mutable machine registers. Entry creates the 0x30-byte frame,
saves old `s2` before capturing signed delta from `a0`, and reads the main clock
and signed phase before saving `ra/s1/s0`. Guest addresses remain 32-bit values
and all little-endian reads and writes use validated retained regions.

The main clock runs for signed phases below `0x80`. Phase `0x81` is paused.
Phase `0x82` additionally requires zero at `0x800FE882`, a nonzero unsigned byte
at `0x80021D90`, `clock > signed word 0x800FDB5C`, and a clock unequal to
`0x800FDB60`. The gate always executes the source `lui a2,0x8888` delay slot,
including exits.

Eligible clocks reload live memory and reproduce each signed MIPS operation in
the division sequence: `MULT` by `0x88888889`, HI/LO assignment, `MFHI`, signed
shift, wrapping add, and wrapping subtract. Four trace records expose the exact
operands and intermediate machine values. The decrement uses signed
`min(clock,s2)` and stores in the branch delay slot. Main-clock services are
`0x80029258(10)` at zero, `0x80029258(11)` when crossing into the final four
seconds, and `0x8007F9C4(2 or 1)` at the 120- and 60-second crossings.

The nonzero shot clock runs only below signed phase `0x80` and repeats the same
HI/LO sequence. Its final-four-second comparison sign-extends only the low
16 bits of the new seconds value, preserving the source wrap quirk. The sound
flag at `0x80021D92` is unsigned; a zero shot clock requests sound 10, while a
crossed final-four boundary requests sound 11 only when the live main clock is
at least 301.

After clock handling, the owner reads the home timer, clears `0x800FDB86`, and
then updates home and away timers in order. A timer already at zero or negative
is clamped to zero and its state becomes 2. A positive timer stores the wrapped
low half after subtracting live `s2`; underflow is intentionally left negative
until the next invocation. The epilogue reloads `ra/s2/s1/s0` through the live,
child-mutable stack pointer, advances `sp` by 0x30, and refuses an unknown JR
target after preserving the completed prefix.

Both unresolved services receive their exact call PC, delay PC, entry,
invocation, argument count, operation order, and full mutable GPR/HI/LO state.
No audio effect is fabricated. `nba97_game_match_clocks_from_match_tick` binds
only the natural `0x80068D40` and `0x80068D58` events. Because the older tick
interface carries scalar arguments but no GPR, stack, HI, or LO state, the
adapter requires an independently established entry machine and checks its
`a0` and JAL-produced `ra` against the actual event.

The focused runtime-generated tests cover every phase gate, phase-82 condition,
signed clock and delta extremes, division boundaries and exact products,
zero/final-four/120/60 sound paths, shot-clock flags and signed-low-half wrap,
team zero/negative/underflow behavior, live callback mutation, stack/global
aliasing, wrapping stack addresses, per-byte unknownness, malformed/refused
children, mapping and alignment traps, and every operation-budget prefix. The
integration test executes the existing match-tick owner through both actual
call sites and proves nested failure propagation without inferring missing ABI
state.

Visual classification: no direct visual effect. This routine changes retained
CPU clock and timer state and invokes typed effect services; it neither renders
pixels nor proves audible output. Manager-owned shared capture registration can
verify matching frame hashes when the routine is integrated.

Manager verification compares 5,112 cases with original instructions, covering
all 182 PCs, complete memory, all 32 GPRs and HI/LO, child-entry machine state,
and bounded prefixes. Review corrected publication of the three conditional
delay-slot results before a possible unknown-branch stop.

The native input driver composes the actual tick adapter with independently
supplied machine/clock fixtures for phases 0, 0x81, and 0x82. These demonstrate
normal thresholds, the paused tip-off phase, and the special phase gate, then
stop at 0x80068D64 -> 0x80067D38. They do not establish the tick's live prologue
or an advancing simulation. Before/after diagnostic scanouts remain identical;
separate frontend frames show User Setup. Lower effect services remain explicit
fixtures, with no audible playback or gameplay claim.

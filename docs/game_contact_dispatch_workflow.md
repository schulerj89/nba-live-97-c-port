# GAMEONLY sorted contact dispatcher recovery

`nba97_game_contact_dispatch` owns the complete GAMEONLY routine at
`0x80060FBC..0x800610FB` inclusive: 320 bytes and 80 instructions. Fresh
Ghidra evidence is retained under ignored local storage in
`game_80060fbc.txt`; its instruction SHA-256 is
`c689adf57bd7c0054146f3a97c0f8c9e3ebafd77fafe55c2fa3ee77956b0d254`.
The recovered match tick is the sole caller, at `0x80068E08`. Ownership search
found no prior complete owner.

The routine allocates a 0x20-byte frame, saves `s2`, clears live `s2`, then
saves `ra/s1/s0`. Its outer loop increments `s2` as a wrapping 32-bit word but
sign-extends only the low half for the `<12` decision and table index. The
first normal entry is therefore index 1. Negative low-half indices still pass
the signed comparison and address words before `0x800FDCBC`; the native owner
preserves them whenever those guest addresses are mapped.

Each outer entry reloads the live ball pointer at `0x800FDC48` and publishes
`s1=s2` in the equality branch delay. A non-ball outer object scans later
references using the same full-word increment and signed-low-half index. A
later non-ball object calls `0x8005FAA8` at `0x8006104C` with `a0=s0` assigned
in the delay slot and the table reference already live in `a1`. A later ball
with nonzero signed halfword `ball+0xB4` is skipped; a zero value calls
`0x80060E8C` at `0x80061070` with `a0=ball` and `a1=s0` assigned in the delay
slot.

When the outer entry is the ball, nonzero `ball+0xB4` skips its entire row. A
zero value calls `0x80060E8C` at `0x800610C4` for each later reference, with
`a1` loaded from the table and `a0=s0` assigned in the delay slot. After every
child return, only the low byte of live `v0` controls continuation. Thus
`0x100` and `-256` exit while `0xFF` and `-1` continue. The `s1` or `s2`
increment in each branch or jump delay uses the callback-mutated live register.
Callbacks may also change selected `s0`, the live ball pointer, table contents,
`sp`, saved words, any other GPR, and HI/LO before subsequent source reads.

The epilogue reloads `ra/s2/s1/s0` through callback-mutable live `sp`, advances
that `sp` by 0x20, and consumes the restored `ra` after the source JR/NOP. Guest
addresses stay explicit `uint32_t` values. Validated retained regions preserve
little-endian widths, alignment faults, wrap, native-storage aliases, per-byte
knownness, source access order, and every completed failure prefix. The owner
contains readable source-level loop blocks rather than an instruction-PC
interpreter.

`nba97_game_contact_dispatch_compose_children` executes the complete recovered
`0x80060E8C` ball-contact gate for both original AI call sites and copies its
full resulting machine back on success or failure. With `contact_binding` configured, that owner composes the complete
`0x800602CC` contact owner and its recovered reset/delay children. An explicit
full-machine child callback is also available for isolated boundary fixtures. The
`0x8005FAA8` child is still unowned and also remains a typed callback. No child
algorithm is duplicated in this recovery. The mux requires a fully known
`ra=call_pc+8` and well-formed incoming machine at both child targets. It also
validates retained-memory metadata before entering the ball-contact owner, so
an argument rejection before that owner publishes progress leaves the incoming
machine unchanged.

`nba97_game_contact_dispatch_from_match_tick` binds only the natural
`0x80068E08 -> 0x80060FBC` event. The older tick service API carries no GPR,
stack, or HI/LO state, so the adapter requires an independently proven entry
machine and verifies the JAL-produced `ra=0x80068E10`; it does not infer ABI
preservation from the narrow call.

Runtime-generated focused tests cover all 55 non-ball pairs, every one of the
11 possible ball positions, absent-ball scans, both `ball+0xB4` states, all
three call PCs and delay slots, low-byte returns 0/1/255/256 and raw negative
values, early exits, duplicate references, live `s0/s1/s2/sp` and saved-stack
mutation, HI/LO and other GPR mutation, live table/ball changes, signed-low-half
negative indices and wrap, unknown comparisons and delay publication,
unaligned/unmapped/null pointers, malformed mappings and callbacks, refusals,
and every operation-budget prefix. The integration test reaches the actual
`0x80068E08` call through the recovered tick, supplies an explicit full entry
machine, executes the real `0x80060E8C` owner through the production child mux,
and keeps `0x800602CC` as a typed fixture while checking nested failure
propagation.

Visual classification: no direct visual effect. This CPU routine selects and
dispatches collision/contact pairs; it does not render court or player pixels.
An advancing match loop with its remaining simulation and rendering providers
is still required before gameplay can be claimed. Manager-owned shared capture
registration can record the pixel-identical diagnostic frame after integration.

Manager review added an explicit wrapped guest-stack test and natural-tick
composition through both complete coordinate-gate and contact owners. Final
focused checks: 2,142; integration checks: 40. The independent original
instruction comparison passes 39,936 cases and all 80 PCs, comparing full
2 MiB memory, 32 GPRs plus HI/LO, child entry machines, budget cutoffs,
negative/wrapped pair indices, mutable ball references and relocated frames.

The native input verifier records an independent dispatcher fixture with two
actual coordinate-gate calls, one completed contact owner, nine typed actor-pair
calls, and phase129 to130 with delay3. Dispatcher work is 62 operations,
47 reads, four stores and eleven children. It returns through the three nested
frames without creating a live tick-entry machine. The receipt
`contact_dispatch_verified.json` and pixel-identical CPU frames remain ignored
local evidence. All 263 asset-free CTests and freshness checks pass.

# Ball/actor contact recovery

`nba97_game_ball_actor_contact` owns GAMEONLY `0x800602CC..0x80060E8B`,
3008 bytes and 752 instructions. The source is the fresh Ghidra listing
`game_800602cc.txt`, instruction SHA-256
`df9074d4240d6e16e099c0d3c5d2a45941355872521ad92d25123f29b30b7ac7`.
Its sole known caller is the now-owned wrapper at `0x80060E8C`, call PC
`0x80060ED4`. The [coordinate gate](game_ball_contact_gate_workflow.md) composes
this complete owner in integration tests and the native CPU probe; this does
not yet establish a live tip-off.

The owner retains all 32 GPRs, HI/LO, per-byte knownness, uint32 guest address
wrapping, mapped stack and retained memory, exact branch/JAL delay slots, and
operation prefixes. It covers eligibility exits, hand/body contact selection,
accepted acquisition and phase 81 to 82 transition, and negative deflection
statistics and velocity. Child callbacks observe JAL `ra` and the completed
delay slot and may mutate the whole machine and memory.

The adapter composes the complete `0x800582DC` actor-resume owner at all three
sites and the complete no-op `0x800295C8` rule-delay owner at all four sites.
The remaining targets are explicit full-machine callback dependencies because
their older narrow owners cannot prove all returned registers and stack state:
`7066C`, `601B8`, `60240`, `60008`, `2AB70`, `581C0`, `58120`, `29258`,
`29590`, `7059C`, `5D140`, `58260`, `5BC34`, `6E7AC`, `6229C`, `62660`,
`35318`, `5699C`, `A5638`, `AA788`, `A5634`, and `5828C`.

Focused fixtures are generated in memory and require no retail assets. The 667
assertions cover eligibility exits and signed thresholds, normal/81/82/special
modes, forced-mode low-half behavior, accepted and negative contact, the phase
81-to-82 transition, aliased timers, capped and wrapping statistics, velocity
clamps, callback mutation/refusal, both full-path operation-budget prefix sets,
all 16 callback-result knownness masks, subtraction borrow knownness, alignment,
mapping, and low-address wrapped stack frames.
The journal assertions pin the opening stack/global access PCs, addresses, and
one-based operation order plus the four aliased phase-81 timer stores before the
broader full-memory oracle comparison.
The 34 integration assertions reach all four source AE sites and all three
source AF sites through the production adapter, including AF refusal. Each composed event
validates the parent PC, delay PC, kind, argument count, and JAL return address.

A private original-instruction differential compared 16,996 cases across all
752 source PCs, full 2 MiB retained memory, all 32 GPRs plus HI/LO, callback
machines, mutable flags and stack frames, and operation budgets 0 through 159.
The complete source owner matched that oracle. The routine changes CPU contact
and possession state; it has no direct visual effect until a match/render bridge
consumes that retained state.

The old `game_tipoff_phase` `608A4` fragment and declaration were removed to
avoid overlapping ownership. Its unknown-target and unknown-jumper ordered
prefix checks now enter the complete owner. Existing hand/body/release unit
checks remain, including the unsigned-side RNG receiver indexing quirk.

The native input verifier runs this production owner and adapter in an
independent phase81 CPU fixture with aliased jumper references. It records
phase129 to130, phase delay3, 69 owner operations (38 reads, 22 stores, 9 calls),
and two completed actor-reset owners. Geometry, acquisition and release remain
explicit typed responses. The 98-state frontend driver and pixel-identical
before/after CPU frames do not demonstrate an advancing match or retained
possession. The state receipt is `ball_actor_contact_verified.json` under the
ignored capture directory. All 259 asset-free CTests pass.

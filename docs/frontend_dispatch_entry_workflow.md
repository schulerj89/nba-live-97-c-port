# FEONLY initialized frontend-dispatch entry recovery

`nba97_frontend_dispatch_entry` owns exactly FEONLY
`0x800360D4..0x8003610B` inclusive: 56 bytes and 14 MIPS instructions. A
repository-wide search found inventory and workflow references plus a narrow
adapter named for the call site, but no complete native owner of this wrapper.
The existing `MatchSession` projection represents only host lifetime policy.

The source boundary comes from the fresh read-only Ghidra listing
`.local/evidence/tipoff-recovery/feonly_800360d4_resume.txt`. Its displayed
instruction bytes and an independent `FEONLY.BIN` range have the same SHA-256,
`6af71d91fded3e2b5260c84bb86fd101539e86fca86ffef2b9e06b93e32dbce0`.
The only evidenced caller is the still-unowned FEONLY frontend main at JAL
`0x80028AA0`; its delay instruction at `0x80028AA4` clears `s0`, and its return
address is `0x80028AA8`.

The owner subtracts 24 from the live `sp`, writes initialization flag `1` to
`0x80021EE4`, writes the incoming `ra` to `sp+16`, and writes scalar `32` to
`0x800C6E68`, in that order. The first global write therefore survives a later
stack mapping or alignment failure. A stack alias can overwrite the first
global, while an alias with `0x800C6E68` is overwritten by the later scalar
store; both source quirks are retained.

JAL `0x800360F4` sets `ra=0x800360FC` before the NOP delay at `0x800360F8`.
The synchronous child callback receives the complete mutable machine after the
delay slot and identifies target `0x8003F7C8`, call PC, delay PC, operation,
invocation, and zero formal arguments. On return, the owner loads `ra` from
callback-live `sp+16`, adds 24 to callback-live `sp`, and executes the JR and
its NOP delay. A partially known restored `ra` reports `NBA97_TEXT_UNKNOWN`
only after that full epilogue prefix. All other child-returned GPRs and HI/LO
remain live.

Guest addresses stay 32-bit values over validated, nonoverlapping retained
regions. Every word access is little-endian, aligned, and carries a four-bit
per-byte knownness mask. Stores to a region with no knownness plane require a
fully known value. A malformed knownness byte is detected before any byte of
that access changes, and a callback refusal or malformed returned machine
publishes the exact callback-visible state. The operation budget counts the
three store attempts, one callback attempt, and final load attempt.

`nba97_frontend_dispatch_entry_with_recovered_dispatch` converts the sole
child event to the established `Nba97FrontendDispatchCallerEvent` contract and
calls `nba97_frontend_dispatch_from_800360d4`. This composes the complete
recovered frontend dispatcher without copying its algorithm. Nested failures
remain typed wrapper refusals while adapter progress records the dispatcher's
exact result and fresh prefix. Reusing a binding cannot expose progress from an
earlier invocation when the nested adapter rejects before execution.

`nba97_frontend_dispatch_entry_from_frontend_main` is a narrow boundary adapter
for integration and capture. It accepts only the exact `0x80028AA0` call,
`0x80028AA4` delay, `0x800360D4` target, `ra=0x80028AA8`, and fully known zero
`s0`. The parent itself remains unowned. Its fixture supplies an explicit full
machine and is not evidence that the preceding frontend-main instructions have
been recovered.

The asset-free focused test covers all 14 instructions and all five operation
budgets; exact access and call order; all 34 machine words; every saved-`ra`
knownness mask with and without a knownness plane; zero, wrapping maximum,
split, missing, unaligned, and overlapping mappings; malformed final bytes at
each store/load stage; callback refusal and invalid zero/GPR/HI/LO results;
callback-live stack relocation; partial JR targets; stack/global aliases;
`SIZE_MAX` region rejection; invalid-context nonmutation; and deterministic
repeatability. The integration test runs the wrapper into the actual recovered
dispatcher and native `UserSetupSession` acceptance fixture. It additionally
relocates the complete combined `0xA0`-byte wrapper/dispatcher frame, both with
and without a knownness plane, and verifies nested limits and reusable bindings.

`captureFrontendDispatchEntry()` returns one JSON object. It identifies the
program, inclusive source boundary, hash, classification, and exact missing
boundary; records the synthetic parent call, before/after globals, wrapper
operation and access journal, typed child call, full dispatcher progress and
exact 42-call sequence, native User Setup acceptance, and all final GPR/HI/LO
words with knownness masks. The receipt explicitly labels the synthetic
full-machine/callee fixture and the recovered wrapper-to-dispatcher composition.
The manager-owned native verifier stores this JSON and frame captures only in
ignored local output.

The visual classification is `UI/menu`. Gameplay shown: BLOCKED. Recovery
stops at wrapper return `0x80028AA8`; frontend-main services through the
`gameload.bin` load at `0x80028ACC`, copy to `0x801E0000` at `0x80028B54`, and
dynamic GAMELOAD transfer at `0x80028B68` remain unbound. The synthetic User
Setup continuation is not advancing gameplay.

Manager verification compared the complete owner with all 14 fresh source
instructions and passed 7,168 private raw-instruction differential cases over
all 14 PCs, all 34 machine words, full 2 MiB mapped RAM, operation budgets 0..6,
and callback mutation, refusal, stack relocation and saved-return changes.
Final MSVC Debug sources pass 286 focused checks and 315 integration checks,
including a regression for numeric serialization of the byte-sized User Setup
result. All 389 asset-free Debug CTest tests pass (33.83 seconds). Progress,
C recovery, instruction-semantics freshness and roster configuration checks
pass. No live no$psx process was available, so this recovery adds no observed
original-runtime register trace.

The final native input-driven run is
`.local/verification/team_select/game-entry-20260906-114319-e14b7207/`.
All 102 captured states and the wrapper receipt pass. The before/after PPM
pixel SHA-256 is identical:
`42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7`.
The manager inspected `frames/frontend-dispatch-entry-after.png`; it remains
User Setup. The exact ordered global/stack changes and returned parent RA are
recorded in `frames/frontend_dispatch_entry_verified.json`. Captures are ignored
local evidence and are not committed. The source return path is recovered;
production frontend-main and GAMELOAD services remain separate dependencies.

# FEONLY frontend I/O drain recovery

`nba97_frontend_io_drain` recovers FEONLY `0x800393F0..0x800394D3` from the complete 228-byte, 57-instruction source range whose SHA-256 is `ddd6a228f2ddfecfebe23641b1c36c549e82172f38dfe659484b2d9e521ea50c`. Fresh Ghidra evidence records callers at `0x800394E8`, `0x8002FB14`, `0x8003E538`, `0x80030D60`, and `0x80031270`. A repository-wide address search found no prior complete full-machine owner; older frontend resource helpers remain narrower semantic code and are not duplicated.

The owner allocates a 32-byte frame, saves `s1` at `sp+20`, clears it, saves `s0` at `sp+16`, clears it, then saves `ra` at `sp+24`. The slot loop loads a signed status word at `0x800EF840+s0`. Status 3 loads the pointer at `0x800EF844+s0`, calls `0x80077638` with that word in `a0`, recomputes both later addresses from callback-live `s0`, and clears the pointer and status. Status 1 clears the status. Signed statuses 4 and 5 clear `0x800EF830+s0`. Negative values, 0, 2, and values at least 6 leave slot memory unchanged.

The control flow preserves the source’s branch latches. Branch `0x80039418` compares `v1` with old `v0=3` before delay `0x8003941C` overwrites `v0` with signed `v1<4`. Branch `0x80039420` tests that SLTI result before delay `0x80039424` overwrites `v0` with 1. Each arm increments callback-live `s1` at its exact source instruction. The signed `s1<8` loop test runs before branch delay `0x80039498`, which adds 36 to callback-live `s0` on both the taken and exit paths. A handle callback can therefore move later clears to another slot, change the number of iterations, run outside the nominal eight slots, or produce an effectively unbounded loop. The native operation budget reports the exact completed prefix instead of repairing either counter.

After the slot loop, `0x800392A0` is called until it returns a nonzero `v0`; each zero result calls `0x80038E84` and jumps back through a NOP delay. The epilogue loads `ra`, `s1`, and `s0` through callback-live `sp+24`, `sp+20`, and `sp+16` in that order, adds 32 with 32-bit wrap, then validates the restored return target. All three children are typed full-machine callbacks. Fresh child evidence confirms the status-3 call supplies one formal argument in `a0` to `0x80077638`.

Guest words use validated little-endian retained regions with an optional byte-knownness plane. Signed comparisons retain full output knownness when all represented values agree and otherwise retain known-zero upper result bytes. Address arithmetic, frame and slot aliasing, malformed knownness, absent planes, alignment, missing mappings, overlapping or wrapping regions, callback refusal, malformed callback state, saved-register masks, and late return faults remain explicit outcomes. Every access and callback spends one operation and journals its exact source PC, value, knownness, and order.

`nba97_frontend_io_drain_from_frontend_exit_drain` is the natural-caller adapter. It accepts only FEONLY call `0x800394E8`, delay `0x800394EC`, entry `0x800393F0`, invocation one, zero formal arguments, and fully known `ra=0x800394F0`. The integration fixture executes the committed `nba97_frontend_exit_drain` C owner and routes its first child through this adapter. Later exit-drain services and all three I/O-drain children remain explicit synthetic fixtures. A relocation case moves the combined 24-byte outer and 32-byte inner frames and proves both owners restore their own saved words through callback-live `sp`, with and without a knownness plane.

The standalone capture generates eight statuses `3,1,4,5,-1,0,2,6` in retained memory. The handle fixture receives the exact first-slot pointer and preserves all 32 GPR words and masks plus HI/LO and memory. The poll fixture changes only `v0`, returning fully known zero and then one. The pump fixture preserves the full machine and memory. These values document synthetic test contracts; they do not implement the children or establish their complete retail ABI. The capture records all 57 unique source PCs across 164 executed instructions, four calls across all three sites, 20 ordered guest accesses, full callback-entry machines, and the final machine.

The focused MSVC `/W4 /WX /Od /sdl` executable passes 386 checks. It covers every signed status class and boundary, all 57 PCs and three sites, old-`v0` branch latching, partial signed decisions, callback-live `s0/s1`, moved clear addresses, slot/frame aliasing, all budget prefixes for simple and all-branch paths, bounded counter and poll runaways, callback refusals, full mutable GPR/HI/LO state, saved-register masks, callback-live frame relocation, malformed and absent knownness planes, alignment/mapping failures, region overlap/wrap guards, and late return faults. The strict natural-caller and capture executable passes 14,125 checks. Manager-owned independent raw-instruction comparison passes 8,424 cases across all 57 PCs, three sites, full 2 MiB RAM, all 34 machine words and masks, callback/access/instruction prefixes, live `s0/s1/sp`, budgets, and refusals; private evidence remains under `.local/evidence/`.

Visual classification is `no direct visual effect`. Gameplay shown is `BLOCKED`: this standalone CPU receipt does not bind the production overlay entry, startup, loader, or advancing native match handoff. No match deferral bypass or fabricated production service is introduced.

Final manager validation (2026-09-06): final MSVC Debug rebuild passed;
386 focused and 14,136 natural-caller integration/capture checks passed directly.
All 401 asset-free Debug CTest tests passed in 34.18 seconds. Progress,
recovery metadata, instruction semantics and roster-contract checks passed.
The private raw differential passed 8,424 cases covering all 57 source PCs
and three call sites. Final C hash matches the compared source:
205c1afa8f3a68ac6cc416db5a8f714a4e7bc6d7e78417b311e1168165ae9eac.

The native input driver captured 114 frames at ignored
`.local/verification/team_select/game-entry-20260906-134911-a57cd00e`.
The final verifier passed after correcting its frame lookup to use frame IDs;
`frames/frontend_io_drain_verified.json` proves all 164 executed PCs, 20 accesses,
four full-machine child snapshots and the exact epilogue. Both CPU-only PPM
pixel hashes equal 42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7.
The manager inspected `frames/frontend-io-drain-after.png`.
Gameplay shown: BLOCKED by the documented production lifecycle and child
bindings; this standalone CPU capture still leaves User Setup visible.

# GAMEONLY VBlank-service initialization recovery

`src/recovered/game_vblank_initialize.c` owns the complete 59-instruction
GAMEONLY routine `0x800A43E8..0x800A44D3`, reached from startup call PC
`0x80029A38`. The adjacent installed ISR at `0x800A450C` increments the frame
counter and visits the same eight callback words, which identifies this owner
as the VBlank-service initializer.

The routine saves `ra`/`fp`, calls `0x800A4830` to retain `gp`, and clears the
eight-word callback table at `0x800D6E0C`. It then executes `DrawSync(0)`, enters
a critical section, installs handler `0x800A450C` on interrupt channel zero,
calls `SetRCnt(0xF2000003, 1, 0x1000)` and `StartRCnt(0xF2000003)`, exits the
critical section, and invokes `0x800A3E48` to zero frame counters `0x800D7A88`,
`0x800D7AFC`, and `0x800D7B00`. All eight callees remain explicit synchronous
service boundaries; this CPU owner does not silently install a Windows
interrupt or manufacture VBlank cadence.

Compatibility deliberately retains a useful retail oddity. PsyQ `SetRCnt`
masks the specifier to low-half index `3`, rejects it because only indices below
three enter its configuration path, and returns false. `StartRCnt` nevertheless
indexes the four-entry mask table, ORs VBlank bit zero into `I_MASK`, and then
also returns false. The caller ignores both results and continues. It likewise
does not roll back the already-cleared callback table if a later service cannot
complete. Saved stack words are reloaded live after all children return.

Standalone tests cover the exact 54 owner operations, all 59 source PCs through
the private R3000 oracle, all eight call boundaries and arguments, counter-3
failure behavior, prefix-committing refusals, live epilogue mutation, unknown
bytes, malformed metadata, missing memory, alignment, and every operation-budget
stop. Private source evidence is kept under
`.local/verification/native_completion/game_call_800a43e8`; the routine bytes
have SHA-256 `71826dc26fa486efe0c88f3b3397c7322c444966b3910997bab5d6f16a83a4e7`.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers, captures all 98 PPM frames and
logs, and adds a VBlank receipt to `game_entry_trace.json`. The new routine only
changes mapped PS1 callback/counter state in that diagnostic, so it is expected
to have no direct pixel effect; Debug and RelWithDebInfo captures must remain
byte-identical.

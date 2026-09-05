# GAMEONLY source game-clock initialization recovery

`src/recovered/game_clock_initialize.c` owns the complete 105-instruction
GAMEONLY routine `0x800914D8..0x8009167B`, reached with rate 120 from startup
call PC `0x80029A4C`. Its installed interrupt-6 handler at `0x800916B4`
increments the free-running game-clock words and visits the eight callbacks at
`0x800D6DEC`, identifying this routine as the source game-clock initializer.

The cold path enters a critical section, sees guard `0x800C4AA4` clear, zeros
the interrupt divider and eight callback words, installs handler `0x800916B4`,
sets the guard, and registers `0x8009167C` in the 32-slot shutdown list. Every
call then performs the source's two signed divisions:

```
timer_target  = 4233600 / requested_rate;
effective_rate = 4233600 / timer_target;
```

For the retail argument, target 35280 and effective rate 120 are published at
`0x800D7A98` and `0x800D7A94`. The owner calls
`SetRCnt(0xF2000002, 35280, 0x1000)` and `StartRCnt(0xF2000002)`, exits the
critical section, then calls `0x800A5880` to reset `0x800D7A7C`, `0x800D7A70`,
`0x800D7B2C`, and `0x800D7B28`. The exact PsyQ Timer 2 fixture produces mode
`0x0258`, unmasks interrupt bit `0x0040`, and returns true from both calls.

Compatibility keeps the original arithmetic and failure behavior. Division is
signed and truncates toward zero; the second division publishes a quantized
rate rather than blindly echoing the argument. A zero argument reaches source
`BREAK 7` at `0x800915B8` after the critical section and cold registration have
already happened. A divisor whose first quotient is zero stores that zero
target and then reaches `BREAK 7` at `0x80091600`. Neither trap path rolls back
or calls `ExitCriticalSection`. The two compiler-emitted `INT_MIN / -1`
`BREAK 6` bodies are unreachable because the numerator is the fixed positive
4233600, but their conditions remain represented. Cold callbacks can also
rewrite the stack-spilled rate before its live reload. Warm calls skip table and
handler registration but still reprogram Timer 2 and reset the clocks. All
pre-final raw child returns are ignored.

The native owner maps original addresses onto caller-owned storage and exposes
platform work as synchronous callbacks. It does not retain raw pointers, call
an emulator, install a Windows interrupt, or create host timing. GAMEONLY main
composes it at its real address boundary, while unit and visual fixtures supply
the explicit critical-section, callback-list, Timer 2, and reset effects.

Public tests cover cold/warm startup, all 62 retail owner operations, exact
calls and arguments, signed and quantized rates, both reachable divide traps,
prefix commits, live argument/epilogue mutation, refusals, unknown data,
alignment, missing mappings, malformed metadata, and every operation-budget
stop. The private bounded R3000 comparison covers 103 of 105 source PCs; only
the two provably unreachable overflow `BREAK 6` instructions are excluded. It
also executes the original Timer 2 service and records the source-byte SHA-256
`47ccb827b9bebd0cf098b6c488472d939a3d3f8d3ca5e45a7264ec76810ccb82`.
Evidence stays under
`.local/verification/native_completion/game_clock_initialize`.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup using the test's native recovered-input handlers, captures 134 PPM frames
and logs, and writes the clock receipt into `game_entry_trace.json`. This
initializer changes source callback/timer state but cannot directly change a
pixel; Debug and RelWithDebInfo frames are therefore expected to remain
byte-identical. The registered shutdown wrapper `0x8009167C` is now recovered
separately and removes handler `0x800916B4` during main teardown; see
[game-clock shutdown](game_clock_shutdown_workflow.md). Ticking the handler
naturally remains a later timer/interrupt integration boundary. The immediately
following `0x800A584C` delta sampler is recovered separately and observes the
initializer's zero baseline without inventing that cadence. The capture still
does not claim a playable court or gameplay frame.

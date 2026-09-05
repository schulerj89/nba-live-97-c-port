# GAMEONLY game-clock shutdown recovery

`src/recovered/game_clock_shutdown.c` owns all 14 instructions of GAMEONLY
`0x8009167C..0x800916B3`. A fresh read-only Ghidra listing and bounded source
oracle show one complete wrapper:

```text
save ra and s8;
InterruptCallback(6, NULL);
reload ra and s8;
return;
```

The original bytes have SHA-256
`0724e7dd8a73dd92dde6a9128d2435f60888f950b29d1bf83f6d8e29f259c5dd`.
Main directly invokes it at `0x80029B6C`, immediately after VBlank shutdown.
The recovered clock initializer also registers this address in the shutdown
callback list. That initializer installs adjacent handler `0x800916B4` on
interrupt channel six, establishing this wrapper's purpose: remove the Timer 2
game-clock interrupt before main continues through controller shutdown and
FELOAD transfer.

The `0x8009860C` child matches PsyQ `InterruptCallback` and dispatches through
the retained PS1 callback table. This recovery keeps it as an explicit service
boundary. The composed diagnostic requires channel six and a NULL replacement,
verifies handler `0x800916B4` was installed in channel-six slot `0x800C54E8`
by the earlier recovered clock initializer, returns that old handler, and marks
the retained clock interrupt removed. It does not unregister a Windows
callback, stop a host timer, or claim native Timer 2 cadence.

Compatibility retains details hidden by the decompiler's `void` signature:

- the child callback's raw, possibly unknown `v0` remains live at return;
- the wrapper does not enter a critical section;
- it hardcodes interrupt channel six and NULL without checking the previous
  handler returned by the service;
- `ra` and `s8` are reloaded from mapped stack after the child, so child-side
  rewrites affect the epilogue; and
- a failure after either prologue store retains that partial source effect.

Native mapping, knownness validation, operation budgets, and callback refusal
are host safety boundaries rather than invented source branches.

`tests/game_clock_shutdown_tests.cpp` covers all five observable operations,
the exact child-call event, raw and unknown child results, live saved-register
rewrites, both unknown epilogue reads, every operation-budget prefix, callback
refusal, malformed metadata, missing memory, alignment, wrapping regions, and
argument validation. `tests/game_main_tests.cpp` composes the wrapper at its
natural caller and proves the clock handler's installed-to-removed lifecycle.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers—never computer-control clicks—
then reaches this wrapper through recovered main. It captures
`clock-shutdown-before.ppm`, `clock-shutdown-after.ppm`, the exact service
event, receipt, and trace. The frames must be pixel-identical because callback
unregistration performs no rendering. They establish native reachability and
absence of a direct visual effect, not a retail gameplay frame.

Main's immediately following controller-suspend call at `0x80029B74` is now
recovered separately; see
[controller suspend](game_controller_suspend_workflow.md).

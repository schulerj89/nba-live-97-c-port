# GAMEONLY VBlank shutdown recovery

`src/recovered/game_vblank_shutdown.c` owns all 14 instructions of GAMEONLY
`0x800A44D4..0x800A450B`. A fresh read-only Ghidra listing and decompile show
one complete wrapper:

```text
save ra and s8;
InterruptCallback(0, NULL);
reload ra and s8;
return;
```

The original bytes have SHA-256
`d30124f93b39486830bd850d0f764977363aebcc9919f7546bf0c1917be5a54c`.
Main is its only caller, at `0x80029B64`, after the second twenty-presentation
wait. The adjacent `0x800A450C` ISR and earlier recovered initializer identify
the purpose: this wrapper removes that VBlank handler before main continues
through clock and controller shutdown toward FELOAD transfer.

The `0x8009860C` child matches the 48-byte PsyQ `InterruptCallback` signature.
It dispatches through the retained PS1 callback table; this recovery keeps it
as an explicit service boundary. The composed diagnostic requires channel zero
and a NULL replacement, verifies that handler `0x800A450C` was installed in
channel-zero slot `0x800C54D0` by the
earlier recovered VBlank initializer, returns that old handler, and marks the
retained interrupt state removed. It does not unregister a Windows callback,
change host timing, or claim a native interrupt backend.

Compatibility retains details hidden by the decompiler's `void` signature:

- the child callback's raw, possibly unknown `v0` remains live at return;
- the wrapper does not enter a critical section;
- it hardcodes interrupt channel zero and NULL without checking the previous
  handler returned by the service;
- `ra` and `s8` are reloaded from mapped stack after the child, so child-side
  rewrites affect the epilogue; and
- a failure after either prologue store retains that partial source effect.

Native mapping, knownness validation, operation budgets, and callback refusal
are host safety boundaries rather than invented source branches.

`tests/game_vblank_shutdown_tests.cpp` covers all five observable operations,
the exact child-call event, raw and unknown child results, live saved-register
rewrites, both unknown epilogue reads, every operation-budget prefix, callback
refusal, malformed metadata, missing memory, alignment, wrapping regions, and
argument validation. `tests/game_main_tests.cpp` composes the wrapper at its
only natural caller and proves the installed-to-removed lifecycle.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers—never computer-control clicks—
then reaches this wrapper through recovered main. It captures
`vblank-shutdown-before.ppm`, `vblank-shutdown-after.ppm`, the exact service
event, receipt, and trace. The frames must be pixel-identical because callback
unregistration performs no rendering. They establish native reachability and
absence of a direct visual effect, not a retail gameplay frame.

Main's immediately following game-clock shutdown call at `0x80029B6C` is now
recovered separately; see [game-clock shutdown](game_clock_shutdown_workflow.md).

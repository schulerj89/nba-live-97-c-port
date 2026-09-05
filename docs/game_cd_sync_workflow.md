# GAMEONLY CdSync wrapper recovery

`src/recovered/game_cd_sync.c` owns all eight instructions of GAMEONLY
`0x8009DBA0..0x8009DBBF`. A fresh read-only Ghidra listing reduces the wrapper
to this source sequence:

```text
return 0x8009E740(mode, result_buffer);
```

The original bytes have SHA-256
`3950cb563b219b3b5b59d41cd74547b23be952e3f494769fc8d77fe186380db3`.
Main reaches it at call PC `0x80029B34`, after the first twenty post-FELOAD
presentation waits, and passes `CdSync(0, NULL)`. Ghidra also reports callers
at `0x80092028`, `0x80092164`, and `0x80092274`; those surrounding paths are
not claimed here.

The eight-instruction shape alone is not enough to name a PsyQ wrapper because
several SDK wrappers have the same prologue/call/epilogue form. The called
routine at `0x8009E740` contains the source diagnostic name `CD_sync` and its
timeout diagnostics, while main's argument pattern and position agree with
`CdSync`. This is why the port records the public wrapper as `CdSync` while
leaving the 160-instruction internal routine as an explicit synchronous CD
service boundary.

The native diagnostic service returns `CdlComplete` (`2`) for main's ordinary
call. That fixture does not claim physical CD progress, device timing, result-
buffer writes, or the internal routine's complete state machine. Translating
those effects without owning `0x8009E740` would make the native port look more
complete than the source evidence supports.

Compatibility preserves everything the wrapper itself establishes. It forwards
both arguments unchanged without validating or dereferencing the result pointer,
passes the child's `v0` through unchanged (including unknownness), reloads saved
`ra` from mutable mapped stack after the child, and adds no timeout, polling, or
return normalization. The internal routine has its own timeout diagnostics;
the statement here is only that this wrapper adds none. Native mapping,
knownness, callback-refusal, alignment, and operation-budget failures are host
safety boundaries, not original branches.

`tests/game_cd_sync_tests.cpp` covers the ordinary call, arbitrary arguments,
unmapped raw result pointers, live and unknown child returns, mutable and unknown
saved `ra`, callback refusal, malformed knownness, every operation-budget prefix,
and mapped-memory validation. `tests/game_main_tests.cpp` composes the owner at
the natural main call frame and checks the exact service event and return value.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers—never computer-control clicks—
and then reaches this call through recovered main. It captures
`cd-sync-before.ppm` and `cd-sync-after.ppm`, exact callback logs, progress, and
the source receipt. The frames must be pixel-identical because synchronization
does not render. They prove the native click-through reached the wrapper and
that it had no direct scanout effect; they are not retail pixels, a gameplay
frame, or evidence that the untranslated internal CD service is complete.

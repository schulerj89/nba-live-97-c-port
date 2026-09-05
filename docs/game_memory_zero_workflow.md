# GAMEONLY zero-fill entry recovery

`src/recovered/game_memory_zero.c` owns the effective execution path entered at
GAMEONLY `0x800A3A74`. The symbol is easy to misread in the recomp function map:
its declared body is one instruction, `ori a2,zero,0`, but it has no return or
jump. It falls directly into all 80 instructions of the shared optimized
memory-fill core at `0x800A3A78..0x800A3BB7`. The recovered C owner therefore
implements 81 executed instructions rather than treating the four-byte entry
shim as a complete clear.

The entry bytes have SHA-256
`3eec77d0e95c14d4c06c9e1d4548029c2bcc34fa7770a485652dbb193a79036c`,
the shared core has SHA-256
`5cf83e6e51d1bf5e8b4accba1415bedee7aa4d9a5c63c188b29f34b1678825f8`,
and their effective 324-byte path has SHA-256
`968a1ee3cee7769e2adb6c49db48dfe8836a0c76d91f05581076bf809690f772`.
A fresh read-only Ghidra audit identifies eleven direct callers of the zero
entry and three callers of the adjacent general-fill entry. This recovery owns
the zero entry only; it does not claim those general-fill calls are composed.

Main reaches it at call PC `0x80029B84`, immediately after controller suspend:

```c
/* Original addresses remain provenance for retained PS1 state. */
Nba97GameMemoryZeroContext zero = {
    memory, 20, 0x800D6DEC, 0x20,
    controller_suspend_progress.return_v0,
    controller_suspend_progress.return_v0_known
};
nba97_game_memory_zero(&zero, &memory_zero_progress);
```

This clears eight four-byte game-clock callback words before main copies and
enters FELOAD. The earlier recovered clock initializer clears the same table,
so the ordinary cold diagnostic sees zero both before and after this call. That
does not turn the call into a no-op: the receipt proves the source executes nine
stores and 36 bytes of store traffic over 32 unique destination bytes. The
last `SWL` repeats the final four bytes already cleared by the word loop.

The owner deliberately retains the optimized source schedule:

- a little-endian `SWR` handles the initial unaligned fragment;
- explicit 128-byte, 16-byte, and four-byte tiers issue their original word
  stores in order;
- a little-endian `SWL` handles or redundantly repeats the tail;
- all address and count arithmetic wraps at 32 bits;
- no destination bytes are read, and every completed store makes its written
  knownness bytes known zero;
- `v0` is never assigned, so the incoming value and knownness remain live; and
- a mapping failure or native operation bound retains the completed source
  prefix rather than rolling stores back.

Two retail count bugs are preserved. The signed `length < 4` branch places its
first byte store in a branch delay slot, so length zero and ordinary negative
lengths still clear one byte. `INT_MIN` is exceptional: subtracting one wraps
to `INT_MAX`, after which the loop attempts `0x80000000` byte stores. The native
operation budget makes that path inspectable without pretending the source
terminates early; it does not repair the behavior into `memset`.

`tests/game_memory_zero_tests.cpp` performs 3,287 assertions over the natural
call, every alignment, all three unrolled tiers, exact source-oracle store
traffic, every natural-call budget prefix, live known/unknown `v0`, the
zero/negative/`INT_MIN` bugs, 32-bit address wrap, knownness, split/missing
mappings, malformed regions, and argument validation. `tests/game_main_tests.cpp`
also composes the owner at the real call and checks the nine-store result.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through recovered native input handlers—never computer-control clicks—
then reaches the owner through recovered main. It writes
`shutdown-table-zero-before.ppm`, `shutdown-table-zero-after.ppm`, the byte
state, store metrics, receipt, and trace. The two frames must be byte-identical
because the routine changes CPU callback-table state rather than scanout. The
138-frame suite proves native reachability and absence of a direct visual
effect; it does not claim the typed downstream boundaries rendered a playable
court or possession.

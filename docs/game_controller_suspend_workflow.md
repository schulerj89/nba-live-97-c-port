# GAMEONLY controller-suspend recovery

`src/recovered/game_controller_suspend.c` owns all 14 instructions of
GAMEONLY `0x8008F19C..0x8008F1D3`. A fresh read-only Ghidra listing and a
bounded source-instruction oracle establish the complete wrapper:

```text
suspended = *(uint32_t *)0x800C4A70;
save ra;
if (suspended == 0) {
    controller_shutdown_80091224();
    suspended = 1;
    *(uint32_t *)0x800C4A70 = 1;
}
reload ra;
return suspended;
```

The original bytes have SHA-256
`40a13c532487813e5aee2bb9caf333e1c69ddbb581cef01b9ae24ea103e10570`.
Main is its only caller, at `0x80029B74`, immediately after the recovered
game-clock shutdown. The earlier recovered `0x8008F1D4` wrapper performs the
opposite transition: its first startup call initializes pad sampling and
clears the same suspend flag. The natural diagnostic consequently reaches
`0x8008F19C` with active value zero, calls shutdown once, and publishes one.

The `0x80091224` child remains a typed synchronous service boundary. Its
descendants stop pad sampling and update controller bookkeeping in the PS1
program, but this wrapper does not justify detaching Windows keyboard or
gamepad devices. The native composition records that the service was reached
and lets the recovered owner make the proven flag transition; it invents no
host-input side effect.

Compatibility preserves source details that a cleaned-up C rewrite could
easily lose:

- the flag is loaded before stack allocation and before the saved-`ra` spill;
- the branch-delay saved-`ra` store runs on both paths;
- only exact zero calls the child and stores one;
- any nonzero fast-path word is returned verbatim, not normalized to one;
- the child's raw or unknown `v0` is discarded and replaced with known one on
  the active path;
- `ra` is reloaded from live mapped stack after the child; and
- a refusal, unknown byte, or budget limit retains the exact completed prefix
  instead of rolling it back.

`tests/game_controller_suspend_tests.cpp` covers active and arbitrary-nonzero
paths, exact operation prefixes, an unknown discarded child return, callback
refusal, malformed knownness, a child-mutated flag, mutable and unknown saved
`ra`, missing/unaligned/overlapping/wrapping memory, and null arguments. Its
deliberate alias places the saved-`ra` word on `0x800C4A70`: the pre-spill zero
still selects shutdown, the later flag store wins, and the live epilogue sees
return address one. `tests/game_main_tests.cpp` composes the owner at its only
natural call and checks the resume-to-suspend lifecycle.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup with the test's recovered input handlers—never computer-control clicks—
then reaches the wrapper through recovered main. It captures
`controller-suspend-before.ppm`, `controller-suspend-after.ppm`, the exact
child event, JSON receipt, and trace. The two frames must be pixel-identical
because this routine changes retained input state, not rendering. That proves
native reachability and absence of a direct visual effect; it is not a claim
that a retail court or playable possession was rendered.

Main's immediately following zero-fill entry at `0x80029B84` is now recovered
separately; see [shutdown-table zero fill](game_memory_zero_workflow.md).

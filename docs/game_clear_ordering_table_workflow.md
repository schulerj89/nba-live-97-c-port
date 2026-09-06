# GAMEONLY `PsyQClearOTagR` recovery (`0x80099960`)

This recovery owns the complete 152-byte, 38-instruction GAMEONLY routine at
`0x80099960..0x800999F7` inclusive. Its source is the fresh Ghidra listing
`game_80099960.txt`; the instruction-byte SHA-256 is
`083408a7237e02d799c70d934420dcff4a7abae0b49927919b183417ffd9750b`.
The known callers are `0x80035B94`, `0x80035BA0`, `0x800310A8`,
`0x800310B4`, `0x80049084`, `0x80049094`, `0x80036114`, `0x80036124`,
`0x80036CFC`, `0x8002B05C`, `0x800A4124`, `0x800A4290`, `0x8002B2A4`,
`0x80053788`, `0x800540A8`, and `0x800540D8`.

The C99 owner accepts a complete 32-GPR machine, HI/LO, and retained guest
memory with one knownness bit per byte. Guest pointers stay as PS1 `uint32_t`
addresses. Every reached mapped load/store and dynamic call attempt consumes
one operation. The access journal reports the original PC, address, value,
width, byte-knownness, kind, and one-based operation position. A stopped run
publishes the exact live machine prefix; earlier writes and completed callbacks
remain visible.

The source order is significant:

1. `LBU` reads the debug byte at `0x800C55C2` before the routine subtracts
   `0x20` from SP. The routine saves old S0 and S1, captures A0 and A1 into
   them, and computes `debug < 2`. The `BNE` delay slot always stores RA at
   live `SP+0x18`, including an unknown branch predicate.
2. Debug levels 0 and 1 branch past the diagnostic. Levels 2 through 255 load
   the live target from `0x800C55BC`, set A0 to `0x80028340`, A1 to live S0,
   set JALR RA to `0x800999A8`, set A2 to live S1 in the delay slot, and invoke
   the typed three-argument debug boundary at `0x800999A0`.
3. After any accepted diagnostic mutation, the routine reloads the dispatch
   table from `0x800C55B8` and loads its target from table offset `0x2C`. The
   JALR at `0x800999BC` sets RA to `0x800999C4`; its delay slot assigns A1 from
   live S1 before the typed two-argument backend boundary is checked.
4. After an accepted backend, including count zero or a negative raw count,
   the routine discards backend V0. It assigns V0 from live S0 and stores
   `0x000C567C` through that raw pointer. It reloads RA, S1, and S0 from the
   callback-mutable live SP, adds `0x20` to SP, and executes `JR RA` with its
   NOP delay slot.

Both JALR targets are runtime state. `0x8009CB2C` (debug) and `0x8009A97C`
(the initial table's backend) are retail evidence used by fixtures; neither is
hardcoded as behavior. A zero target is still offered to the typed callback.
An unknown target stops after the JALR register and delay-slot changes. An
unaligned target traps at that same prefix. The callback must return exactly
one and leave a valid full machine; the recovery does not translate the SDK
backend, reproduce diagnostic printing, or fabricate child results.

The frame adapter composes the existing `0x80049018` owner at its actual
`0x80049084` (32-entry table) and `0x80049094` (4096-entry table) calls. The
narrow parent event proves only the two arguments and JAL return address. A
provider must therefore supply an independent full entry machine for every
invocation. The adapter then binds zero, A0, A1, and RA to the actual call and
forwards every other frame service through its typed callback. The owner memory
can share native backing with the frame access fixture, so both CPU paths see
the same retained bytes without converting guest addresses to host pointers.

The focused suite covers debug levels 0/1/2/255, raw counts including
0/1/32/4096/`0xFFFFFFFF`, both call records and delay slots, dynamic target and
dispatch-table mutation, full callback machine mutation, all byte-knownness
masks, unknown control and target prefixes, stack/ordering-table/global native
aliases, 32-bit stack wrap, malformed metadata, missing memory and callbacks,
unaligned and zero targets, callback refusal, and every operation-budget
prefix. The natural suite executes the actual `0x80049018` owner, supplies two
different complete entry machines, checks both ordering-table head writes in
shared RAM, and keeps every other child as an explicit typed fixture.

The strict local validation commands are:

```powershell
$clang = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe'
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_clear_ordering_table.c -o $env:TEMP\game_clear_ordering_table.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_clear_ordering_table_tests.cpp $env:TEMP\game_clear_ordering_table.obj -o $env:TEMP\game_clear_ordering_table_tests.exe
& $env:TEMP\game_clear_ordering_table_tests.exe
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_match_frame.c -o $env:TEMP\game_match_frame.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered -c src/game_clear_ordering_table_adapter.cpp -o $env:TEMP\game_clear_ordering_table_adapter.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_clear_ordering_table_integration_tests.cpp $env:TEMP\game_clear_ordering_table.obj $env:TEMP\game_match_frame.obj $env:TEMP\game_clear_ordering_table_adapter.obj -o $env:TEMP\game_clear_ordering_table_integration_tests.exe
& $env:TEMP\game_clear_ordering_table_integration_tests.exe
```

This routine has no direct visual effect. It initializes one CPU ordering-table
head after the actual clear backend returns. Pixels cannot change until later
render-packet construction and GPU submission consume that table, so validation
uses retained CPU bytes, machine state, call metadata, and exact prefix order.

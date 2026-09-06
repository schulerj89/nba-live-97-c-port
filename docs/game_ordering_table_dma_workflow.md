# GAMEONLY ordering-table DMA backend (`0x8009A97C`)

This recovery owns the complete GAMEONLY routine at
`0x8009A97C..0x8009AA63` inclusive: 232 bytes and 58 instructions. The source
is the fresh Ghidra listing `game_8009a97c.txt`, with instruction-byte SHA-256
`3c4579b04a6bc93731d4328d630c48510c4cb64bdb40694267946013017bfaaa`.
Its code caller is the dynamic JALR at `0x800999BC`; `0x800C55A4` contains the
initial dispatch-table data reference. No prior complete owner was found.

The C99 owner accepts all 32 GPRs, HI/LO, retained stack/RAM/MMIO regions, and
one knownness bit for every little-endian source byte. Guest addresses remain
32-bit PS1 values. A mapped load, mapped store, or child call attempt consumes
one operation. The access journal records the source PC, address, value,
knownness, access kind, and one-based operation position. Every stop publishes
the exact live machine and retains all preceding stores and callbacks.

The source performs these operations in order:

1. It subtracts `0x20` from SP, saves S0 at `SP+0x10`, captures raw A1 in S0,
   loads the pointer at `0x800C56B0`, then saves RA and S1. It reads the pointed
   control word, ORs in `0x08000000`, and writes it back.
2. It loads the channel-control pointer from `0x800C56AC` and clears that
   pointed register. It calculates `(S0 << 2) - 4`, loads the address-register
   pointer from `0x800C56A4`, adds the wrapped offset to A0, and stores that raw
   result. It loads the count-register pointer from `0x800C56A8` and stores raw
   S0. Count zero therefore programs `A0-4`; `0xFFFFFFFF`, `0x80000000`, and
   every other raw value keep the original 32-bit shift/add wraparound.
3. It reloads `0x800C56AC`, stores `0x11000002` through the live pointer, and
   calls typed child `0x8009BAFC` at `0x8009A9F0`. JAL sets RA to
   `0x8009A9F8`; the delay slot is NOP and the source declares no arguments.
4. After the mutable child returns, the routine reloads `0x800C56AC` and its
   pointed channel-control value. It masks `0x01000000`. The BEQ delay at
   `0x8009AA14` assigns V0 from live S0 even when the predicate is unknown. A
   clear busy bit proceeds directly to the epilogue.
5. A busy channel assigns S1=`0x01000000` and calls typed child `0x8009BB30` at
   `0x8009AA1C`. JAL sets RA to `0x8009AA24`; the delay is NOP. The BNE tests
   child V0 while its delay always assigns V0=`0xFFFFFFFF`. A nonzero child
   result therefore returns `-1`. A zero result reloads the pointer and control
   register, ANDs with callback-mutable live S1, and branches back while busy;
   the loop branch delay always assigns V0 from callback-mutable live S0.
6. The epilogue reloads RA, S1, and S0 through callback-mutable live SP, adds
   `0x20` to SP, and executes JR RA with a NOP delay. An unknown restored RA
   stops only after those preceding effects.

The owner does not validate count, run a software ordering-table clear, or
fabricate DMA completion. `0x8009BAFC` and `0x8009BB30` remain typed
full-machine services. They may update device bytes, pointer globals, stack,
S0/S1/SP, other GPRs, and HI/LO. Accepted callbacks must return exactly one and
leave a valid machine; malformed accepted state reports `NBA97_TEXT_ARGUMENT`
with its live prefix. A permanently busy device repeats the exact source loop
until the explicit operation budget reports `NBA97_TEXT_LIMIT`.

The native adapter composes the existing complete `0x80099960` owner. It
intercepts only the exact backend event with call PC `0x800999BC`, delay PC
`0x800999C0`, runtime target `0x8009A97C`, and the parent's two-argument
metadata. It passes the same full machine and retained memory directly into the
DMA owner and copies the nested live machine back. It requires valid incoming
machine metadata and the source JALR return address `0x800999C4`; a rejected
entry leaves the caller machine byte-for-byte unchanged. When BAFC or BB30
accepts and produces malformed machine metadata, that accepted malformed prefix
crosses the adapter so the parent performs its own required argument check.
The diagnostic target
`0x8009CB2C` and any different dynamic backend stay with the parent's typed
callback. On a completed DMA error return, the parent preserves its original
quirk: it discards backend V0, writes `0x000C567C` to the ordering-table head,
and returns its live table pointer.

The focused asset-free suite covers count 0/1/32/4096/`0xFFFFFFFF`/
`0x80000000`, exact register programming and access order, immediate completion,
one and multiple waits, nonzero wait result, mutable pointers and machine state,
all byte-knownness masks, partial busy-bit evidence, every normal operation
budget, bounded perpetual busy state, RAM/MMIO/stack backing aliases, pointer
self-aliasing, stack wrap, unknown/misaligned/unmapped addresses, unknown JR,
null knownness storage, malformed regions/bytes/machines, and both callback
refusals. The natural suite runs the real `0x80099960` owner, validates exact
dynamic-call metadata, leaves the diagnostic typed, covers DMA success and
`-1`, and proves the unconditional parent head store in shared memory.

Strict validation uses the VS2022 LLVM toolchain:

```powershell
$clang = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe'
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_ordering_table_dma.c -o $env:TEMP\game_ordering_table_dma.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_ordering_table_dma_tests.cpp $env:TEMP\game_ordering_table_dma.obj -o $env:TEMP\game_ordering_table_dma_tests.exe
& $env:TEMP\game_ordering_table_dma_tests.exe
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_clear_ordering_table.c -o $env:TEMP\game_clear_ordering_table.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered -c src/game_ordering_table_dma_adapter.cpp -o $env:TEMP\game_ordering_table_dma_adapter.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_ordering_table_dma_integration_tests.cpp $env:TEMP\game_ordering_table_dma.obj $env:TEMP\game_clear_ordering_table.obj $env:TEMP\game_ordering_table_dma_adapter.obj -o $env:TEMP\game_ordering_table_dma_integration_tests.exe
& $env:TEMP\game_ordering_table_dma_integration_tests.exe
```

Gameplay shown: NO - no direct visual effect. This routine programs and polls
DMA registers. It performs no GPU submission and cannot directly change a
pixel. Matching visual hashes remain manager-owned shared integration evidence;
this bounded module proves exact CPU/MMIO state and call prefixes. No asset or
fixture pose is part of it.

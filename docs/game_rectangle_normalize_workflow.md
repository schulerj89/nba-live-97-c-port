# GAMEONLY rectangle normalization recovery

`game_rectangle_normalize` owns exactly `0x80094440..0x8009446B`, 44 bytes
and 11 instructions. The recovery follows fresh Ghidra evidence
`game_80094440.txt`, instruction SHA-256
`758caadbf9bfc2e94e142374466f354974715d9525934b12817928022cb73d7f`.
Its callers are recovered CQ at `0x80094508` and the currently unowned
`0x800944B4`; it has no callees.

The strict C99 owner retains all 32 GPRs, HI, LO, and per-byte knownness. It
reads the unsigned width at `a0+4`, keeps only its low bit, and returns V0 zero
without touching height when the width is even. Odd width reads the unsigned
height at `a0+6`, ORs its low bit, stores the halfword to the same address, and
leaves that unsigned value in V0. The owner preserves exact access prefixes,
32-bit address wrap, alignment behavior, and the JR NOP delay. Each mapped
access consumes one operation.

The focused runtime-generated suite checks all 11 instructions; zero, even,
`0x8000`, and `0xFFFF` widths; heights `0`, `1`, `0xFFFE`, and `0xFFFF`; the
original odd-height rule; exact one-read and read/read/store access paths;
complete GPR/HI/LO retention; every operation prefix; unknown low and discarded
unknown high width bytes; unknown and malformed height reads; all 16 RA masks;
unaligned RA after the JR delay; unknown, unaligned, and unmapped pointers;
aliased host backing; overlapping, wrapped, zero, SIZE_MAX, and exact-2^32 guest
region boundaries; invalid machine masks; and deterministic RAM, knownness,
and full-machine output.

The natural suite executes the actual recovered CQ owner
`0x800944F4..0x8009453F`. Its real first call uses PC `0x80094508`, delay
`0x8009450C`, RA `0x80094510`, and argc 1. An even `16x1` rectangle remains
unchanged, while a generated odd `17x2` rectangle becomes `17x3`; CQ then
rebuilds its callback-live pointers, reaches the typed `0x8009971C` fallback,
writes the pending flag, restores its frame, and returns. The suite also checks
binding reuse, normalization budget failure, `0x8009971C` refusal after a
completed normalization, parent prefixes, and malformed event guards.

The strict validation commands are:

```powershell
$clang = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe'
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_rectangle_normalize.c -o $env:TEMP\game_rectangle_normalize.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_rectangle_normalize_tests.cpp $env:TEMP\game_rectangle_normalize.obj -o $env:TEMP\game_rectangle_normalize_tests.exe
& $env:TEMP\game_rectangle_normalize_tests.exe
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_rectangle_upload_submit.c -o $env:TEMP\game_rectangle_upload_submit_cs.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered -c src/game_rectangle_normalize_adapter.cpp -o $env:TEMP\game_rectangle_normalize_adapter.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_rectangle_normalize_integration_tests.cpp $env:TEMP\game_rectangle_normalize.obj $env:TEMP\game_rectangle_upload_submit_cs.obj $env:TEMP\game_rectangle_normalize_adapter.obj -o $env:TEMP\game_rectangle_normalize_integration_tests.exe
& $env:TEMP\game_rectangle_normalize_integration_tests.exe
```

Visual classification: no direct visual effect. The routine only normalizes a
CPU rectangle descriptor; it does not render or submit pixels. Manager-owned
native capture composes CI through CM and CQ and compares pixel-identical User
Setup frames while observing the even rectangle and a generated odd rectangle.
No rendered match or tip-off gameplay is claimed.

Manager comparison matched 8,192 cases against the private original 11-instruction source: all 34 machine words and output masks, full mapped RAM and every operation-budget prefix. Native capture proves the same-memory countdown/image/upload/normalization path and a separate generated odd-width CQ invocation, with identical before/after User Setup frame hashes. The remaining GPU service stays typed.

# GAMEONLY rectangle upload submission recovery

`game_rectangle_upload_submit` owns exactly `0x800944F4..0x8009453F`, 76
bytes and 19 instructions. The recovery follows fresh Ghidra evidence
`game_800944f4.txt`, instruction SHA-256
`bc37688dc8aae684e9c98121320f42e0385f49e7ddad7e6cc93f3257305b2f75`.
Its known callers are `0x8009464C` and `0x800A8790`. The first typed child
signature is independently confirmed by `game_80094440.txt`, SHA-256
`758caadbf9bfc2e94e142374466f354974715d9525934b12817928022cb73d7f`.

The strict C99 owner preserves all 32 GPRs, HI, LO, and per-byte knownness. It
saves incoming `s0`, `s1`, and `ra`; forwards live `a0` to typed child
`0x80094440`; captures `a1` in that JAL delay; rebuilds `a0` and `a1` from
callback-live `s0` and `s1` for typed child `0x8009971C`; and writes `1` to
`0x800D7B14` only after both calls succeed. It then restores through
callback-live `sp`, advances that stack by `0x20`, and executes the `JR` NOP
delay before validating the restored target. Every mapped word access and
typed call consumes one operation and exposes its exact completed prefix.

The focused runtime-generated fixture executes all 19 instructions and checks
both call PCs, targets, delay slots, argument counts, JAL return addresses, the
pending-flag write, exact source access order, callback-live `sp`/`s0`/`s1`,
relocated restores, callback HI/LO and untouched-register retention, absent and
refused callbacks, invalid callback machines, every operation-budget prefix,
all 16 saved-RA masks and the JR NOP, unknown spills with and without a known
plane, unknown and unaligned stack pointers, a malformed final restore load,
guest-region overflow/overlap/SIZE_MAX validation, and deterministic full RAM,
knownness, and machine output.

The natural fixture runs the actual recovered image-record owner
`0x80094540..0x800946A3`. A generated type-`0x23` record reaches its real
`0x8009464C` call with delay `0x80094650`, RA `0x80094654`, and two arguments.
The CQ owner then reaches both typed services with descriptor
`(0x340, 0xF0, 0x10, 1)`, sets `0x800D7B14`, and returns so the CM owner can
reload its record link and restore its own frame. The integration suite also
checks binding reuse, parent fallback for CM's `0x800A3BF8` child, nested
refusal and invalid-machine publication, child budget failure, and malformed
kind/PC/delay/entry/invocation/argc/RA guards.

The strict validation commands are:

```powershell
$clang = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe'
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_rectangle_upload_submit.c -o $env:TEMP\game_rectangle_upload_submit.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_rectangle_upload_submit_tests.cpp $env:TEMP\game_rectangle_upload_submit.obj -o $env:TEMP\game_rectangle_upload_submit_tests.exe
& $env:TEMP\game_rectangle_upload_submit_tests.exe
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_image_record_upload.c -o $env:TEMP\game_image_record_upload_cq.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered -c src/game_rectangle_upload_submit_adapter.cpp -o $env:TEMP\game_rectangle_upload_submit_adapter.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_rectangle_upload_submit_integration_tests.cpp $env:TEMP\game_rectangle_upload_submit.obj $env:TEMP\game_image_record_upload_cq.obj $env:TEMP\game_rectangle_upload_submit_adapter.obj -o $env:TEMP\game_rectangle_upload_submit_integration_tests.exe
& $env:TEMP\game_rectangle_upload_submit_integration_tests.exe
```

Visual classification: UI/menu upload dependency. Gameplay shown: BLOCKED on
the unresolved typed `0x80094440` rectangle-normalization and `0x8009971C`
rendering services. The fixtures prove CPU memory, full machine state, call
metadata, and access order; they do not claim renderer completion, native
pixels, or gameplay.

Manager review compared 10,240 runtime-generated cases with the private original instruction stream, visiting all 19 instructions and matching all 34 machine words and output masks, full mapped RAM, callback-entry machines, relocated stacks and every operation prefix. Native input-driven capture composes the actual countdown, image-record and rectangle-submit owners on the same generated memory, checks both typed upload calls and pending flag 0 to 1, and retains the User Setup frame. This remains blocked on the two typed GPU services and is not an advancing match.

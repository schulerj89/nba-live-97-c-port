# GAMEONLY image-record upload recovery

`game_image_record_upload` owns exactly `0x80094540..0x800946A3`, 356 bytes
and 89 instructions. The recovery was checked against the fresh Ghidra listing
`game_80094540.txt`, whose instruction SHA-256 is
`a5143c4c8d65ce4f5f2cedbc38824e250bb702edad2358fff88bbeed885c8f9d`.
The four source callers are `0x8002E8AC`, `0x80032AE4`, `0x800947C8`, and
`0x800947EC`. The only children are typed full-machine boundaries:
`0x800A3BF8` at `0x800945C8` and `0x800944F4` at `0x8009464C`.

The strict C99 owner keeps all 32 GPRs, HI, LO, and per-byte knownness. It
models the prologue and callback-live stack, header bit-3 masking, the distinct
type-`0x23` and type-`0x40..0x43` paths, ordered record and descriptor stores,
signed `MULT` and its HI/LO publication, both source rounding paths, fresh
post-callback header reads, signed relative links, cycles bounded by the
operation budget, and the six ordered restore loads followed by the `JR` NOP
delay. Each mapped read, mapped write, and typed call consumes one operation;
refusals preserve the exact completed prefix.

The manager's independent raw-instruction comparison passed 13,280 cases over
all 89 PCs. It compared all 34 machine words and masks, the full 2 MiB guest
RAM, callback snapshots, 12 stack words, callback-live `sp`/`s0`/`s1`/`s4`,
type boundaries, signed relative chains and cycles, and every operation prefix.

The focused fixture creates its complete 2 MiB guest image at runtime. Its four
test groups cover null and linked records, full-word versus low-halfword zero
tests, all dispatch boundaries, bit-3 headers, positive and negative rectangle
math, HI/LO, exact access and callback metadata, callback refusal and invalid
machines, callback-live `s0` and `sp`, relocated restores, per-byte unknownness,
alignment, unknown stores without a knownness plane, every operation-budget
prefix, every restored-RA knownness mask, deterministic bytes/knownness/full
machine output, and preservation of untouched registers.

The natural fixture executes the actual recovered countdown owner
`0x8003287C..0x80032B0F`. Its real `0x80032AE4` call and `0x80032AE8` delay
store enter this owner with RA `0x80032AEC`, five arguments, and the fifth
argument visible at live `sp+0x10`. The generated countdown table reaches the
typed `0x800944F4` boundary with rectangle `(0x340, 0xF0, 0x10, 1)`, changes
the record header from `0x23` to `0x2B`, returns to the countdown owner, and
preserves its later `s2` cache store of `2`. The fixture also checks binding
reuse, nested refusal, parent budget refusal, and malformed call identifiers,
arguments, invocation, delay PC, and register values.

The strict validation commands are:

```powershell
$clang = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang.exe'
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_image_record_upload.c -o $env:TEMP\game_image_record_upload.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_image_record_upload_tests.cpp $env:TEMP\game_image_record_upload.obj -o $env:TEMP\game_image_record_upload_tests.exe
& $env:TEMP\game_image_record_upload_tests.exe
& $clang -std=c99 -Wall -Wextra -Werror -pedantic -Isrc/recovered -c src/recovered/game_countdown_ui_update.c -o $env:TEMP\game_countdown_ui_update.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered -c src/game_image_record_upload_adapter.cpp -o $env:TEMP\game_image_record_upload_adapter.obj
& $clang -std=c++17 -Wall -Wextra -Werror -pedantic -Isrc -Isrc/recovered tests/game_image_record_upload_integration_tests.cpp $env:TEMP\game_image_record_upload.obj $env:TEMP\game_countdown_ui_update.obj $env:TEMP\game_image_record_upload_adapter.obj -o $env:TEMP\game_image_record_upload_integration_tests.exe
& $env:TEMP\game_image_record_upload_integration_tests.exe
```

Visual classification: UI/menu rendering dependency. The routine changes CPU
record bytes and submits an upload descriptor, but gameplay and pixel output
remain blocked on the typed `0x800944F4` and `0x800A3BF8` rendering services.
Validation therefore observes retained memory, full machine state, callback
metadata, and access order; it does not claim native gameplay or rendered
pixels.

Manager integration executes the active countdown owner and this upload owner on
the same synthetic retained memory through the production adapter. The existing
native self-driving frontend test records the exact caller, 25 operations
(11 reads, 13 stores, one typed upload), rectangle `(832,240,16,1)`, header
`35 -> 43`, and the caller's subsequent cache `65535 -> 2`. Its ignored receipt
is `image_record_upload_verified.json`. Both CPU checkpoint frame hashes remain
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The displayed screen is User Setup. Gameplay shown: BLOCKED on the typed
`0x800944F4` upload and `0x800A3BF8` dimension service; the parent also retains
its typed `0x80030D18` text prerequisite. Focused and natural tests pass, as does
the complete 373-test asset-free suite and all metadata freshness checks.

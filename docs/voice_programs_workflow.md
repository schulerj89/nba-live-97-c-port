# Bank dispatch and the Cool Facts callback contract

`voice_programs.c` closes the CPU request path93098/9180C/92B74 and registration
path919A0/92628. PATl upload and mapping CPU owners now compose through the
borrowed table API; actual lower SPU operations, PT upload and voice launch
remain mandatory backend operations. A rejected native memory/backend operation returns
`NBA97_PROGRAM_IO_REFUSED`, never a fabricated original handle or successful
registration. This work does not wire full31A88 into the host.

## Native interface

`Nba97VoicePrograms` borrows the same voice enabled byte through
`Nba97VoiceHandles` and ten actual D99E4+i*8 bank-header tokens. READ8/16/32 and
WRITE32 callbacks must resolve owned source memory, with explicit32-bit address
wrapping and correct little-endian widths. Reads do not mutate source state.
Unknown nonzero tokens refuse; they are not treated as empty banks/programs.
No immutable archive bytes are silently relocated in place: any mutable runtime
bank/program must have its own real owned representation and lifetime.

`nba97_voice_bank_validate` preserves93098: indices outside unsigned0..9 or a
null bank token return-8, otherwise0. It does not inspect a bank header or
pretend to validate its allocation lifetime.

`nba97_voice_program_play(bank, program, volume)` preserves9180C: disabled
returns-10 first; invalid bank/index returns-8. A header byte4 of0 selects
the legacy count128; any nonzero byte selects the unsigned16-bit count at+6.
The selected pointer is read from header+8+4*program. Null returns-8, PATl
dispatches to9267C, a low halfword PT dispatches to91CD8, and other tags return-7.
The complete eight-argument request is
`(program_token, bank, -1, -1, -1, -1, volume, -1)`. Volume is not clamped in
this owner; the actual program launch validates/defaults its parameters.
The original backend result is returned unchanged.

`nba97_voice_program_register(bank, output_program, header, body, table)`
preserves919A0 and92628. It does not check the voice-enabled byte. Invalid bank,
null header/body, zero capacity, or a full bank return the source error before
accessing the table. After finding a vacancy, source91A4C storesFFFFFFFF into
the first table word **before**92628 reads the program tag. This ordering is
observable when retained table bytes alias the program header.

The caller supplies the actual retained incoming24 bytes from source frame
sp+10..27, with their knownness. Registration rejects any other table extent
at the reached sentinel store, so a third record cannot read saved s0 atsp+28.
It does not initialize the other20 bytes or preflight them. This extent refusal
is a native ownership boundary; the original contains no such bounds check.
Malformed reached metadata returns `NBA97_PROGRAM_TABLE_METADATA`; unknown or
missing reached table storage returns `NBA97_PROGRAM_TABLE_RESOURCE`.

The request carries the SAME `Nba97VoiceMappingTable*` in `mapping_table`;
`argument[2]` remains zero rather than encoding a fake native address. No table
copy, scalar compatibility cast, or automatic terminator is permitted. PATl
upload924B4 and PT upload921F4 receive that pointer. Memory/play requests have
a null table pointer. Unknown tag returns-1. Any original negative upload
result writesFFFFFFFF to the caller's output while retaining table changes.
For a nonnegative result, registration writes the header token into the
previously selected bank address/index, writes that index to the caller, and
returns the original constant8. Callback changes to the bank pointer or vacancy
do not trigger another search. Output storage remains owned and distinct from
the memory/bank/table projection during this synchronous call.

Callbacks may mutate live state during upload/play, but return1 only after
performing the requested original operation. Successful synthetic results in
the tests exercise the boundary contract; they are not production backends.
Refusals retain already-performed operations and are not resumable cursors.

## Exact314A0 resource callback

The next frontend bridge needs this sequence, not a PNG/PCM-ready shortcut:

1. The resource descriptor passed as a0 is resolved by77CF8, which reads its
   first word. FB1E8 indexes the **owned F84C8 Z1COOL index** to obtain the
   slice length. CRC9045C over `length-2` must equal the final16-bit word.
   Mismatch returns0 before cleanup, upload or pending-state clear.
2. On match, run313C8 cleanup with its original active/stale announcer handle
   behavior. Store the new resolved sample-data token in ECF8C.
3. Call919A0 with bank21D6C, output DE484, header=data, body=data+74hex.
   **314A0 ignores the upload return.** Native I/O refusal still cannot be
   declared a complete source transaction: it means this callback is not yet
   implementable with the available resource/backend ownership.
4. Pump28BF0 exactly eight times, then call7AFB0 on the original resource
   descriptor with74hex. Its return is also ignored by314A0.
5. Reread live FDC00. Only value2 invokes9180C with bank21D6C, the live DE484
   program index, and `min(unsigned speech_setting * 15,127)`. Store the exact
   signed result in DED08. No successful handle may be synthesized if the
   real launch rejects or cannot be implemented.
6. Clear FDC00 and return1. This acceptance remains1 after an original upload
   or launch error; do not silently repair that source caller behavior.

7AFB0 forwards `(descriptor, requested_size, 1)` to7AFF0. That owner reads the
heap selector from descriptor+18hex, consults its real allocator metadata,
locks F7E88, and calculates aligned available/requested sizes using the
following descriptor's first-word address and the heap overhead/alignment.
Only on a fitting nonnegative aligned size does it write descriptor+14hex
(requested size) and+10hex (aligned size), optionally call8BFF4, unlock, and
return the requested size. Failure unlocks and returns the source size/error
result. Therefore the74hex call is an allocation-size/lifetime operation after
sample upload, not an audio-duration marker, an unconditional successful
resize, or a call to free borrowed native PCM. The heap/lock/coalescing effects
remain required before claiming this source callback is composed.

## Remaining program and voice operations

The inspected PATl upload924B4 relocates the program's tone pointer and
optional tone map/envelope pointers in place, then calls70884 for each tone's
mapping/body upload. Previously uploaded mappings can be freed through714B8
on later failure. The composed owner preserves this confirmed source quirk:
when the **first** tone upload fails, its saved cleanup bound becomes-1,
so the source skips cleanup and sets the loaded byte+5 to1 while returning
that negative failure. Relocations have already happened. Do not modernize
this into an atomic load or clear the loaded byte. Current registration
dispatch preserves the returned failure and output behavior. The bounded
relocation/mapping CPU effects are now implemented; actual allocator/transfer
completion and later voice launch remain separate required operations.

PATl9267C chooses a tone, applies source parameter/default/range/mapping and
randomization rules, calls the shared91338 allocator, initializes the selected
voice, calls76334 and70E54, and handles launch failure. PT91CD8 is a distinct
required path. Their random draws and source voice generation must not be
replaced with new seeds or WinMM lifetime IDs. `voice_allocation.c` now supplies
the real bounded allocator, including failure prefixes; it does not initialize
all missing launch fields or deliver SPU keyon/release. Physical-channel
metadata and hardware state must remain shared with `voice_channels.c`.

The implementation-ready frontend composition is consequently: retained raw
COOL index and accepted slice ownership; real314A0 checksum/cleanup callback;
mutable bank/program registration and mapping upload; original program launch
using the shared allocator/voice/channel owners; explicit native platform
effects; real descriptor shrink/release. Source STATUS/fade must observe those
same voice records. A decoded PCM vector alone closes none of the original
resource checksum, relocation, bank slot, allocator or SPU release obligations.
The existing checked WinMM storage owner may safely implement native buffer
lifetime once those source operations have a real adapter.

## Evidence

Current evidence is in
`.local/verification/native_completion/voice_mapping/table_api/`. Strict private
Debug and Release builds each pass45 program checks and5,400 original-instruction
comparisons, with479 backend events and206,068 executed instructions. All191
direct instructions are covered:93098=14,9180C=53,92B74=34,919A0=69,92628=21.
The adapted boundary comparison checks the whole24-byte table and pointer
identity; callback mutations still verify the saved bank-slot decision.

The additional composed comparison executes919A0/92628/924B4/70884 and reached
CPU wrappers:658 cases per configuration, including18 actual resource prefixes,
2,274 lower-boundary events and262,659 instructions. Synthetic cases supply
explicit allocator/transfer/event/free fixture effects. Every actual asset case
refuses the first required7EC2C allocator operation, preserving the real CPU
prefix without claiming a successful SPU allocation. Native table ownership
guards stop original execution at the exact read/store that would be refused;
they do not claim that the original game checks knownness or stack bounds.

The new `table_api/freeze.json` supersedes earlier program, PATl and standalone
mapping freeze bindings and the old scalar-only callback domain. Historical
receipts remain unchanged in their original directories. The source FE SHA-256
is `14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.
The program test now also links `voice_mapping_table.c`. No host, device, shared
build or saved state was used for this correction. Full31A88 still requires the
ordering in `frontend_resource_cleanup_workflow.md`: BEGIN, cleanup/graphics,
END, then outer selector PORT/COOL initialization. Neither registration nor a
render-complete event clears resource24 early.

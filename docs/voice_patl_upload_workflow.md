# PATl relocation and mapping-upload ownership

`voice_patl_upload.c` implements the whole924B4 CPU owner and91AB4 unload loop.
The actual919A0/92628 registration and70884/714B8 mapping CPU owners now compose
through the same borrowed table and memory registry. Lower SPU allocator,
transfer and event operations remain required backends. Neither a plausible
address nor a decoded PCM vector proves that those operations completed.
This bounded table API correction does not change the native host or audio.

## Owned memory and source-address provenance

`Nba97VoicePatlMemory` retains spans with explicit source addresses, native
storage, extent, source-address knownness, writability, and either a fully-known
domain or a per-byte known mask. Encoded32-bit addresses must come from the
actual source-address mapping; do not truncate native pointers or invent tokens.
Pointer arithmetic wraps as original32-bit arithmetic before registry lookup.
A span itself cannot wrap the source address space. Reads/writes require original
1/2/4-byte alignment and reject ambiguous source ranges rather than choosing an
alias arbitrarily.

Every flag and mask entry must be exactly0 or1. Every access validates span flags
and all mask entries in its requested width before refusing an unknown requested
byte or performing any store. A0 followed by marker2 in the same access cannot
hide malformed metadata. Unvisited allocation bytes are not scanned or
interpreted. `fully_known=1` allows no mask, or asserts every supplied mask byte
is1; consistency is checked when reached. Malformed reached metadata has its own
native refusal code. Writes need valid writable metadata, without reading old
destination bytes, and establish knownness through the mask. An entirely unknown
span with no mask cannot record that transition and refuses writes. These are
native safety rules, not repairs to original game behavior.

All storage, masks and span metadata remain valid and fixed during a call.
Callbacks may change bytes and knownness; later accesses revalidate without
caching. Defensive tests also reject corrupt metadata flags after callbacks.
Distinct proven source-address aliases may share native bytes and must share
corresponding knownness storage/semantics. They are never copied into separate
header state. A COOL slice may retain header and body at+74hex in one allocation;
a cursor bank may provide separate header/body spans. Read-only body spans are
supported. Resource checksum acceptance and allocation lifetime remain owned by
the caller's resource lifecycle.

`nba97_voice_patl_read/write` expose the same checks to the mapping backend.
924B4 itself does not consume body bytes; ownership/knownness must be established
at the reads required by70884 and its real transfer backend. Missing ownership
never becomes inferred body content or upload success.

## Preserved924B4 behavior

Loaded byte+5 is read first. A nonzero byte returns-1 without relocation or
mapping work. Otherwise the relative tone pointer+12 and count+7 are read, then
the pointer is replaced by `header + 12 + old_pointer`, even for zero tones.
Each tone rereads the live pointer and adds its92-byte stride. Optional pointers
at tone+24,+28,+32 relocate only when nonzero. Envelope+36 relocates even when
its old value is zero.

These stores precede70884 `(tone+40, body, mapping_table)`. Every tone receives
the SAME borrowed `Nba97VoiceMappingTable*` supplied by registration. No record
is reset, resized, copied or implicitly terminated between tones. Paths that
never call70884, including loaded and zero-tone programs, do not inspect the
table. Unload calls pass a null table and body zero. The common table helper
owns requested-word bounds/knownness checks; see `voice_mapping_workflow.md`.

After a nonnegative upload return, the live tone count is reread to decide
whether another tone is needed. Callback pointer/count changes therefore affect
later operations. The owner returns the last original upload result, or0 for
zero tones, and sets loaded byte+5 to1.

On a negative return the cleanup bound becomes `failed_index - 1`. A later-tone
failure unloads only previous mappings, in increasing stride order, rereading
the program pointer before each call. Cleanup results are ignored; relocation
and previous effects are not undone. The failed return is retained, and this
cleanup arm does not write the loaded byte.

The first-tone failure is a confirmed original bug: index0 makes cleanup=-1,
so the source takes the ordinary loaded=1 arm while returning the negative
failure. Retry sees the relocated, loaded header and returns-1 immediately.
Zero-tone programs also become loaded without mapping calls. These behaviors
are preserved and commented in code and tests.

91AB4 rereads the live tone pointer before each unload and live count afterward.
It ignores all714B8 results, returns0, and never clears loaded or reverses
relocation. A completed source error differs from a native memory/backend
refusal: the latter stops with preceding mutations intact and no invented
return. Refusals are not resumable cursors; do not retry partially relocated
storage as untouched raw input.

## Registration/backend composition

For `NBA97_PROGRAM_UPLOAD_PATL_924B4`, invoke `nba97_voice_patl_upload` with the
same retained registry, `request.argument[0/1]` header/body, and
`request.mapping_table`. Return callback success only on COMPLETE and preserve
the exact signed return bits. Registration retains its source output/bank-slot
decisions, including no slot publication after a negative upload result.

The program-register caller supplies the original24-byte incoming stack-table
region.919A0 initializes only its first word at91A4C, after finding a vacancy
and before92628 tag reads. Remaining bytes retain incoming knownness. No
`auxiliary` scalar compatibility cast is retained. The registry and table must
use the same runtime bank/program representation and aliases throughout;
immutable archive bytes must not be silently relocated. Actual714B8 must free
the resources actual70884 created. Missing operations refuse. WinMM lifetime
IDs and `RecoveredAudioPlayer::isPlaying()` cannot replace mapping ownership
or original voice state.

70884 now supplies mapping/header interpretation and the source allocation/
transfer sequence; the lower allocator and hardware/service contract remain
required. PT921F4/91CD8 and PATl9267C play are separate unowned operations.
314A0 descriptor shrink and announcer lifecycle remain described in
`voice_programs_workflow.md`. No resource24 completion or full31A88 transition
is claimed.

## Verification and limits

Current evidence is in
`.local/verification/native_completion/voice_mapping/table_api/`. Strict Debug
and Release builds each pass66 PATl checks and5,400 original-instruction cases,
with10,324 mapping boundary events and544,890 executed instructions. All93
direct924B4 instructions and29 of91AB4 are covered. The adapted proof checks
the whole24-byte table and SAME pointer through actual919A0/92628 composition.
Live count/pointer changes, loaded-byte behavior, cleanup order and prior
memory-safety tests remain covered.

A separate658-case comparison per configuration runs the full
919A0 -> 92628 -> 924B4 -> 70884 CPU chain with2,274 lower-boundary events and
262,659 instructions. It includes unknown remaining records, reached malformed
masks, third-record escape, callback effects and first/later source failures.
Native safety refusals compare original execution stopped immediately before
the corresponding access; no source bounds/knownness check is invented.

The18 actual prefixes now run through that chain to7EC2C, beyond the former70884
entry boundary: all12 cursor programs and six complete CRC-validated COOL
slices. They retain source relocation, mapping/body arguments and only the four
initialized table bytes. The missing allocator is refused; no SPU address,
upload completion or playback is fabricated. The corrected full inventory
remains1,185 nonempty COOL PATl slices and1,281 absent entries among2,466 IDX
entries, with reserved zero absent. Absent entries provide no header evidence.

`table_api/freeze.json` supersedes earlier program/PATl/mapping bindings and the
scalar-only callback contract. Historical receipts remain unchanged in their
original directories and no longer bind current binaries. The source FE SHA-256
is `14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.
The test now links `voice_mapping_table.c` alongside PATl/program owners.
No host/audio, CMake, Git, shared build, device or Ghidra state was changed by
this bounded correction.

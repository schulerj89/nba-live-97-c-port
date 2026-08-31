# PATl relocation and mapping-upload ownership

`voice_patl_upload.c` implements the whole924B4 CPU owner and91AB4 unload loop.
It composes with the existing919A0/92628 registration owner. Actual70884 sample
mapping upload and714B8 mapping unload remain required backend operations;
neither a plausible address nor a decoded PCM vector is accepted as proof that
those operations completed. No native host/audio or original gameplay behavior
is changed by this new standalone owner.

## Owned memory and source-address provenance

The owner uses `Nba97VoicePatlMemory`, a registry of retained spans with explicit
source addresses. Each span declares native storage, extent, source-address
knownness, writability, and either a fully-known domain or a per-byte known
mask. The encoded32-bit address must come from the actual source-address
mapping; do not truncate a64-bit native pointer or invent a token that existing
bank/program references cannot resolve. Pointer arithmetic wraps as original
32-bit arithmetic before registry lookup. A span itself cannot wrap the source
address space. Reads/writes require original1/2/4-byte alignment and reject
ambiguous source ranges rather than choosing a convenient alias.

Every flag and mask entry must be exactly0 or1. Every access validates span
flags and all mask entries in its requested width before refusing an unknown
requested byte or performing any store. A0 followed by malformed marker2 in
that same access cannot hide the malformed metadata. Unvisited allocation
bytes are not scanned or interpreted. `fully_known=1` allows no mask, or
asserts every supplied mask entry is1; consistency is checked as each byte is
reached. Malformed reached metadata has its own native refusal code.
Read permission requires the requested bytes to be known. Write permission
depends on valid writable metadata, without reading prior destination bytes;
a store makes those bytes known through the supplied mask. An entirely unknown
span with no mask cannot record that transition and refuses writes. These are
native safety rules, not repairs to original game behavior.

All storage, mask buffers, and span metadata remain valid and fixed throughout
the call. Callbacks may change bytes and known masks, so later accesses
revalidate the bytes they reach, without caching previous validation. Defensive
tests also reject a callback that corrupts a fixed
metadata flag. Distinct proven source-address aliases may share the same native
bytes; aliases must share the corresponding knownness storage/semantics. They
are not copied into separate header state. A COOL slice can provide one retained
allocation containing both the header and body at+74hex; a cursor bank can
provide separate retained header and sample-body spans. Read-only body spans
are supported. Resource checksum acceptance and actual allocation lifetime
remain responsibilities of the caller's source resource owner.

`nba97_voice_patl_read/write` expose the same checked memory access to the real
mapping backend. Body bytes are not consumed by924B4 itself. They must become
known/owned at the actual reads required by70884; this CPU owner does not infer
body contents or return success when that required backend is missing.

## Preserved924B4 behavior

The owner reads loaded byte+5 first. Any nonzero byte returns the original-1
without relocation or mapping work. Otherwise it reads the relative tone
pointer at+12 and tone count at+7, then writes the absolute tone pointer back
as `header + 12 + old_pointer`. Even zero tones perform this relocation.

For each tone, it rereads the live program pointer and adds the current92-byte
stride. Optional relative pointers at tone+24,+28,+32 are relocated only when
nonzero. The envelope pointer at+36 relocates unconditionally, including an old
zero value. These writes happen before70884 receives `(tone+40, body,
shared_auxiliary_output)`. The auxiliary output is the caller's actual native
word standing in for the original writable word; the mapping backend can
update it, and the same word is passed to every mapping operation.

After a nonnegative original upload result, the owner rereads the live tone
count before deciding whether another tone is needed. Callback changes to
pointer/count therefore affect the next operation. It returns the last original
upload result, or0 for zero tones, and sets loaded byte+5 to1.

On a negative original upload result, the cleanup bound becomes
`failed_index - 1`. A failure after at least one earlier tone unloads only the
previous mappings in increasing stride order. The program pointer is reread
before each cleanup call. Cleanup return values are ignored; relocations and
any other performed effects are not undone. The failed upload result is
retained and returned, and this cleanup arm does not write loaded byte+5.

The first-tone failure is a confirmed original bug: index0 makes cleanup=-1,
so it takes the ordinary loaded=1 arm while returning that negative upload
result. The header remains relocated and marked loaded; retrying it returns-1
immediately. Zero-tone programs likewise become loaded without mapping calls.
These behaviors are explicitly preserved in code and tests.

91AB4 rereads the program's live tone pointer for each unload call and rereads
the live tone count afterward. It ignores all original714B8 return values,
returns0, and never clears the loaded byte or reverses relocation. A completed
source error is distinct from a native memory/backend refusal: the latter
stops at that boundary with preceding mutations intact and no invented return.
Refusals are not resumable cursors. Do not retry a partially relocated header
as though it were untouched raw input.

## Registration/backend composition

For `NBA97_PROGRAM_UPLOAD_PATL_924B4` in the existing `Nba97VoicePrograms`
callback, invoke `nba97_voice_patl_upload` with the same retained memory
registry, `request.argument[0/1]` header/body, and `request.auxiliary` output.
Return callback success only when the CPU owner reports COMPLETE; propagate
its exact signed result bits. The existing registration owner then preserves
its own output/bank-slot decisions, including no slot publication after an
original negative upload result. The public composed tests and original-MIPS
oracle run this actual chain rather than a PATl-upload dispatch stub.

The registry must be the same owned runtime bank/program representation used
by the resource caller. It must not mutate immutable archive bytes or register
a token for an unrelated temporary copy. Registration, relocation and later
unload share one identity. Actual714B8 must retire the mapping resources that
actual70884 created. An unimplemented operation returns a native refusal;
`RecoveredAudioPlayer::isPlaying()` and WinMM lifetime IDs are not substitutes
for either mapping ownership or the original voice table.

For the next backend,70884 still owns mapping/header interpretation, SPU memory
allocation, transfer, and resulting source metadata. The actual allocator and
hardware/service contract is not implemented here. PT921F4/91CD8 and PATl9267C
play are separate unresolved operations. The314A0 descriptor shrink and shared
announcer lifecycle remain described in `voice_programs_workflow.md`. No
resource24 completion or successful full31A88 transition is claimed.

## Verification and limits

Private evidence is under
`.local/verification/native_completion/voice_patl_upload/`. Debug and Release
MSVC builds each pass43 public checks and5,400 original-instruction cases,
including10,324 mapping boundary events and544,890 executed instructions.
All93 direct instructions of924B4 and29 of91AB4 are covered. Cases include
full919A0/92628 composition, mutable mapping bytes and auxiliary outputs,
live count/pointer changes, first/later failure, and cleanup ordering. Native
tests additionally cover unknown bytes, malformed metadata hidden after an
unknown byte in the same access, unvisited malformed bytes left unchanged,
write-only destinations, read-only storage, aliases, alignment,
source-address wrap, conflicting provenance, and missing required backends.

Each configuration also compares18 actual private asset prefixes: all12
cursor programs and six nonempty COOL slices. The six COOL inner CRC16 values
were verified before use. Original execution halts at the first70884 entry;
the native callback refuses there. All relocated bytes, mapping/body arguments
and auxiliary input match at that boundary. **No successful upload result was
invented for these private assets.** The broader corrected inventory contains
1,185 nonempty COOL PATl slices and1,281 absent entries among2,466 IDX entries;
the reserved zero entry is absent. Absent entries provide no header evidence.

`oracle.py`, `source_mips.txt`, `asset_prefix.py`, per-configuration reports and
logs, and final `freeze.json` identify the current source/test binary hashes.
The source FE SHA-256 is
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.
The corrected header inventory receipt is in the sibling
`voice_allocation/asset_allocation_inputs_corrected.json`; it supersedes only
the earlier overbroad inventory claim, not the allocation instruction proof.

The public test links `voice_patl_upload.c` and `voice_programs.c`. No existing
frozen source, native audio/host file, CMake, Git state, shared build, device,
or Ghidra project was changed. This is a bounded CPU/resource-memory owner
ready for the root's review and subsequent coordinated backend integration.

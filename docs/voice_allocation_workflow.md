# Original voice allocation and generation

`voice_allocation.c` implements all of `91338` and its membership helper
`912E8`, using the existing full handle/lock/stop/status owners. It is a CPU
allocation owner, not a successful native playback adapter or an SPU voice
release simulation. All original failures and partial writes are retained.

## Shared state and call boundary

`Nba97VoiceAllocation.shared` borrows the same `Nba97VoiceHandles`, 24
`Nba97MusicVoice` records, clock, and physical-channel stop metadata used by
the frozen voice service. New `Nba97VoiceAllocationRecord` fields project
age at record+16, unsigned priority at+15, and linked bytes+4..+10. Linked
allocation indices are distinct from F0D58 physical channel kind/paired
metadata; do not substitute one for the other.

The caller supplies the actual shared source generation word GP+178/D9BAC,
the separate membership count GP+254/D9C88, and eight scratch bytes
D9C8C..D9C93. The original function itself accesses the scratch base both
directly and through GP+258; that equality identifies GP as800D9A34. The
normal output word is D9C94, immediately after this scratch span. The native
API requires it to be distinct from the projected scratch/other fields.
No original32-bit pointer is cast to a native pointer.

`nba97_voice_allocate(state, mask, requested_count, priority, output_handle)`
returns a completion tag and the original signed result. On complete source
success, that result is a physical index; `output_handle` carries the source
generation/index handle. They are not interchangeable. On complete source
failure, the result is-9, but the output word or shared state may already have
changed. Native pointer errors, unowned scratch/slot reads, or the existing
timer trap have different completion tags. Their preceding writes are retained
and the call is not a resumable transaction.

Only the first eight scratch bytes are owned here. A ninth write/read refuses
when reached, rather than inventing storage over the adjacent output/service
globals. Signed selected indices outside0..23 likewise refuse at the first
voice dereference. This is a native memory boundary, not an original count
clamp. Actual private asset inspection found requested count1 in all2,466
Z1COOL records including fallback and all12 cursor programs. Every speech
header uses priority0 and maskFFFFFFFF. That inventory does not establish
unknown program inputs or replace allocation proof.

## Preserved source decisions and quirks

The owner locks, clears only `requested_count` scratch bytes when the signed
count is positive, and increments generation by32. If the resulting signed
word is negative, it becomes0. This happens before selection can fail. The
source generation is not the native WinMM64-bit lifetime ID and consumes no
RNG. In particular, generation wrap can produce a valid first voice handle0.

Free candidates are scanned in physical order0..23, require a set mask bit
and active byte exactly0, and consult `912E8`. The membership helper reads its
separate signed count once per invocation and compares signed scratch bytes.
The allocator never synchronizes that count with the requested count. A zero
or short membership count can therefore admit duplicate selections; this is
preserved in code and tests, without guessing a missing producer.

If more candidates are needed, the owner scans the same mask and membership
test for the lowest unsigned priority. Equal priorities select the lowest
**signed** age; equal ages retain the first physical index. Age is the shared
D9CDC service counter when activation succeeds. Candidate defaults are
priority102, age7FFFFFFF, index0. A requested priority at least102 can accept
that default index even when mask0 provided no candidate. This original
outside-mask fallback is deliberately retained.

After exactly the requested number is found, the function publishes the first
new handle both to its output word and the first voice **before** stopping or
checking any selected voice. For each selected voice, active byte exactly1
calls original STOP, then original STATUS must return1 before marking it active
and writing priority/age. STOP merely queues keyoff. With the outer lock still
held, an active stolen voice normally remains active during the immediate
STATUS and the allocator returns-9. It retains the published handle, keyoff
mask, and any earlier activations. It neither waits for hardware release nor
rolls back the prefix. Active values other than0/1 follow the original separate
selection/status decisions; they are not normalized on entry.

Successful multiple allocation writes linked indices into the first record and
sets the other selected voice handles toFFFFFFFF. A zero requested count has
another preserved quirk: when the free scan adds no candidate, stale
`selected[0]` still publishes/changes a handle without activating a voice.
The outer unlock runs the real pending timer. It can retire a voice before
the function returns its already-saved physical index; this is not repaired.

## Proof and next integration

Private evidence is under
`.local/verification/native_completion/voice_allocation/`. Standalone Debug
and Release builds pass27 public checks. Each configuration compares4,000
cases against the original FE instructions, including915 platform callback
events and3,976,325 executed instructions. Coverage is20/20 direct instructions
for912E8 and221/221 for91338. Nested lock, handle resolve, stop, status, pending
timer, and voice service execute their real original owners; hardware/APPLY
effects are explicit boundaries. `source_mips.txt`, `oracle.py`, per-build
logs, `report-Debug.json`, `report-Release.json`, and `freeze.json` record the
source, implementation and tested binary hashes. `asset_allocation_inputs.json`
records the separate header inventory. Source FE SHA-256 is
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.

To build the public test, link `voice_allocation.c`, `voice_handles.c`, and
`music_voice.c`. No host, shared build, audio device, or saved state was touched.

The immediate caller remains the program launch owner: PATl9267C locks,
passes the selected tone's mask/count/priority into91338 with output D9C94,
checks its physical-index result, and then initializes further voice fields
and launches70E54. Explicit requested physical voice overrides the mask and
uses priority101. The program launch also owns source randomization and
mapping/envelope reads; this allocator must not manufacture those effects.
See `voice_programs_workflow.md` for the now-closed bank/request dispatch and
the remaining resource/upload/launch contract. Failed native WinMM retirement
is already handled by `RecoveredWaveOutput`, but does not supply original
voice active state or hardware completion.

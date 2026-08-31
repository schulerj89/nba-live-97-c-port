# View Player resource completion and music unblock

`src/recovered/frontend_resource.c` closes the content decision behind the
View Player music unblock and the scalar ownership of its Cool Facts index.
It does not turn PNG readiness, a rendered frame, or a UI fade into a source
resource completion. Frozen music playback and transition files are unchanged.

The relevant owners are distinct:

| Source field | Actual owner |
|---|---|
| F9418 | Loaded Z1PORT.IDX data allocation |
| F95E8, F95F4 | Two portrait caches: physical record, owned slice data, graphic ID |
| F84C8 | Loaded Z1COOL.IDX data allocation, released by31A88 and3122C |
| FB214 | Z1COOL.BIG filename, not an allocation |
| DE484, DED08, ECF8C | Announcer program, announcer voice, speech sample data |
| 93708 | Shared accepted whole-file data pointer used during index loading |
| F9720 | Music selection block cleared by the portrait checksum-success arm |

Actual instructions in5A538 call30D14 with Z1PORT.IDX/BIG at5A558, then3122C
with Z1COOL.IDX/BIG at5A570. F84C8 is neither the portrait nor the streamed
menu music. Native32-bit tokens must resolve to owned allocations; never cast
a64-bit host pointer into these recovered fields.

## Checksum and publication boundaries

9045C computes CRC16 with initial value FBEA, reflected polynomial A001, and
no final xor. The implementation generates the recurrence arithmetically;
all256 entries match the original512-byte split lookup table. Native APIs
refuse invalid pointers and lengths outside0..INT32_MAX; the source did not
provide these memory-safety checks. Refusal is not a source checksum result.

The installed whole-file validator is8ABF0, installed by8ACB0 into D9B50.
2F870 calls it before publishing the resolved handle data to93708. CRCF at
file length minus12 identifies a trailer. The stored checksum is the full
little-endian32-bit word at length minus4; CRC covers length minus12 bytes.
With the retail D9B3C value12, acceptance shrinks the allocation by12. The
file's own trailer length word is ignored: retain this source quirk. Missing
CRCF is accepted when D9B40/strict is0, rejected otherwise; do not silently
require trailers for every source resource. A mismatch frees the whole-file
handle and returns0 so the queue retries. Successful2F870 publishes93708,
records elapsed clock, and returns1.

`nba97_resource_validate_file` implements that content decision, including
the full32-bit checksum comparison, optional trailer, and retained byte count.
Allocation, shrink, free, clock, and queue ownership remain adapter effects.
Actual Z1PORT.IDX3970bytes becomes3958bytes; Z1COOL.IDX19746 becomes19734.
Both have an inner two-byte checksum followed by the12-byte CRCF trailer.
The checksum of each inner payload including its two checksum bytes is0,
matching the outer stored32-bit zero. Do not strip an extra two bytes as part
of the source whole-file shrink.

30E78 receives `(handle, identity, filename)` from the resource queue.
Identity10/11 selects a live cache record, whose physical record indexes the
F9418 table. The callback resolves the handle, reads the requested slice's
declared size, and compares9045C(data, size-2) with its final little-endian
16-bit checksum. On mismatch30EE4 branches to return0 without clearing F9720.
The queue retains the slice allocation and retries; it does not publish the
new portrait or free its previous cached data.

On match, **30EFC clears F9720 before any texture decode or visibility update**.
Only afterward does the callback detach prior graphics, free old cache data,
publish the new data, call8AE7C to decode its texture, rebuild primitives and
enable the photo/city. The clear has no state24 condition. Therefore a native
`ArchiveChecksumAccepted` event must precede image publication; a later PNG
decode failure must not undo that clear. `nba97_portrait_checksum_accept`
implements precisely this gate, not full30E78 callback acceptance.

## 3122C index lifetime API

Call `nba97_cool_index_load(&state, &transition.resource_handle, index_token,
archive_token, invoke, context)` using the same F84C8 field that the frozen
transition owner releases. State and that field must be distinct storage.
The function executes all3122C scalar decisions in source order:

1. Free an existing nonzero index data allocation, then clear F84C8. A null
   index or archive path returns here, retaining all other old fields.
2. Drain393F0, request2F8F4(index,0,400), then wait for accepted data in93708.
   Use3282C with graphics, otherwise38E84. Request must clear93708 itself.
3. Clear FDC00 and publish93708 into F84C8 before testing the old announcer
   voice. A nonnegative voice with status0 fades via7B2BC(voice,100,-1), waits
   for nonzero92BFC status, then becomesFFFFFFFF. An already-finished
   nonnegative voice keeps its old value: this source quirk is intentional.
4. Unload a nonnegative announcer program via91B28(bank_context, program),
   clear it toFFFFFFFF, free existing sample data, then store the BIG path.

The waits have no timeout. In the announcer-voice wait, graphics0 means a
busy loop without even an I/O pump. Do not call the blocking helper on the UI
thread with a callback that needs that same thread to progress. A native
resident adapter can validate/read the index before invoking it and publish
its accepted data token during REQUEST. Voice status must describe actual
announcer state, not the streamed music's FINISHED result or a dummy flag.
The source's100-unit announcer fade and hardware completion are not proven
equivalent to `RecoveredAudioPlayer::stop()` by this work.

393F0 drain deliberately retains pending whole-file jobs, whose callbacks can
complete. It cancels pending slice jobs and strips active callbacks; completed
work with no callback is freed. Review the existing resource queue audit in
`docs/team_select_text_history_workflow.md` and private
`.local/verification/gameplay/audit_b/team_text_pump/report.md` before adapting
cancellation. No instruction-conversion credit is claimed here for that queue.

## Host composition contract

The existing `PlayerPhotoLoader::Ready` validates only a180x156 PNG. It has
no raw IDX/BIG checksum evidence and cannot clear F9720. Its generation
discard is a native cancellation policy, not proof of original CD timing.

The concrete native integration should:

1. On a real transition into resource24, run31A88 begin once, retaining its
   live input bytes and the same F84C8 allocation token. This includes ordinary
   View Rosters PlayerCard entry and editor/Re-order child entry through
   `openReorderView`; it does not include every in-card player cycle.
2. Validate and retain both index allocations before resolving their records.
   Publish Cool Facts ownership through3122C; keep portrait F9418 ownership
   separate. Do not create a nonzero F84C8 token without the owned index data.
3. Request the original physical portrait record using the index's count:
   signed logical player below count uses logical+1, otherwise reserved0.
   For the current nonnegative player-ID domain, this preserves310D8. Negative
   malformed IDs can index before the source table; the native adapter must
   refuse them explicitly rather than claim source equivalence.
4. Complete raw slice I/O with immutable record/size/identity information.
   Discard a cancelled native request before applying its effects. For a
   current request, run the raw checksum gate on the UI thread with the live
   F9720 field, then perform image publication separately. If raw validation
   and PNG decoding share a worker result, carry the checksum result through
   PNG failure and apply it before `nba97_player_photo_complete` on the UI
   thread; never let a worker mutate music state.
5. Run31A88 end after resource dispatch. It preserves resource24's current
   block, including a clear already performed by an accepted callback. A
   first draw, transition animation end, missing PNG, or speech load must not
   provide an extra clear. Failed raw checksums remain blocked/retry or report
   an explicit native asset error; never fabricate successful completion.
6. On ordinary card exit, `returnReorderView`, and the Trade/Release/Sign
   child return path, compose the source transition back to the actual parent
   state before losing resource ownership. Cancel pending native portrait
   events and release each owned allocation once. The frozen transition owns
   F84C8 release and saved-volume restoration; the index loader must not free
   a second independently copied token for the same allocation.

`loadSelectedPlayerCardAssets(false)` is a portrait change, not repeated24
entry: re-entering would overwrite the saved volume with the already reduced
byte, as the original31A88 does. Keep the reduced21D7C input persistent rather
than reloading the configuration option every music update. Do not call an
extra gain setter after the source50-unit transition fade; that would cancel
its ramp. Restoring the saved byte on exit must retain the original behavior
even if the option was edited meanwhile. ED2AC denotes View Player here, not
a guessed gameplay Pause request or an automatic request for pause.cnk.

CD queue delay60/minimum music buffer400, retry scheduling, nested UI pumps,
allocator identity, texture/GPU lifetime and announcer envelope completion
remain platform or unintegrated source obligations. This content/lifetime
closure does not prove their wall-clock timing, full frontend dispatch, or
SPU/WinMM equivalence. No render-complete unblock is warranted.

## Evidence and one native extraction defect

Private proof: `.local/verification/native_completion/frontend_resource/`.
`oracle.py` builds the current C implementation and its standalone tests with
MSVC/O2/W4/WX, then compares it with actual FEONLY instructions. The receipt
contains107 CRC cases,72 installed-validator/whole-file callback cases,
30 portrait checksum-prefix cases and1200 complete3122C cases. The latter
cover all103 instructions and7368 callback events with state snapshots and
callback mutations. Total execution is2,933,854 MIPS instructions; this is a
test count, not new project conversion credit. Platform calls are explicit
hooks. Source FEONLY SHA256 is
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.

All494 physical Z1PORT records pass the recovered checksum. Actual310D8
execution maps logical492 to physical493 (28650bytes at offset13267728),
while logical493 falls back to0. The existing native extractor iterated
`range(count)` even though count493 excludes reserved record0, omitting
physical493. At the proof checkpoint `player_493.png` was the sole missing
PNG; host fallback by PNG existence therefore concealed an extraction defect.
This is a port bug, not an original game bug. A correction must include the
reserved entry in bounds/count iteration and preserve all existing outputs.
The raw final record's SHA256 is
`5c952cfd71ff949b54b57feabdb650e13c5e2b2afa9e46846053514410bc2412`.

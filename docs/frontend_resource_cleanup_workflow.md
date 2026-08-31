# Frontend resource cleanup and transition composition

`frontend_resource_cleanup.c` implements the complete scalar/call owners at
313C8 and2FB00. It supplies the missing cleanup between the two recovered
music-transition boundaries. It is not yet connected to the native host.
Allocation, queue progress, announcer lifetime, buffer waits and presentation
remain explicit callbacks, not invented successful operations.

Source addresses below omit the80000000 prefix. The private FEONLY.BIN SHA256
is `14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.

## Complete cleanup owners

313C8 first cancels resource identity20 through39308. A nonnegative announcer
voice whose92BFC status is zero receives7B2BC(voice,20,-1). The source then
polls92BFC until nonzero, without a UI pump, I/O pump or timeout, and writes
FFFFFFFF to the voice. A nonnegative already-finished voice keeps its old
value. A nonnegative program is unloaded through91B28(bank_context,program)
and becomesFFFFFFFF; existing speech sample data is freed and cleared.
The original return is1 only when the live voice needed the fade/wait, else0.
This owner does not clear FDC00 or free the Cool Facts index F84C8.

2FB00 performs the following in order:

1. Drain393F0; free and clear portrait index F9418 if nonzero.
2. Execute the complete313C8 owner above.
3. Free and clear Cool Facts index F84C8 if nonzero; clear FDC00.
4. Call38AE0(buffer_target,480,8003282C), then804E8(0).
5. For each of the two portrait caches, free its data only when both its
   graphic and data fields are nonzero. Clear both fields regardless; retain
   the physical record number.
6. Free and clear random-card allocation F7E7C if nonzero, then804E8(0) and28BF0.

The conditional portrait free is preserved and commented: nonzero data with
graphic0 loses its field without a free. Reachability of that inconsistent
state in retail execution is not established, so this is not a claim of a
confirmed retail leak. Adding an unconditional free would change the owner.
The retained finished voice and unbounded waits are likewise source behavior.
313C8's20-unit fade is distinct from3122C's100-unit fade; neither is replaced
with an immediate stop. F84C8 is cleaned on ordinary transitions too, not
only entry/exit of resource24.

F1478 graphics remains live during2FB00's buffer callback. Outer31A88 releases
it afterward. Freeing it early would prevent the original old-screen callback
from observing its graphics. Cached physical record IDs, loaded-data93708
and archive filename FB214 are not cleared by2FB00.

## API and callback ownership

`nba97_frontend_announcer_stop` uses the existing `Nba97CoolIndexLoad` state;
`nba97_frontend_resource_cleanup` additionally uses a
`Nba97FrontendResourceCleanup` and a pointer to the same F84C8 field used by
`Nba97MusicTransition` and `nba97_cool_index_load`. Do not copy the token into
independent owners. The structures and F84C8 field require disjoint storage.
Tokens represent native owned allocations, never truncated host pointers.
The source rereads live state after callbacks, including voice and subsequent
resource fields; adapters must not replace this with an entry snapshot.

| Callback | Required effect |
|---|---|
| DRAIN |393F0 queue boundary; not an unconditional deletion of every job |
| CANCEL |39308 cancellation of identity20 |
| VOICE_STATUS |92BFC for the supplied announcer generation/handle |
| FADE |7B2BC on the supplied announcer voice with20,-1 |
| UNLOAD_BANK |91B28(bank_context,program), using actual owned program state |
| FREE_DATA |7760C data allocation release, not the resource-handle free API |
| BUFFER_WAIT |38AE0(target,480,8003282C), preserving the old-screen boundary |
| SYNC |804E8(0) presentation/resource synchronization |
| HARDWARE_WAIT |28BF0 platform completion boundary |

Missing native arguments are refused before effects. The stop helper returns
-1 for such refusal; cleanup returns0 for refusal and1 for completion. The
cleanup success value is a native API convention, not a reconstruction of
the unused2FB00 return register. Callbacks must not recurse into this owner.

The original92BFC status is more specific than audio output readiness. When
signed byte D9BB5 is zero it returns-10. Otherwise it calls916CC and returns
that result's sign bit:0 for an active, matching handle,1 for invalid/stale.
916CC indexes `handle &31` in the68-byte F06B8 records and, under its lock,
requires active byte+13 to equal1 and stored word+0 to match the entire handle;
failure returns-8. These spans were inspected as instructions, not implemented
or given execution credit by this change.

`RecoveredAudioPlayer` currently has PCM/WinMM readiness but no exposed source
program/voice-generation owner or source fade lifecycle. Its `isPlaying()` is
not a proved92BFC adapter. Its reset/unprepare/close storage behavior also needs
separate driver-error ownership review before reuse for this lifecycle. A
callback that needs the blocked UI thread to advance would deadlock the
original busy wait. Native playback service must make real progress, or this
boundary must remain explicitly unintegrated; returning a dummy finished value
is not acceptable.

## Exact31A88 ordering: graphics first, selector initialization afterward

This audit refines the earlier music/resource workflow wording. In particular,
31F48 is graphics construction, not the outer View Player selector dispatch.
The music END boundary occurs **before**5A538 loads PORT/COOL indices. Do not
use the older phrase “after resource dispatch” to put new index ownership
between BEGIN and END.

The direct caller3F7C8 writes the old context resource+720 to+722, writes the
new resource to F13FC and context+720, then calls31A88 at3FA08. Within31A88:

1. 804E8(0) and51534 perform preceding synchronization/context cleanup. If
  current PSP filename DED24 is null, F1478 and F115C are cleared.
2. Run music BEGIN31ADC..31BF8 with the actual routing state, persistent
  `frontend_music_inputs_`, and persistent transition state.
3. Run2FB00(800). Then free F1478 and perform349C8 graphics/model cleanup.
  The ED270/nonzero-to-resource9 branch has additional model cleanup and
  clears ED270; it must not be synthesized from an unrelated native page.
4. Resolve the24-byte record at `FDBDC + resource*24` (retail table9378C).
  Reuse PSP F115C if the new filename matches DED24; otherwise free/load it.
  The graphic tag DED28 is updated even when the PSP is reused.
5. Load any random ZCARD allocation described below. Clear ED274 and ED244,
  then call38BD4 to resume/fill the music stream.
6. 31F48 constructs the new resource's graphics; music END31F10..31F20 then
  preserves F9720 for24 or clears it for other resources.31A88 returns.
7. Only now does3F7C8 dispatch through24F80. Resource24 reaches4072C and calls
  5A538. Its5A558 call initializes Z1PORT via30D14, followed by5A570 initializing
  Z1COOL via3122C. The selector then initializes its View Player context.

The source30E78 checksum callback may subsequently clear F9720 before publishing
the portrait. The native photo loader now carries that genuine raw-checksum
event through PNG failure and applies it on the UI thread only after generation
validation, to the same `frontend_music_inputs_.selection_blocked` used by the
router. That implemented event does not by itself implement BEGIN/END, the
resource cleanup, PORT/COOL initialization, or CD scheduling.

## Remaining resource and timing dependencies

38AE0 can present the old screen via3282C before its buffer/clock test;3282C
also pumps the resource queue and existing text/title state. The source waits
while the positive music-buffer level is below the target and its clock has
not exceeded start+480, then pauses the stream.38BD4 resumes, pumps until the
level leaves1..699 or its clock reaches start+60, then pumps twice more. Native
resident PCM may replace CD I/O, but not by claiming invented source buffer
values, fixed presentation counts or equal hardware timing. Prior ordinary
resource proof is documented in `team_select_text_history_workflow.md` and
private `team_select/audit_a/resource_text_history/contract.md`.

31A88 also consumes the shared16-bit frontend RNG for cards. Resource byte+23
selects a card count; it allocates count*4992 bytes, opens ZCARD.BIN, computes
total=file_bytes/4992 and chooses start with29B20 modulo(total-count+1).
It reads a contiguous group and validates each card's CRC over4980 bytes against
the32-bit word at4988. Failure repeats the random selection/read, consuming
another draw. The actual private file has95 records. Current native
`loadMenuCards` uses a separate wall-clock-seeded mt19937 and is an acknowledged
approximation, not this source owner. The existing Release-to-Rosters capture
variation in card art must not be treated as exact shared-RNG equivalence or
silently resolved by changing golden images.

Current `prepareCoolFact` reads its IDX per request and checks PATl/TMxl payload
shape, but does not provide the source whole-file CRC publication or a persistent
owned F84C8 token. A validated native COOL index owner remains necessary before
3122C or transition RELEASE can be connected honestly. The portrait archive
owner is separate and cannot stand in for it. The queue's393F0 semantics retain
pending whole-file work but cancel slice work/strip active callbacks; immediate
native cancellation is a platform policy and must not claim original CD timing.

## Host integration plan

An authorized next change can expose a narrow mutable transition method inside
`MusicPlayback`/`FrontendMusicPlayer`, where the actual routing, voice and
completion owners reside. BEGIN must return `completion_.finished` for FINISHED,
apply the recovered voice fade to the supplied live voice for FADE, and delegate
RELEASE to the real COOL allocation registry. END can use the existing C helper.
Do not use `isPlaying()`, phase10 or a Pause-song override for resource24.
Do not add an immediate gain setter after BEGIN: it would cancel the source
fade. Keep the original zero-adjacent-volume skip, repeated24 saved-volume
overwrite, and low-byte restoration even after an intervening option edit.

The host already retains `frontend_music_inputs_`; BEGIN, checksum completion
and routing updates must all use that object. Raw music21D7C should initialize
from settings and update on explicit option edits, not reload from durable
settings every frame once ducking is connected. Adjacent21D7D comes from the
speech option. Temporary ducking is not a request to save reduced preferences.

`beginFrontendTransition` rejects same-page transitions. View Player and Compare
children can enter/leave without changing `FrontendPage`, so compose resource
entry explicitly at successful child push/pop boundaries too. Relevant current
owners include `openReorderView`/`returnReorderView`, Trade child return, and
View Rosters activation/return. `loadSelectedPlayerCardAssets(false)` is a record
change inside24, not a repeated resource entry. Never trigger BEGIN per draw.

| Current host destination | Established source resource (hex) |
|---|---|
| Game Setup / Rules / Options |00 /01 /02 |
| Team Select / User Setup |03 /05 |
| Rosters / Card |09 /0B |
| Re-order / Trade / Sign / Release |0C /0D /0E /11 |
| View Rosters team roster / Users |10 /13 |
| Create Players |1F |
| Create child states |20 /21 /22, according to the actual child |
| Compare / View Player |23 /24 |

Profile Setup has no established matching source resource in this audit; do not
default it to00. Create's ED270 model guard can persist through child states and
is cleared on the actual return to9. Full composition therefore needs an owned
guard/model lifetime, not an unconditional “enter1F means guard0” rule.

Once the real resource/announcer adapters exist, the insertion order is BEGIN,
old-resource cleanup, actual resource/graphic construction, END, then selector
initialization and its requests. Native cancellation must precede applying stale
completion effects. Test accepted/rejected raw checksums, PNG failure, pending
request cancellation, repeated24 entry, exit restoration, zero speech option,
and active/already-finished announcer handles. A full frontend dispatch or exact
SPU/WinMM timing claim still requires the dependencies above.

## Verification and scope

Private receipt: `.local/verification/native_completion/frontend_resource_cleanup/`.
The current C implementation and public test compile with MSVC/O2/W4/WX; the
test uses active checks under Release builds. The original-instruction oracle
passes6000 cases (3000 standalone313C8 and3000 complete2FB00 including313C8),
matching44068 callback events and complete projected snapshots, including
callback mutations. It executes482042 MIPS instructions and covers all54
instructions of313C8 and all76 of2FB00. The original BIN, source extent hashes,
current DLL/public hashes, disassembly and build/test logs are recorded privately.

These are call/state ownership results, not new whole-frontend conversion
credit. Additional caller/status/resource observations above are instruction
inspection with explicit platform effects, not proof of their native execution.
The cleanup recovery itself changes no playback or host implementation. Root
integration links the C owner and standalone test through CMake; all84 CTests
pass in Debug and RelWithDebInfo. No actual31A88 host composition is claimed.

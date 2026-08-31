# Native five-resource frontend music playback

`frontend_music.cpp` now connects the recovered routing, voice service and
finite stream owners to WinMM. `music_playback.cpp` owns source state on the UI
thread; its output interface isolates worker/device lifetime. Original CNKs,
slot records, decoded samples and proof output remain private.

The adapter loads all five audited CNKs and the208-byte `music_slots.bin` from
the extracted menu directory. It checks each SHA256 before using the proven
fixed header/voice projection. It does not embed the slot table, deduplicate
it, remove ZTPAUSE from random selection, reseed a generator, or loop a CNK.
Resident decoded PCM occupies about135MiB, plus temporary decode/staging input.
The decoder algorithm has not changed; the previously documented FFmpeg
rounding difference remains, and exact original SPU waveform is unproven.

Host API

* `startFrontend(directory, raw_option_volume, source_clock)` validates and
  loads the bank, initializes the source router with menu1, and draws no RNG.
  The next two UI updates run source phase0/load and phase2/start respectively.
* `updateFrontend(source_clock, frontend_rng, inputs)` uses the caller's
  existing16-bit TITLE/Cool Fact state by reference and returns the number of
  actual ordinary selection draws. The host adds this to its existing draw
  counter. The six-word team generator is unrelated and untouched.
* `inputs.volume` is the raw option byte, while `setRecoveredVolume` accepts
  the already recovered0..127 gain and cancels the current source fade, as
  the gain owner does. Do not call it unconditionally every frame.
* The clock is a continuously advancing120Hz source-domain uint32 value.
  Native host policy uses elapsed `GetTickCount64()*120/1000`, truncated to
  uint32 after conversion. It must not use the wrapping32-bit menu millisecond
  accumulator. The adapter adds no second wall clock or catch-up cap.
* `requestSourceStop()` explicitly enters source phase10. It is not the
  resource-transition31A88 fade/phase4 owner and must not be used as a guessed
  View Player transition. `overrideResource(index)` supplies an explicit
  source override token, zero-based across the five resources.
* `currentResource`, `routingPhase`, `outputGeneration`, and `sourceFrameLimit`
  are diagnostics. `isPlaying()` describes native output activity and is never
  supplied as source FINISHED or BUSY.
* `start(path, recovered_volume)` remains compatible with the existing silent
  decoder smoke. It plays one finite source-eligible staging prefix without
  routing or RNG. Its native drain is not a source completion claim.

All public calls belong to the UI thread. Workers read immutable shared PCM,
an atomic gain and stop signal, and publish generation-tagged progress. They
never read or mutate recovered state, settings, UI state or either RNG.

The caller's ED2AC value selects resource/ring behavior only. View Player
resource0x24 is an identified writer; gameplay Pause is not established by
that fact. The complete31A88 transition, selection-block, saved volume and
restore owner remains separate work at this checkpoint. Host guard words and
selection blocking must be explicit inputs; normal native zero values do not
claim those original subsystems have been recovered.

Source decisions retained

The CNK plan passes real SCDl metadata and source spans through
`music_stream_fill`. It keeps full1024-byte/channel ADPCM blocks and refuses
the original partial tail. Each complete unit corresponds to1792 PCM frames.
The ordinary SCEl path closes the producer; no continuation is invented.

| Resource | Full staging units | Unsubmitted ADPCM tail bytes/channel | Native slot-entry frame limit |
|---|---:|---:|---:|
| ZTMENU1.CNK |4141|544|7418880|
| ZTMENU2.CNK |3972|960|7116032|
| ZTMENU3.CNK |3196|880|5725440|
| ZTMENU4.CNK |3232|816|5789952|
| ZTPAUSE.CNK |5196|800|9309440|

The plan simulates the recovered stop/advance predicate with resident available
data, an initial capacity-minus-one prefill and one subsequent refill per slot.
ED2AC selects200 or105 stereo slots. It therefore queues key-off at entry into
the last full slot, not at return of that slot's final sample. Native output
supplies an initial slot0 event and then slot-entry events after each completed
1792-frame prefix. A completed WinMM submission is progress; a queued submission
is not. The recovered source still advances in 256-frame quanta, while the
host WinMM queue uses four 1024-frame buffers. Keeping the source quantum and
host-device queue depth separate avoids audible starvation from ordinary
Windows scheduler jitter without changing decoded PCM or source cadence.

The source signed `read_index == write_index - 1` bug is retained. If final
write_index is0, PCM exhaustion does not set FINISHED or repair the comparison.
The resident adapter does not synthesize stale-SPU-ring waveform after such
exhaustion. None of the five current resources reaches this zero-wrap boundary.
Source signed absolute deadline comparisons, strict inequalities, zero fade
steps and the stopping/status mismatch are also retained.

`music_voice_timer` receives one call per120Hz tick, at rate120, with the
original inclusive service loop:120 callbacks initially execute101 services.
Voice ramps update every third service. The initial cold native service
counter is0; an original pre-menu audio timer history is not claimed. The
ordinary fade argument60 is not a half-second count of120Hz interrupts.

The successful enabled source stream has C6CAC flags7 and C6CAD pending0:
6A8F8 marks available data with4,6ACAC adds reservation2/audio1/valid-handle4.
6B6A0 returns3. Voice completion does not clear these bytes;28C28 through6B784
clears flags on detach, after which status is-14. No status is replaced with a
boolean. Private original-instruction probes cover allocation/status/detach,
including retained unrelated flag bits and nonzero pending bytes.

Native output boundary and storage safety

Key-off first enters source completion channel_state1. Only a later native
drained observation supplies substituted hardware status0 to
`music_voice_complete`; this can set source FINISHED. Old-generation progress
is ignored. Source retirement/deadlines request output reset and join the
worker before replacing its session. A reset-returned buffer is never counted
as naturally rendered media.

WinMM has no original SPU ADSR register, ADPCM prefetch or IRQ latency. The
adapter substitutes logical slot entry for the source slot+8 interrupt and
native drain for released ADSR level0. These are explicit platform policies,
not exact hardware equivalence. Gain applies to subsequently filled buffers;
up to4096 already queued frames, about92.9ms at44100Hz, retain prior gain,
plus any driver/device latency. Negative source stop-target gain is rendered
as native silence while the recovered stop request still waits for a slot.
Exact SPU mixing, release waveform and starvation waveform remain unverified.

All headers, sample buffers and the CALLBACK_EVENT handle live in one heap
owner. `MusicBufferRetirement` releases it only after successful reset, every
prepared-header unprepare, and device close. If any driver cleanup operation
fails, the whole outstanding owner remains allocated for process lifetime and
an error reports the retention. This deliberate exceptional leak prevents a
driver from accessing freed headers, sample memory or an event reused by a
later generation. Normal successful retirement closes and frees everything.
Driver API calls occur only on the worker; no wave calls run in a callback.
This follows the documented [reset](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutreset)
and [unprepare ownership requirements](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutunprepareheader).

Verification and limits

`tests/music_playback_tests.cpp` is standalone and Release-safe. It covers
source natural key-off before drain, premature native EOF, late old-generation
returns, exact shared RNG draws, full source status lifecycle, all-five plans
under both ring geometries,120Hz cadence, fade progression, uint32 clock wrap,
the zero-wrap source bug, and failed reset/unprepare/close ownership ordering.
An optional private asset directory checks all five real hashes, full decoded
counts, staging tails and routed finite plans. MSVC `/O2 /W4 /WX` passes.

Private receipts are in `.local/verification/native_completion/music_playback/`.
The real-device probe performed105 finite1792-frame sessions and105 queued
resets across all five resources with gain0. Process handle count remained
163 before and after the repeated retirements. It also exercised six composed
native generations: explicit overrides through all five resources, then one
ordinary shared-RNG selection. The caller RNG stayed29521 through overrides,
then changed to64293 with exactly one draw. No cleanup errors occurred.

These short device tests prove real output/control/lifetime behavior; they do
not claim listening validation, full-duration natural playback of every track,
exact waveform, or original hardware-time release. Original MIPS differential
proofs for the three recovered owners remain in their respective workflow
documents. No XA decoder, gameplay Pause owner, full CD task scheduler or
speculative playlist was added.

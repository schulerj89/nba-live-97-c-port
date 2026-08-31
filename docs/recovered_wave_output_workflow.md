# Recovered clip output ownership

This fixes a native port defect in `RecoveredAudioPlayer`; it does not change
the original game. The old same-rate replay and `stop()` paths ignored reset,
unprepare, and close results, then cleared a header and PCM vector that WinMM
could still own. The replacement uses a heap session and a separate heap clip
generation. Neither the header address nor PCM storage changes while borrowed.
The existing player delegates only construction, PCM submission, stop, and
native playing-state polling to this owner.

## Platform contract

The default `RecoveredWaveApi` calls WinMM with `CALLBACK_NULL`. It opens no
device at construction. Calls, polling, destruction, and retained-session
collection must be serialized on the host thread. This is the current player
usage; this class does not add synchronization for concurrent callers.

Each prepared generation owns an immutable native generation number, PCM
vector, and `WAVEHDR`. Before replacing it or closing its device:

1. If a submitted buffer has not returned, attempt reset. A nonzero result
   retains the entire session and stops retirement.
2. Even after successful reset, require `WHDR_DONE` before unprepare. A delayed
   return retains the session as `AwaitReturn` with a native
   `WAVERR_STILLPLAYING` pending result. This is not a driver-returned error.
3. Save observed progress before unprepare, since unprepare may change flags.
   Only actual successful unprepare permits freeing the header and PCM.
4. Only successful close invalidates the device handle. A close failure may
   retain just the device after the safely unprepared PCM has been freed.

These boundaries follow Microsoft's [polling and reset ownership
instructions](https://learn.microsoft.com/en-us/windows/win32/multimedia/managing-data-blocks-by-polling),
[unprepare contract](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutunprepareheader),
and [close contract](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutclose).
The [header flags](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-wavehdr)
are native driver observations, not original SPU voice status.

Same-rate replay retains the existing open device only after the previous
header was successfully unprepared. A format change requires successful
retirement and close before opening the next format. Prepare/write failures
also use checked retirement. A failed submission throws its original operation
and code; `failure()` separately reports a later cleanup failure if one occurs.
Playback validation and native generation overflow refuse before touching the
current clip. Every success comparison uses `MMSYSERR_NOERROR` explicitly.

`stop()` remains `noexcept`; failures produce a debugger diagnostic and leave
the outstanding storage/device owned. The destructor retries. If it still
cannot retire, it transfers the stable session to an intrusive retained list
without allocating. Each entry retains its API implementation as well as any
borrowed header/PCM. Successful later collection deletes the entry; failures
leave it alive. Subsequent serialized `play()`/`stop()` calls collect retained
sessions automatically, and `collectRetained()` is available for an explicit
retry. No owning static destructor frees failed entries at process shutdown.
A permanently broken driver can therefore cause intentional retention until
process exit, rather than a use-after-free.

`progress()` distinguishes submitted, returned, storage-released, and a reset
attempt. `natural` requires a returned buffer without any reset attempt for
that generation; after a failed reset it conservatively remains false even if
the buffer later returns. `isPlaying()` says only whether the current submitted
buffer lacks `WHDR_DONE`. It can be false while unprepare still refuses to
release storage. An old generation cannot become current again, and polling
an old returned header cannot finish a new clip. There are no audio callbacks
holding pointers into a destructed player.

## Original semantics and remaining source boundary

All existing public audio method bodies from `playCursorSound` through
`prepareCoolFact`, and all parser/decode/gain/pitch helper bodies, are unchanged.
The accepted-cue callback still runs once after validated PCM preparation and
before any native device failure. Muted/rejected cues and exports do not call
it. If retirement fails after acceptance, that accepted event is not undone;
the old clip's storage remains safe, and `info()` remains the previous result.
No RNG, original volume, waveform normalization, pitch interpolation, resource
selection, or saved game state is introduced or changed here. Existing source
quirks remain in their original recovered owners and comments.

Native generation IDs are not original announcer handles. The source handle
generation producer/allocator, SPU keyoff/release, and full frontend announcer
resource lifetime remain separate integration obligations described in
`voice_handles_workflow.md`, `voice_channels_workflow.md`, and
`frontend_resource_cleanup_workflow.md`. In particular, this native `isPlaying`
result must not be substituted for original `92BFC` status or used to erase
the original already-finished stale-handle behavior. This change does not claim
full `31A88` frontend transition composition or exact SPU completion timing.

## Verification and integration

Standalone MSVC 14.38 builds passed in Debug (`/Od`) and Release (`/O2`). No
actual device, UI, shared build, CMake, or host callsite was used or modified.

- `recovered_wave_output_tests.cpp`: 49 fake-driver checks, including nonzero
  reset failure, successful reset with delayed return, unprepare STILLPLAYING,
  other unprepare/close errors, open/prepare/write failures, same-rate reuse,
  format changes, stale generations, flags cleared by unprepare, and failed
  destruction with both explicit and automatic later reclamation.
- `recovered_audio_lifetime_tests.cpp`: 24 checks through the actual player,
  including byte-for-byte exported/submitted cursor PCM, speech gain, accepted
  ordering before failed retirement, muted/rejected isolation, metadata
  publication, and actual-player destructor retention. Both tests use only the
  injected `RecoveredWaveApi` and synthetic data.
- The unchanged `recovered_audio_test.cpp` passed against both the saved
  pre-change implementation and current implementation in both configurations:
  all 256 volume/speech inputs; 168 synthetic cursor level cases; 144 private
  cursor cue levels; 2,465 private speech mappings and six clip payloads; 24
  synthetic and 72 private speech gain vectors; malformed/rejected inputs.

Private commands/receipts are under
`.local/verification/native_completion/recovered_wave_output/`:
`build.py`, `audio_regression.py`, `unchanged_audio_semantics.json`, and the
per-configuration build/test logs. `freeze.json` identifies the final public
files and tested executables. The unchanged-method normalized-text SHA-256 is
`d8ab2bfda74f3d9c0f6608032ebcbabf36f435772b488ce77051e1940a5cee76`;
the unchanged PCM/helper text SHA-256 is
`da6e5803e6c7bb1f62bb485165a0b71e81306d85e66a9bc20207d876269713f3`.
These checks establish port behavior preservation, not a new original-MIPS
oracle for WinMM.

Root integration must add `src/recovered_wave_output.cpp` wherever
`src/recovered_audio.cpp` is linked. The helper test needs that new source and
Winmm; the actual-player lifetime test additionally needs
`src/recovered_audio.cpp`, `src/psx_adpcm.cpp`, and
`src/recovered/frontend_audio.c`, with the existing Windows definitions and
libraries. Both tests share `tests/recovered_wave_test_api.hpp`. No host API
change is required: the original default constructor and public methods remain
available. The extra `shared_ptr<RecoveredWaveApi>` constructor exists for
injected tests. Shared builds and real-device regression remain the root's
coordinated next step.

## Completed native integration

Root added the output owner to the Windows host and all targets that link
`recovered_audio.cpp`, plus both lifetime tests. Full Debug and RelWithDebInfo
builds each pass92/92 CTests. The code hashes remained unchanged throughout
both builds and match the worker freeze; this documentation note was appended
only after those checks.

A separate private real-device probe built with `/W4 /WX` passes in both Debug
and Release. Each run submits160 native generations of silent PCM at22050 and
44100 Hz, including4 natural returns and156 reset replacements. It checks
monotonic generations, rejection of stale generations, successful final
retirement, and an empty retained-session list. After warming both formats,
process handles remain191 before and191 after each run. No game asset, source
RNG, saved state, UI, or audible game cue is involved.

The device logs and probe are under
`.local/verification/native_completion/recovered_wave_device/`; full build/test
logs are `checkpoint11-*-build.log` and `checkpoint11-*-tests.log` in the parent
verification directory. The silent-buffer probe verifies actual WinMM
ownership and retirement. It does not verify source SPU envelopes, original
voice allocation, sound selection, or complete match audio timing.

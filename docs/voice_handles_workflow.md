# Announcer handle validation, fades and deferred service

`voice_handles.c` closes the original handle/lock wrappers needed by3122C,
313C8 and speech callers. It borrows the existing24 `Nba97MusicVoice` records
and `Nba97MusicVoiceClock`; it does not create another announcer clock or
replace source status with WinMM `isPlaying()`.

Source is FEONLY.BIN SHA256
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.
Addresses below omit the80000000 prefix.

| API | Original owner |
|---|---|
| `nba97_voice_handle_resolve` |916CC..91748, including nested lock/unlock |
| `nba97_voice_handle_status` |92BFC..92C34 plus the real resolve owner |
| `nba97_voice_handle_fade` |7B2BC..7B3B4 plus existing resolved fade arithmetic |
| `nba97_voice_handle_gain` |91748..9180C, including effective gain and APPLY |
| `nba97_voice_handle_stop` |92C34..92CB0 plus71A68 stop request |
| `nba97_voice_handle_lock/unlock` |7AD08..7ADF0, including pending7A6A8 service |
| `nba97_voice_stop_request` |Entire71A68..71B94 for physical voices0..23 |

## Shared state and original semantics

916CC increments C7408 lock depth, indexes F06B8 by `(handle &31)*68`, and
requires signed byte+13 to equal1 and the entire stored word+0 to match the
supplied handle. It saves either the physical index or-8, unlocks, then returns
that saved result. A high-bit-set handle is not rejected merely for its sign
by916CC. The service's separate negative-handle test remains unchanged.

92BFC returns-10 when D9BB5 is zero; every nonzero signed byte enables it.
Otherwise it returns the sign bit of916CC's result:0 for a match,1 for invalid.
This tests an actual generation and active record, not whether PCM was decoded,
a header was submitted, or an output device reports playing.

7AD48 decrements lock depth without checking for zero first. Only a resulting
zero depth drains pending C7404 callbacks: decrement pending, call the complete
existing7A6A8 timer owner, then reread pending. Underflow and wrapping counters
remain source behavior. No catch-up cap or artificial timeout is added.

Consequently a status query can report0 even when its own unlock drains a
pending service that clears the voice's active byte. The saved result is
returned; a subsequent query observes1. Fade/gain/stop hold an outer lock,
so the resolver's inner unlock ordinarily leaves service deferred until the
outer operation finishes. Do not collapse these nested locks.

Fade first checks enabled, then target validity, then locks/resolves. Targets
-1..127 are accepted; signed nonpositive duration becomes1. The existing
`nba97_music_voice_fade` preserves wrapping arithmetic and truncation that can
produce a zero step. Gain accepts0..127, cancels the ramp without changing its
target, computes the real effective gain and calls71600 before unlocking.
The original enabled=-10 and invalid=-8 return values are retained.

STOP does not clear active.71A68 skips the excluded C6D34 voice entirely.
Otherwise it sets C6D30, either sets C6D58 for the signed E45E4 tracked stream
or queues a bit in C6D3C for this physical voice, adding the paired bit when
the channel kind is at least2. The variable shift uses the low five bits of
the paired byte, as MIPS does. C6D30 is then cleared. These fields are shared
with the actual channel service, not private stop-complete flags.

`Nba97VoiceHandles` borrows retained clock, voice and stop state plus the
existing service callback. All accesses must be serialized; callbacks may
mutate live fields but may not free borrowed objects. The service callback
must perform actual hardware/service, optional, gain-map and APPLY effects.
For its STOP event, compose `nba97_voice_handle_stop` on this same context.
The caller must handle a native refusal or timer trap as incomplete, never
convert it into an original successful status.

The native result has separate `completion` and `value` fields. Only
`completion==COMPLETE` supplies an original return value. Invalid native
arguments refuse before effects. Slots24..31 would read beyond the24 owned
voice records in916CC; reaching one refuses with preceding lock increments
retained. It does not invent-8/finished1 or silently mask to24. This boundary
is not resumable. Source divide traps likewise retain their preceding state.
The unused7AD48 return register is not reconstructed; native value0 there is
an API convention, not an instruction claim.

## Generation production and remaining channel lifecycle

The actual allocator91338 advances its GP+178 generation counter by32 at
913AC..913C4 and resets it to0 if the wrapped result is negative. This occurs
before allocation selection can fail.9152C..91560 combines that generation
with the selected physical voice and writes both the returned handle and
the voice record. Secondary linked voices can receive handleFFFFFFFF.
These allocator instructions were inspected, not implemented by this change.
Do not initialize a different counter and call it the original allocation
algorithm, or omit its priority/steal/linked-voice decisions.

The downstream702B0 lifecycle inspected here is equally explicit:

* A key-on mask is sent to6F858 and affected channels become state2. State2
  becomes4 only when7BFA0 reports1.
* A state4 channel whose hardware status is no longer1 becomes state1 and
  queues keyoff; linked channels are also affected.
* Queued keyoff is sent to6F858(0,mask), then affected channels become state1.
  These mask/state changes do not themselves clear the voice's active byte.
* In state1, hardware status0 clears transient/state and calls916AC to clear
  active while retaining the handle. The tracked stream also receives its
  separate E45E7 completion update.

The existing `music_voice` completion helper covers the final selected arm.
Whole702B0, actual SPU register sampling, paired channel lifetime and the
allocator remain separate integration obligations. WinMM returned buffers can
support an explicitly native hardware adapter; they do not prove exact SPU
release timing or justify clearing source active on a stop request.

## Concrete31A88 and Cool Facts integration contract

3122C's100-unit fade and313C8's20-unit fade should call the new full fade
wrapper; their status callback should call the full status wrapper and accept
its value only on completed native execution. Keep the source callers'
already-finished nonnegative handles unchanged. Do not use `requestSourceStop`
phase10 or menu-music FINISHED as announcer status.

314A0 validates the raw Cool Facts slice checksum before stopping the previous
announcer through313C8. It publishes ECF8C and loads the real PATl program with
919A0, performs eight28BF0 calls, transfers ownership through7AFB0(handle,74),
and, when FDC00 is2, calls9180C at `min(21D7D*15,127)` and stores the returned
generation handle in DED08. It then clears FDC00. The current native
`prepareCoolFact` payload checks and PCM preparation do not implement this
checksum/program/resource path.

9180C validates a bank/program and dispatches92B74; it is not simply “assign
the next voice ID.” A real program/sample allocation owner must connect its
returned handle to the shared voice table before the new validation wrappers
are useful in actual playback. The native speech gain is already baked into
prepared PCM; a future fade adapter must not apply that setting twice.
Keep unscaled/source sample ownership distinct from device-format PCM.

Current host call sites stop speech in View Player entry/return, Trade child
return, Compare entry, Square, player cycling and Cool Fact replacement.
In `playSelectedCoolFact`, the native order is prepare, immediate stop, cue6,
start, flash. Square currently uses `isPlaying()` to decide stop feedback.
These are known native approximations. Actual source composition should
replace them through the proper313C8/314A0/31770 caller boundaries, keeping
the same live31A88 input state and separate F84C8 COOL allocation. A missing
variant must retain its established source branch; do not substitute a clip.

There must be one source timer/voice service for the borrowed table. The
original interrupt can advance a fade during313C8's busy wait. A native UI
thread cannot block waiting for a service that only runs on that same blocked
thread. The host needs an owned continuation or actual elapsed-clock service
while waiting, with source ordering documented; no dummy successful status or
iteration-count completion is acceptable.

## Confirmed native WinMM ownership defect and proposed adapter

`RecoveredAudioPlayer::stop` ignores reset, unprepare and close results, then
zeros the header and releases PCM. The same-rate branch of `playPcm` ignores
reset/unprepare results and reuses that same header/storage. Thus a failed
retirement can leave a driver referring to freed or overwritten memory. This
is a port defect; it is not an original-game behavior to preserve.

Microsoft requires driver completion and unpreparation before buffer release;
unprepare can return `WAVERR_STILLPLAYING`. Close can also fail while queued
buffers remain. These failure returns cannot be treated as retirement.
[Unprepare contract](https://learn.microsoft.com/en-us/previous-versions/ms713763%28v%3Dvs.85%29),
[close contract](https://learn.microsoft.com/en-us/previous-versions/dd743856%28v%3Dvs.85%29).

A concrete replacement should retain a stable heap `DeviceSession` containing
HWAVEOUT and stable heap `ClipGeneration` objects. Each clip owns its WAVEHDR,
PCM, native64-bit generation, source handle binding, and opened/prepared/queued/
reset-requested flags. Neither object nor PCM capacity can move while prepared
or submitted. Device-format reuse can retain the session after successful old
generation unpreparation, preserving the current cursor happy path.

On reset failure retain the entire old generation and session and report the
failure. After successful reset, require successful unprepare before retiring
header/PCM. On close failure retain the session and retry or quarantine it.
Destructor failure must retain storage for process lifetime, as the existing
music output already does, rather than destroying possibly live driver memory.
The process-lifetime retention must be diagnosed; normal retirement must not
leak. Use injectable driver calls to test these paths without opening a device.

Separate natural buffer return from reset-induced return. `WHDR_DONE` means
the driver returned the buffer; it does not identify why it returned.
[WAVEHDR contract](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-wavehdr).
Every event must identify both its immutable native generation and bound
source handle. An old generation may release its own storage but must not
clear a newly allocated voice or apply gain to its replacement. Only the
owned channel/service adapter may translate a current generation's actual
drain into source hardware status; worker threads never mutate frontend/music
state directly.

The next authorized adapter tests should inject reset failure, unprepare
failure, close failure, write/prepare failure, stale completion, rate reuse,
natural drain versus reset, and destruction with outstanding storage. This
checkpoint proposes that ownership change; it does not edit or replace the
existing audio/host implementation or claim a real-device failure test.

## Verification

Private build and source oracle:
`.local/verification/native_completion/voice_handles/`.
The C wrapper plus frozen `music_voice.c` compiles in Debug and Release with
MSVC/W4/WX; all26 Release-safe public checks pass in each configuration.
Each configuration's5900 original-instruction cases match2135
callback events and complete projected state over1067972 executed instructions.
They include real nested locks and pending original7A6A8/7A81C execution,
enabled/invalid/high-bit handles, hardware callback retirement, gain application,
stop masks, paired shifts and unlock underflow. All direct owner instructions
are covered except four unreachable fade division-trap check instructions after
the source clamps duration to a positive value. Explicit trap/unowned-argument
tests cover native refusal behavior without claiming original success.

Hardware service and APPLY remain controlled callback effects in the oracle.
The results prove these CPU owners and composition, not program allocation,
full31A88 integration, original I/O timing or waveform/SPU equivalence. No
existing host/audio/CMake files, shared builds, UI/device or Git state changed.

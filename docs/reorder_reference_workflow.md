# Re-order original-reference capture gate

Milestone acceptance (2026-08-28 UTC): the user accepted the current Re-order
presentation/audio after the scoped ledger reached875/875 with zero pending
instructions and native regressions passed. The Re-order goal is closed with
the exact-reference limits documented in `reorder_completion_plan.md`.
This document remains the optional stricter fidelity workflow; none of its14
paired comparisons is promoted to passed by user acceptance.

The 875-instruction owner ledger, native regression tests, and original-reference
comparisons answer different questions. None is a substitute for the others.
The current129 native compositor images are checkpoints, not continuous
recordings. They do not establish frame timing, sound onset, or original parity.

An additional original-only observation now retains ten debugger frame steps
from Chicago Re-order, with20 untouched desktop screenshots and a hashed
manifest. Two four-step title holds are verified by
`tools/inspect_title_frame_steps.py`; see `frontend_title_recovery.md`.
This is limited within-original timing evidence, not one of the14 paired
scenarios. Desktop geometry/occlusion, missing RNG state and missing audio remain
explicit; the main comparator's framebuffer policy has not been weakened.

## Fixed capture inventory

The machine-readable contract is
config/decomp/reorder_reference_scenarios.json. It defines 14 cases across the
five goal gates, including source anchors, initial conditions, action order,
completion markers and minimum sequence lengths.

| Cases | What to retain in the recording |
|---|---|
| help_first, help_replacement | Distinct modal bounds, grow/open/shrink/return, underlying selection and sounds |
| view_first, view_replacement, view_swapped | Draft portrait, player/team/layer changes, child Help, return to unchanged parent |
| compare_first, compare_replacement, compare_swapped | Both draft identities, active-side switch, browsing, Help, unchanged parent return |
| reorder_navigation | Title motion, palette change, six-row handoff, focus pulse and directional cues |
| compare_navigation | Top-Up on both sides, active-side team scan, layer changes, stat scrolling, extra Up after returning to top and return scan |
| cool_fact_play_stop | Same player/variant, cue6, speech, eight overlay phases, active and idle Square stop |
| no_cool_fact | Source-empty player62, red warning bounds/text/animation, dismissal and retained state |
| reset_cancel, reset_confirm | Non-default accepted roster, prompt, cancellation or restoration followed by Reset locking |

This is a minimum comparison suite for the current normal frontend, not a
claim to enumerate all game states. Season-mode gates, special/free-agent View
eligibility and original RNG/cache behavior remain separately open.
Native file persistence/recovery is a port-specific gate: do not compare its
file bytes to a PS1 memory card or require memory-card emulation.

## Capture without changing the evidence

1. Use the locally owned disc and an isolated test session. Preserve existing
   saves/memory cards and original assets. Do not Reset a user's live roster
   merely to prepare a reference.
2. Use a clean normal frontend baseline. Record the actual working 535-slot
   table hash (little-endian u16 IDs, empty=ffff), not a pristine-file hash
   substituted for runtime state. Record team, both cursor/top pairs, phase,
   draft changes and relevant child/player/layer/variant state.
3. Record original overlay and derived catalogue hashes, game music/speech/SFX
   settings, emulator version, capture tool, session ID and date.
   For audio isolation, use music0 with speech9/SFX9 in the test session.
   Retain any settings needed to restore the user's setup.
4. Begin before the first declared action. Capture every logical frame through
   the final completed transition. Keep lossless 512x240 RGB PNG/PPM frames
   with no scaling, smoothing, warping, dropping, interpolation or duplicated
   stills. Preserve raw recordings if conversion is necessary and document
   that conversion; do not invent missing frames.
5. Retain PCM16 mono/stereo audio across the entire sequence, its sample rate,
   and the measured sample corresponding to video frame0. Use comparable
   capture paths for original/native; raw native decoder exports are not the
   same as original mixed-output recordings. No gain normalization, pitch
   correction, channel conversion, latency search or automatic time alignment.
6. Record each logical input's frame and each completed event's frame/state.
   Retain the original raw debugger trace and native CLI trace beside the
   annotations. A typed address or matching sidecar alone does not prove
   original execution.
7. Replay the same setup and input timeline in the native host. The checkpoint
   command is not continuous evidence. The passive recorder below now retains
   live presentations, input timestamps and optional process-mixed audio.
   Deterministic replay, source provenance and a matched original pair remain required.

The no$psx automation has not reliably registered game-key taps; original
capture/navigation still needs assistance or a verified alternative. The
bindings were inspected, not changed. Do not force RAM state to skip the game
flow and call the resulting recording an end-to-end reference.

### Observed no$psx export geometry (unresolved capture boundary)

A built-in Utility > Screenshot export from the live Atlanta Re-order screen
is retained unchanged at
`.local/verification/reorder/original-input-audit-20260827/atlanta-reorder.bmp`.
It is RGB **512x224**,344118 bytes, SHA-256
`47243a29a93182d4b29e7ab1cdb841eb9b4e389057f78fe3474d0c0fb7394fab`.
It was exported while paused, not reconstructed from assets or resized from
the desktop. It is a single observation, not a continuous scenario recording.

Read-only no$psx I/O Map > GPU inspection at the same paused state showed:

| Register/view | Observed value |
| --- | --- |
| GP1(05) display address | `040000`:x0,y256 |
| GP1(06) X range | `C60260`:608..3168,width512x5 clocks |
| GP1(07) Y range | `040010`:16..256,height240 |
| GP1(08) display mode | `000002`,described by no$psx as512x224 NTSC15bpp |
| GPUSTAT | `5604240F` |
| GPU command viewer | `FillVram512x240`; two framebuffer areas visible |

These are transcribed observations from the tool screenshots, not a saved
instruction trace or a verified hardware scanout model. In particular, a
240-line render buffer/range is not proof that a screenshot export has240 rows.
The current reference schema's512x240 requirement cannot directly accept this
512x224 export. Keep it failing closed. Establish the actual display-window
mapping before selecting a common capture representation; do not stretch,
pad, guess a crop, or search offsets to manufacture equality. This issue is
separate from native controller correctness and from the missing continuous
audio/video recordings. The14 paired cases remain unverified.

Input follow-up: with the viewport explicitly focused, V still did not open
Compare. Temporarily disabling the Controls > Joystick Enable checkbox also
did not make V open it; the checkbox was restored to enabled and applied.
Keyboard mappings were unchanged. The original was resumed with F9 afterward,
on Atlanta Re-order with both cursors on Mutombo. No roster mutation, game RAM
patch, memory-card change, rendering-setting change or external publication
was performed. A manual Compare opening is still needed for child recordings.

2026-08-27 follow-up: the existing no$psx2.3 session resumed through its Run
menu from PC801E11B0 and progressed normally through loading/intro to the title.
Automated Return at the visible "press start" prompt did not advance it; Cross
and Return during the intro also did not visibly skip it. This is a navigation
blocker, not evidence of a game/controller defect. Manual help reaching Re-order
is still needed before collecting the original scenario. No RAM state, bindings,
memory cards or emulator settings were changed. Its current INI reports no screen
filter, Bright video and "Quads (better than real)" rendering; preserve and declare
these reference conditions rather than calling its pixels hardware-exact.

Later follow-up: the original is now on Atlanta Re-order, first selection,
Mutombo in both lists. A fresh Controls Setup inspection confirms player1
Triangle=F, Cross=C, Square=D, Circle=V, Start=Enter, Select=Right Shift,
L1=A, L2=Z, R1=S and R2=X. Thus F is not a guessed binding. The original Help
panel was still closed at inspection; the cause of missed input remains unknown.
Setup was closed without edits and execution resumed via Run (not Reset & Run),
leaving the original game foreground. No paired reference credit is earned by
this inspection or by its scaled desktop screenshot. Wait for visible Help or
resolve input delivery before collecting the continuous reference sequence.

## Private capture manifests

Restart/input retest (2026-08-27): no$psx was reopened and the recent original
disc loaded normally. The game reached the title, then Game Setup, then an
explicitly labelled `demo mode: press any button` match. That sequence does not
prove an injected Start was accepted. Down/F did not produce a visible menu or
Help response; a further Return at the demo prompt did not exit demo mode after
a three-second observation interval. Controls were inspected again (arrows,
F=Triangle, Enter=Start), left unchanged, and execution resumed. Do not count
autonomous attract-mode transitions as successful navigation or reference input.
The original is running; manual navigation to Re-order/Help remains necessary.
No paired capture was added. The same retest passed17 Windows suites and all28
isolated save-host scenarios, which remain native evidence only.

### Passive native video and process-audio collection (partial evidence)

First original Help still retained (2026-08-27): after manual navigation to
Chicago Re-order, the first-stage green Help panel became visible. Its untouched
no$psx Utility > Screenshot export is
`.local/verification/reorder/original-help-chicago-20260827/help-first.bmp`
(512x224,344118 bytes, SHA-256
`a894bdfcf9cbeb08ac9dff06a6ef996c09681c079e2b4923cb335e7f321d9e5c`).
The neighboring private observation.json records provenance and limitations.
Visual inspection against screen/help-first-open.png confirms the compact modal,
heading and seven control rows with the same icon/text arrangement. It does not
establish pixel equality: the original is brighter/banded, while native uses an
unquantized interpolation. Recomp800305B4..80030680 specifies corner RGB10/20/10
and0/150/0, matching the native input colors; no$psx's current Bright setting is
a separate capture condition, not a proven complete explanation of the difference.
The512x224 versus512x240 boundary remains unresolved; no crop/stretch was applied.
This Chicago still is not the Atlanta continuous help_first scenario, has no
audio or runtime-slot hash, and adds no paired-reference credit. An unintended
debugger assembler dialog was cancelled without submitting code before this
observation. Subsequent automated Continue input remains unreliable; do not
infer a complete Help round trip from the open panel alone.

First original Compare still retained (2026-08-27): manual Circle/V opened
Compare with Chicago/Longley on both sides. Untouched export:
`.local/verification/reorder/original-compare-chicago-20260827/compare-first.bmp`
(512x224, SHA-256
`2d40a8a196280c80a5f0586a3e6753b8515578ce0ff0f9f4a26c1efa82e66885`).
Inspection against screen/compare-first-entered.png confirms the same identities,
number13/starting C, five labels/values (62,62,564,9.0,1641), portraits,
left-side horizontal arrows, lower scroll marker and bottom team labels.
Title phase, brightness/rasterization, exact geometry, scroll timing and audio
are not verified by these stills. Original Help had also visibly returned to
the unchanged Chicago/Longley list endpoints, but no continuous recording or
runtime-slot hash proves the full round trip. All14 paired cases remain open.

One-row original Compare endpoint is also retained beside that image as
`compare-down-one.bmp` (SHA-256
`d47a29bb1e57a9d2bee4591dce26c5fbb0ffeab4d3e84e3acb84b905697deb1d`).
Both columns now start with games started and end with minutes per game;
values are62,564,9.0,1641,26.4 on both sides. Up appears, Down remains, and
both identities/team labels plus the left-side horizontal arrows are retained.
This supports the settled shared-scroll result tested by roster_compare_test;
it does not prove intermediate old/old -> new/old -> new/new presentations,
arrow-flash timing, cue selection or event-frame counts. The paired gate stays open.

The subsequent `compare-right-active.bmp` export (SHA-256
`a41642d7715144e00419b17b47f5afa0c0378e016b529bc2c57569457875d477`)
shows Longley on both sides, the same one-row scroll endpoint and horizontal
arrows beneath the right identity. This agrees with the side-toggle callback
at80059A88/80059A90 (context+15 XOR1, then80039BA8) and the native controller.
An earlier live snapshot showed Edwards on the left, but the user corrected
their input before export; that intermediate state is NOT in this saved image.
The private observation records that distinction. This is endpoint evidence,
not a continuous input trace or timing/audio match; paired credit remains0/14.

`compare-return-first.bmp` (SHA-256
`7f43a4d0ec5c8666db9849e8af1f5b0b421bfd1360f1d690ff6e00abe56d555c`)
then shows the original first-stage Re-order screen again: Chicago, both
Longley portraits/selections, left highlight, HELP1 and the same six visible
rows on each side. This supports first-stage parent-state restoration, not a
full-table no-mutation claim: no original before/after runtime hashes were
captured. The native child test separately checks both stages after browsing
both identities/teams, scrolling and changing layer, comparing the whole parent
state (except expected input barrier fields) and the live slot table. That
test passed again; the still does not establish return timing/audio parity.

The original replacement-stage Help still is retained separately at
`.local/verification/reorder/original-help-chicago-20260827/help-replacement.bmp`
(SHA-256 `2b304e9aed35ccea90594763a29322e8ff83a9ae5dcc9ceadb4dd20bc71deba0`).
Its five action rows and button order agree with the native
`help-replacement-open.png` checkpoint, including Select/cancel swap, and both
use a shorter modal than HELP1. The original has Longley selected on both sides;
the native checkpoint has Parish as the replacement. Brightness/banding and
512x224 versus512x240 remain explicit differences, not normalized away.
The paused debugger displayed a2/r6=800B102C, consistent with the second Help
descriptor; this transcription is not a retained execution trace. Growth,
shrink and audio remain unmeasured in the original. A later read-only UI
observation showed the modal dismissed with HELP2, both Longley selections and
the six visible rows retained; this supports the settled return only, not
full-table identity or transition timing.

Original replacement View entry is retained at
`.local/verification/reorder/original-view-chicago-20260827/view-replacement-rodman.bmp`
(SHA-256 `541453fa354e472a195f56e938e1c287977900a5f5d7c6c61cf1cddf07639b0a`).
After Down then Square from HELP2, it shows Dennis Rodman, number91, starting PF,
his action photo/Chicago strip, six season rows and the cool-facts button.
This verifies the visible replacement identity, not an entire round trip.
An earlier live snapshot showed the identity/stats already drawn while a
"please wait" graphic occupied the photograph area; the photo and city strip
appeared later. The native host currently loads that PNG synchronously; the
renderer uses `shot` if absent and draws the city strip unconditionally. The
`wait` sprite is loaded but not drawn by this path. Recover the original loading
lifecycle and completion order before claiming this entry animation matches;
do not fake a CD delay from two snapshots. See the private observation for
scope; neither the waiting interval nor its audio was continuously captured.

After returning from Rodman's View, a live observation retained HELP2,
Longley left/Rodman right and the same six rows. Replacement Compare then
opened with those two identities and LEFT active, as the constructor requires.
Its retained bitmap is
`.local/verification/reorder/original-compare-chicago-20260827/compare-replacement-longley-rodman.bmp`
(SHA-256 `956c09c6428a35350cf1c49d47cb5aa3c2a92e17190326f5a7693626a9e9a05a`).
The five season rows start at games played. Values are62/62/564/9.0/1641 left
and64/57/351/5.4/2088 right. This is stronger visible identity evidence than
the earlier self-comparison, but still not paired animation/audio evidence.
Following Right then Enter, a live transition snapshot showed Rodman on both
Compare portraits; the settled parent restored Longley left/Rodman right and
HELP2 with the same six rows. This supports the source's non-adoption behavior
for Re-order (distinct from Trade's guarded adoption path). These later UI
observations are documented privately but are not saved built-in bitmaps or
continuous recordings; full-table identity and timing/audio remain unverified.
The subsequent Select cancellation returned to HELP1 with Longley active left,
Rodman retained right and unchanged visible rows. This closes the observed
replacement selection/Help/View/Compare/cancel path at the endpoint level only.

Read-only layout extraction following the View loading observation found
graphics layout36 at80097A24: record16 (`atlZ`, replaced with the team strip)
starts disabled at(x296,y35), depth7; record17 `shot` starts enabled at(297,35),
depth3; record18 `wait` starts enabled at(334,35), depth13. The original asset
already exists in the local ZSET8 pack. Further recomp inspection recovered
310D8's same-record guard and photo-only disable, followed by30E78's checksum
gate and photo/city enable at31084..310B0. The city is not disabled on every
subsequent request. `wait` is not toggled off: the completed photo covers it.

The native path now implements that visibility subset in recovered C plus a
bounded asynchronous local-PNG worker. Latest request wins; stale completions
are discarded after browsing/exit/reentry. Decode failures leave the wait
image, log the problem and do not prevent navigation. This queue policy and
PNG validation are port adaptations, not recovered PS1 CD/checksum timing.
The interactive path does not sleep or enforce a minimum waiting duration.
Offline captures explicitly await completion for settled frames and retain
separate pre-completion images. CLI `PLAYER-PHOTO` events report requested
physical record, city visibility, success/failure and stale result disposal.

`nba97_player_photo_tests` tests the C visibility branches and bounded-worker
guards without copyrighted assets. `verify_reorder_screen.py` additionally
checks six loading frames against the local `wait` sprite and source layout,
retained city-strip pixels on later requests, and unchanged pixels outside
the photo/city region on completion. The ZSET8 loader's existing RGB-zero
transparency mask is explicit in those checks. None of these tests establishes
original loading duration, original continuous motion/audio parity or full
feature completion. Owner accounting remains separate and unchanged.

A separate `--capture-view-rosters` checkpoint now renders Rodman (id38,
physical portrait39) with the same six initial values as the saved original.
Both untouched512x240 native frames are in the private
`view-rosters-photo-20260827` directory with lossless PNG previews; provenance
is appended to the original View observation JSON. Visual inspection agrees
on the player photograph, Chicago strip and stat content. The512x224 original
export, brightness and unsynchronized title phase remain visibly different.
This is a same-identity compositor comparison, not a completed Re-order
end-to-end reference scenario. The original bitmap was not changed.

Launch the usual executable with an additional argument:

~~~powershell
./build-windows/Debug/nba97_boot_decomp.exe --record-native-frames .local/verification/my-fresh-recording --record-native-audio --record-native-limit 3600
~~~

Navigate normally to the frontend screen; F9 starts/stops recording. No game
state is forced. Default limit is600 presentations; --record-native-limit accepts
1..6000. The example allows3600 (about90 seconds at the observed presentation
cadence, not an assumed60fps). Audio has a separate120-second hard bound: stop
with F9 before that bound, or the audio capture is incomplete. Leaving the frontend
also stops capture. Choose a
fresh private directory for every run; existing recordings are never replaced.
Keep the CLI trace with the recording. Use isolated --roster-save, --settings
and --profiles paths when testing mutations; the recorder does not sandbox
normal gameplay writes by itself.

The worker writes original-size RGB PPMs, frames.csv (monotonic nanosecond
timestamps and editor state) and inputs.csv (raw Windows input and next-frame
boundary). It has eight queued framebuffer slots plus one in-flight packet,
roughly5MB of frame buffers regardless of the configured cap;600 frames occupy
about211MiB on disk (3600 about1.24GiB). Disk/queue
failures mark the session incomplete and retain a reason in error.txt. The
host clock and draw/update flow are not replaced. Capture overhead and stalls
remain in the timestamps. Repeated pixels are preserved, not deduplicated.

~~~powershell
python tools/inspect_native_frames.py .local/verification/my-fresh-recording
# Optional stricter parent-Help diagnostic (not an original comparison):
python tools/inspect_native_frames.py .local/verification/my-fresh-recording --require-help-roundtrip
~~~

recording.json is deliberately **not** a reference manifest. It records the
actual audio requested/captured/completed status, reference_ready=false, frame_rate=null and
scanout_verified=false: this records the framebuffer passed through the
native paint path, not proof of monitor scanout or a PS1 logical frame clock.
The editor-state columns are diagnostic snapshots, not a complete replay
state (child browsing details, all535 runtime slots, asset hashes and settings
must still be retained for a comparable pair). Do not relabel these variable
presentation times as a constant frame rate or silently resample them into the
reference contract. A paired variable-time comparison/replay path remains open.

With --record-native-audio, the Windows process-loopback API includes only the
current process tree, not the microphone or a whole-system fallback. Unsupported
activation fails visibly. Windows converts the mixed output to PCM16 stereo48kHz;
this is not the raw PS1 SPU signal or a decoder WAV export. The audio directory
contains mixed.wav, packets.csv (unaltered QPC timestamps/device positions/flags)
and its own recording.json. Video and audio share a QPC origin. Startup waits for
the first real audio packet; stop drains packets to cover the requested boundary
without synthesizing silence. Audio is bounded to120 seconds/about23MB plus metadata.

The inspector reports packet gaps/overlaps, video-boundary coverage, peak/RMS and
samples above one PCM least-significant bit. Nonzero samples alone are NOT proof
of an audible cue: the Windows path produced +/-1-level noise while all cues were
muted in a real failed capture. Preserve that noise; do not normalize it away.
Our virtual process device returns all-zero device positions. This is reported
as unavailable, with sample_continuity_verified=false even when QPC intervals are
contiguous. A flat WAV concatenates received packets; use packets.csv to retain
wall-time gaps, not just a sample-count-derived duration. Packet coverage does
not independently prove audiovisual synchronization or actual monitor scanout.

### Original emulator-only audio capture

`nba97_capture_reference_audio` now uses that same Windows process-mix pipeline
with an explicitly selected no$psx PID and expected executable. The normal port
constructor remains current-process-only. The diagnostic verifies the executable
against the live process, retains a query/synchronize-only process handle and
records its creation timestamp. Target exit fails the capture; there is no device,
microphone or system-wide fallback. It does not send game input, adjust volume or
change emulator settings. Output must be a fresh directory below `.local` and
duration is bounded to1–110 seconds, inside the recorder's120-second limit.

Example (replace the PID after checking the running process; never reuse an old
PID blindly):

```powershell
./build-windows/Debug/nba97_capture_reference_audio.exe 39736 'F:/Games/PS1/no$psx/NO$PSX.EXE' '.local/verification/reorder/my-fresh-original-audio' 20
python tools/inspect_process_audio.py .local/verification/reorder/my-fresh-original-audio --selected-pid 39736
```

The inspector requires the expected PID explicitly; it will not reinterpret a
selected-process recording as a native/current-process capture from its sidecar.
The recorded identity is provenance, not cryptographic authentication of the
emulator's behavior. Keep the executable hash and session/game observations too.

2026-08-28 UTC checkpoint: original Chicago Re-order idle produced240960 stereo
frames across502 packets, peak21680, RMS3615.36 and478671 samples above1LSB.
There were no reported discontinuities or timestamp errors. Raw WAV SHA256:
`eedfa1fa07097ddaabe46360d5e51708e4a7150af9708f90da132df886db3433`.
Files remain under `.local/verification/reorder/original-audio-idle-20260828`;
source/executable hashes and inspection results are retained in
`.local/reports/reference_audio_checkpoint_20260828.json`.

A separate live isolation diagnostic launched a known660Hz child while440Hz
and880Hz played in the recorder process. Selected-child capture measured1199.36
at660Hz, while excluded tones remained below0.002 PCM amplitude in both channels.
This checks selected-process scoping, not original sound fidelity. The native
current-process path has separate live tests. Device-position fields remain all
zero, so sample continuity is still explicitly unverified even with regular QPC.

The original idle recording contains no annotated Help/navigation input and no
matched video. Do not label it a verified cue or count it as any of the14 paired
scenarios. Automated `f` still did not open Help; no bindings were changed. Next
is a timed original cue capture with observed input and a matching native run,
retaining music/SFX settings and raw gain/pitch/onset differences.

The parent-Help diagnostic requires observed phases closed/grow/wait/ready/shrink/
return-barrier/closed, matching keydown boundaries, and unchanged recorded parent
fields. It does not verify all535 slots, geometry, sound identity or child state.
Some nonvisual states can enter and leave between two paints; the diagnostic
then remains unverified. Do not force an extra rendered barrier frame merely to
pass it. New recordings also retain a separate timestamped semantic event trace
for those states; it does not replace or synthesize video frames.

### Help call-boundary evidence

New passive captures write `help_events.csv` (schema1, at most10000 events).
It records the baseline and changed open/input/tick calls: observation QPC,
next submitted-frame index, input mask/result, before/after modal geometry,
parent diagnostics and SHA-256 of the effective535-slot draft (little-endian
16-bit IDs). Multiple transitions may share a timestamp or frame boundary;
their serial indices preserve call order. CLI `RECORD-HELP` messages summarize
phase changes without printing every animation tick.

~~~powershell
python tools/inspect_native_frames.py .local/verification/your-capture --require-help-events
~~~

This separate gate checks controller arithmetic, state chains, input boundaries,
rendered phases and unchanged parent fields/slot hashes across an observed
parent-Help cycle. It permits the controller's wait-to-shrink transition within
one call, without inventing an intermediate event. The existing
`--require-help-roundtrip` remains the stricter video-only all-phase diagnostic.
Neither flag verifies original execution, sound identity, screen pixels, all
mutations between events, or child round trips. Event times are observations
after native calls, not original PSX instruction/tick times. Legacy captures
have no semantic-event credit; incomplete recordings still fail inspection.

Explicit live diagnostics (not run by CI; audible tones, private paths only):

~~~powershell
./build-windows/Debug/nba97_process_audio_capture_tests.exe --live .local/verification/fresh-tone-test
python tools/inspect_process_audio.py .local/verification/fresh-tone-test --expect-test-tones
./build-windows/Debug/nba97_process_audio_capture_tests.exe --music-isolation .local/verification/fresh-music-test .local/assetpacks/menu/ZTMENU1.CNK
~~~

The isolation diagnostic records two low-level tones before/during/after the
production music player's mute, with event QPCs in music-isolation.csv. A complete
recording is not an isolation pass: inspect the tone levels across all phases.
The --live foreign-process rejection check additionally needs a concurrently
running --foreign-tone instance, with its successful run retained separately.

API rationale: [process-loopback scope](https://learn.microsoft.com/en-us/windows/win32/api/audioclientactivationparams/ns-audioclientactivationparams-audioclient_process_loopback_params),
[packet timestamp/flag semantics](https://learn.microsoft.com/en-us/windows/win32/api/audioclient/nf-audioclient-iaudiocaptureclient-getbuffer),
and [shared audio sessions](https://learn.microsoft.com/en-us/windows/win32/coreaudio/audio-sessions).

### Paired comparison schema

Keep each pair at:

~~~text
.local/verification/reorder/reference/<case>/original.json
.local/verification/reorder/reference/<case>/native.json
~~~

Each manifest has the following fields. Artifact paths are relative to .local/,
not the manifest's directory; every referenced file has a SHA-256.

| Field | Required meaning |
|---|---|
| schema_version, kind, scenario | 1, original/native, exact case ID |
| contract_sha256 | Canonical contract digest in the local report; stale contracts rejected |
| provenance | Nonempty tool, version, session, captured_at, notes; retain capture/normalization details |
| continuous_frames | true only for a genuine continuous capture |
| asset_sha256 | overlay and roster_catalogue hashes |
| initial_state | Contract conditions plus actual slot_table_sha256 and relevant runtime state; must match between sides |
| settings | Integer music, speech, sfx in0..9; must match |
| frame_rate | Exact rational [numerator, denominator]; do not assume native17ms equals the original |
| inputs | Ordered {frame, action}; contract names, frame0-based, distinct frames; pair timelines must match |
| events | Ordered {frame, name, state}; one completion per input, contract names and nonempty state; required integer fields/ranges when specified; differences reported |
| frames | Contiguous frame0..N-1 records, each {path, sha256}; unique paths, at least two distinct decoded images |
| audio, raw_trace | Each {path, sha256} |
| audio_frame_zero_sample | Measured audio sample-frame offset corresponding to video frame0 |

For example, a frame path might be
verification/reorder/reference/help_first/original/00000.png. Never guess hashes,
timestamps, frame markers, initial states or trace annotations.
Duplicate JSON keys, non-finite values, stale contracts, wrong scenarios,
missing/truncated files, hash mismatches, path escapes and incomplete media
are rejected. Different initial state/assets/settings/inputs are not comparable.

Compare navigation starts on normal layer2, active left, with both column tops0.
Each event must retain `active_side`, `stat_layer`, `stat_top_left` and
`stat_top_right`. For original traces, the normalized tops are manager+6/+7
minus the respective measured minima at+8/+9; retain the raw bytes and their
addresses in the debugger evidence. Do not replace both values with the native
shared top. Signed ranges intentionally retain any observed underflow as a
difference instead of silently clamping it. These annotations remain subject to
independent trace review; valid types do not authenticate execution.

The first two top-Up checks occur before and after switching sides; the third
follows a Down/Up round trip.5A1EC clears the first-row Up callback in both
groups, so3D930 bypasses callback, sound and post-delay at those boundaries.
Retain the absence of dispatch and the next input-poll frame in the trace; an
unchanged screenshot alone cannot prove that distinction. This contract update
changes its digest, so old manifests must be recaptured/reviewed against the
new actions, not merely assigned the new hash. The inventory remains14 cases.

## Run and interpret

~~~powershell
python -m unittest discover -s tools -p test_reorder_reference.py
python tools/verify_reorder_reference.py --check-config
python tools/verify_reorder_reference.py

# Native/database tests plus the separate reference requirement:
./scripts/verify_reorder_rosters.ps1 -SkipBuild -RequireDatabase -RequireReferences
~~~

The report stays at .local/reports/reorder_reference_run.json. CI runs only
synthetic comparator tests and public contract validation; it cannot certify
original media deliberately absent from GitHub.

- Exit0: every supplied pair has equal decoded frames, clock, audio and
  state/event timing. This is supplied-media equality, not authenticated
  original execution or overall game fidelity.
- Exit1: comparable supplied recordings have differences.
- Exit2: evidence is missing/invalid or recordings are not comparable. This is
  an intentionally incomplete reference gate, not a Windows Debug crash.

The report includes missing/invalid/not-comparable/different/equal counts,
changed frame numbers/pixel counts, maximum and mean channel error, clock
agreement, event-frame/state differences, audio format/sample count, maximum
sample error and RMS values. It never blends these into a completion percentage.
Audio alignment uses only the declared frame0 origin; onset discrepancies
remain visible.

A reviewer must independently verify provenance, trace annotations and capture
continuity. Hashes detect changed files, not fabricated provenance. Synthetic
tests produce matching invented media and assert that the report still says
authenticity requires review and overall fidelity is not inferred.
Do not lower tolerances or edit reference frames to turn a difference green.
Investigate differences, preserve raw evidence, and document explicitly accepted
limitations separately before auditing the overall goal.

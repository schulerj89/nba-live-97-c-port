# Re-order child-screen verification

This tracks new feature evidence separately from the existing875-instruction
Re-order owner ledger. No new instruction totals are claimed until the complete
owner/callback inventory and explicit block mappings exist.

## Recovered routes

| Original anchor | Recovered contract | Current native boundary |
|---|---|---|
| `80040FCC` | Help table at `800B00E0`; state12 selects active row descriptor | Help indices0/1 extracted from the original pointers |
| `80040A1C` | Style-zero no-choice Help; font/control text, sounds7/8; input barriers | Specialized C lifecycle + private descriptor renderer; other modal styles outside this slice |
| `80030430`, `80030784`, `800309DC` | Initial rectangle, green G4 colors, independent grow/shrink clamps | Geometry recovered; native interpolation, not PSX raster equality |
| `8003B194` | Wait until raw input differs from saved input | Opener/closer barriers; no timeout substitute |
| `8002C6B0` | Inline byte `1F` advances by the following unsigned operand | Original glyphs + encoded spacing; not the full original text engine |
| `8003F7C8` dispatch, `80056254` exit | Re-order result2 pushes state24; result3 pushes state23; parent UI context | View and Compare host dispatch/return wired and tested |
| `8005A074`, `8005A538` | Parent-aware selected identity; View uses Z1PORT/Z1COOL | Direct View entry with a validated draft projection; no fake standalone roster construction |
| `8005A3FC`, `8005A6F0` | Only Trade parent13 may adopt a browsed child selection | Re-order parent12 retains both cursors/tops/IDs and the complete draft |
| `8005A880` | Compare has its own state23, Z2PORT portraits, five visible rows | Own native screen, private portraits/text pack and five-row C controller |

The shared selector's Help descriptors are copied locally without embedding
their copyrighted strings in the public source. Four records use a bounded,
versioned712-byte pack. Synthetic tests contain invented strings only.

## Current gates

The separate 14-case continuous-media contract and fail-closed comparator are
documented in [the reference workflow](reorder_reference_workflow.md). Existing
checkpoint stills and the two older View trace scenarios are not substituted
for these Re-order recording pairs.

| Gate | Native evidence | Original reference evidence |
|---|---|---|
| Help first selection stage | Pass: private descriptor, exact panel bounds, held-input tests, frozen frame return | Source geometry/text/colors found; emulator frame-sequence comparison pending |
| Help replacement stage | Pass: distinct smaller panel, selection/scroll retained | Same limitation |
| Dirty multi-team draft across Help | Pass: two edits, both stages, full parent-state equality in C test | Live original transaction trace pending |
| Help sound choice | Pass: host dispatches7 on open,8 on close using local ZCURSOR | Recorded original waveform/gain/timing comparison pending |
| View round trip | Pass: both stages, swapped draft, empty rejection, wrap/team/layer browsing, return and subsequent Accept/Discard | Source return gate found; original transition/frame/audio comparison pending |
| Compare round trip | Own host screen; both phases and post-swap captures; draft portraits, active side, independent palettes, Help/return, layers/free agents, source sound IDs | Original frame-sequence/transition/audio comparison pending |
| Persistent accept/Reset | Compact versioned saves, host Accept/reload/error recovery and normal-roster Reset wired; 28 isolated host scenarios | Reset source gate/descriptor/audio IDs recovered; matched original frame/audio sequences pending |

## Reproduce

Windows with private assets:

```powershell
./scripts/build.ps1
./scripts/verify_reorder_rosters.ps1 -SkipBuild -RequireDatabase
python tools/verify_reorder_screen.py
```

Asset-free CI/Linux:

```sh
python -m unittest discover -s tools -p test_frontend_help.py
cmake -S . -B build-core -DNBA97_RECOVERED_TESTS_ONLY=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

`nba97_frontend_help_tests` prints nine named asset-free checks; passing the
private asset root adds one real-descriptor/font/control-glyph check. They are
not silently counted as original execution comparisons. The extraction suite
adds five synthetic tests for routes, bounds, alignment, termination and inline
control validation. Missing assets fail local-required tests, not fall back to
invented content.

`verify_reorder_screen.py` verifies the extracted Help bytes against original
pointer-selected descriptors, runs all ten native Help checks, twenty-six child
checks, eight Compare controller checks and four Compare pack checks. It captures65
real compositor frames: four parent states, eight parent Help frames, twelve
View-child frames, eighteen Compare entry/browse/Help/return frames and four
extra Compare layer/free-agent frames, seven no-facts notice frames, four speech
cycle/return frames and eight Cool Fact overlay phases.
It checks the initial20x10 rectangle, full panel bounds, five-tick shrink bounds,
no changed pixels outside the panel, exact frozen-parent return, sound dispatch,
and unchanged database asset. It records source/asset/executable hashes under
`.local/reports/reorder_screen_run.json`, with original visual/audio comparison
explicitly `not_verified`. Frames remain private in
`.local/verification/reorder/screen/`.

The child suite contains seventeen asset-free checks (including twelve using an
invented catalogue loaded through the real public database format), plus nine
with the local original catalogue. `draftView` must share the exact player vector
and player pointers with its source while keeping all team order/derived indexes
isolated. Tests cover catalogue reload lifetime, stale/tampered rejection, and
multi-team Accept/Discard after returning. The C return context is16 bytes; this
is not the total allocated cost of the child view or renderer.

View capture routes cover first-stage, replacement-stage and post-swap entry.
Each route checks the parent-selected identity/slot, actual Z1PORT record pixels
outside the team-logo strip, child Help's `(130,60,250,140)` rectangle, and exact
frozen-parent return. This is regression evidence of native composition and
draft isolation, not an original-game screenshot comparison. Compare is now
available through S; its independent captures use the same real host handlers.

The deterministic capture freezes parent animation to isolate modal effects.
The interactive host continues parent tint/UI updates during Help, but retains
the suspended parent during View. Child entry/return currently switches native
composition directly; original transition/tint/repeat timing remains to compare.
Consequently these stills do **not** prove original motion, real-time duration,
held-key repeat or audio waveform matching. The combined editor/save workflow
has a separate native host verifier (`tools/verify_reorder_save_host.py`).
Inherited View details still require the reference pass: the unavailable-cool-fact
notice is currently CLI-only, and the two-tick player transition needs
original-matched verification/implementation. Navigation sound dispatch is now
wired as described below; recorded waveform/timing comparison remains pending.
Next reference work must capture aligned input/frame/audio sequences with their
initial state and timing parameters, as described in the completion plan.

### View team-scan correction

Recomp `80059ABC`/fragment `80059ACC`, backed by the Ghidra export, retains
slots0..14, resets an index above14 to0, and walks backward over signed `-1`
slots on the new team. `80059808` refreshes the selected identity without
resetting the stat-list scroll. Native View previously reset both indices.
The corrected normal-team path preserves them and atomically rejects an empty
target list rather than reproducing the original unchecked underflow.

The suite now checks338 synthetic scan cases (13 destination lengths,13 source
slots,both directions), every occupied slot on all29 teams in both directions
for both synthetic and private catalogues, and an empty-target guard. These
are grouped scenarios, not338 newly recovered instructions. CLI team-scan
events report before/after team, slot and stat-top.

View entry `8005A538` now binds the actual layout`0x24` to the stat-change
controller. The old native default`0x23` incorrectly reported a second Compare
column refresh. New tests require one six-row group and extent-dependent scroll
reset. Free-agent/special-team scanning in View remains **pending** (Compare
already covers normal free-agent scanning). This correction does not establish
animation/audio parity or increase the existing875-instruction owner ledger.

The `8003D930` selector, not `80059610`'s text-label redraw, dispatches View's
left/right sounds2/1, team/layer sound6, and Start/Cancel return9/10. The host
now follows those routes. Up/down sounds3/4 and the gold stat flash only occur
on an actual successful vertical input, not an incidental scroll reset during
a layer change. Endpoint inputs remain silent. The46-frame verifier exercises
all four directions, forward/reverse team/layer inputs, changed/same descriptor
extents and both Start/Cancel returns; it checks exact sound-ID sequences and retained
stat tops. It also emits lossless PNG copies of the raw PPM frames for inspection.
None of these dispatch tests establishes original recorded waveform equality;
the live SFX setting is now wired and tested separately below. That46-frame
checkpoint is now extended to53 frames by the no-facts notice scenario.

### Original no-Cool-Facts warning

`80059E14` tests the selected variant at context+32 for signed -1. The absence
branch calls `80040A1C(800AFE06,-1,0)`. Its descriptor specifies red style1,
rectangle `(136,90,240,64)`, two centered body lines and no choices. The shared
dialog code adds the continuation prompt from `8002502C`: this is not stored
inside the player-specific descriptor. Body baselines are105/117 and the
prompt baseline is135 (warning start+15,12-pixel rows,6-pixel footer gap).
The opening sound is5, closing8; no speech clip or selection sound6 is requested
on this branch. Original input-change barriers and the shared growth/shrink
steps yield13 ticks each for this geometry.

`extract_player_notice.py` creates a92-byte **private**
`player/no-facts.n97ui` containing those original records. No game text or
screenshots are embedded in source. The host uses the existing ZFONT1 pack,
red modal renderer and C modal lifecycle in both normal and Re-order View paths.
Keyboard/hover/click input cannot reach the underlying screen while it is open.
Missing or truncated speech indexes now report an asset error, not a false
claim that the selected player has no Cool Facts.

`player_notice_core` tests bounded parsing, all14 controller dismiss masks,
held-input barriers,13-tick growth/shrink, initial20x10 red geometry and the
five-record-per-player availability lookup. Its optional private check verifies
all three original text rows against ZFONT1 at the source coordinates. The
extractor has synthetic-only CI tests too.

The53-frame host check finds a real absent-fact player from the original IDX
(current disc: Reggie Geary, ID62), enters View through the actual Re-order key
handler, opens/dismisses the notice, then returns to the editor. It independently
checks all five original index entries are empty, verifies exact changed-pixel
modal bounds and sounds5/8, and compares returned player/editor frames byte-for-
byte with their pre-modal/pre-child frames. The parent draft is also checked
unchanged in memory. Private proof: `.local/verification/reorder/screen/no-facts-*.png`;
report: `.local/reports/reorder_screen_run.json` (source/asset/binary hashes).

This is original-data/source evidence plus native regression, **not** an original
emulator frame comparison. Available-fact selection scheduling, eight-tick icon
flash and original recorded speech/mixer comparison remain separate open work.

### Cool Fact record-zero correction

Recomp `800315BC` computes `player*5+variant`, compares it with the IDX count,
then **adds one** before the eight-byte table lookup. Physical record0 is a
fallback, not player0's first variant. `8003122C` stores the unshifted loaded
IDX pointer; `800314A0` uses the resulting physical record for archive loading.
The earlier native decoder and availability check both missed this addition.
That shifted variants and could borrow a clip from the preceding player.

`CoolFactIndexView` now supplies the same checked lookup to both paths, including
the reserved entry in its file-size bound. Logical0/variant0 maps to physical1;
player0 variant4 maps to5, and player1 variant0 maps to6. A synthetic player-
boundary regression failed before the fix and passes after it. The old decoder
self-test still decodes the same physical1 payload, now correctly named variant0
instead of variant1. Reggie Geary's corrected five-record group is also empty,
so the no-facts host scenario remains valid.

The optional private audio tests check all2,465 logical index mappings against
an independently transcribed address formula. Six decoded/exported payloads
cover every variant of player0 and player1's first variant. The screen harness
retains these six WAVs locally as `cool-fact-p0-v0.wav` through `p0-v4` plus
`p1-v0`, with archive offsets, lengths, record IDs and WAV hashes in its report.
Every exported sample is checked against the decoder fed from the separately
selected original archive region. These checks establish record selection and
native PCM extraction, not an independent ADPCM decoder or original mixed-
audio oracle. The selection/overlay checkpoint below closes the native scheduling
predicates and feedback lifecycle; shared original RNG state, speech gain/
envelopes and recorded timing remain open. No instruction-ownership credit is
added by this correction.

### Cool Fact selection and overlay checkpoint

Recomp `800593F0` refreshes five availability flags, excludes the previous
selection when more than one clip exists, and draws `RNG & 7` until the
candidate is below the available **count**. It does not test that candidate's
availability flag during refresh. `80059D18` instead picks only flags equal
to1, refilling through593F0 when all have been consumed. A single available
variant can repeat. The private index has one non-prefix group: player320,
mask19 (slots0,1,4). Initial refresh can select its absent slot2; the port
preserves this source behavior instead of silently substituting a different clip.

The nine-byte C selection context accepts explicit random draws. The host uses
its own seeded stream: original shared `8007A538` RNG state/consumption is
**not matched**. Fresh native contexts initialize the prior selection to-1;
the original entry's initial stack value is not established. Decoder inspection
and WAV export now require explicit variants and cannot advance gameplay state.

Recomp `80059EBC` toggles object21 eight times before clearing the chosen flag.
The graphics object stride is108;2268/108=21. Original state36 table
`80097A24` gives object20=`o18a`, enabled1/depth3, and object21=`o18b`,
enabled0/depth2; both are at356,198. The base plaque stays visible, with the
gold overlay alternating on/off/on/off/on/off/on/off. It is not a highlight
lasting for the whole recording. The shared one-byte flash context defers
consumption until the eighth completed presentation and blocks child actions
while the callback is in flight. The Win32 host advances at most one phase
after a paint and a new17ms logical tick, never skipping unseen phases during
a stall. Exact original `39574` cadence/scanout alignment remains unverified.

Five asset-free checks cover32 masks, six previous selections,256 supplied
draw values,243 flag combinations, repeated depletion/refill, single/sparse
groups and all eight overlay/consumption phases. CI and the local verification
script explicitly run this target. The private host runs two five-clip cycles,
tests13 blocked key routes at every phase, verifies physical archive records,
and checks full parent-state preservation. Eight captured phase images are
compared pixel-for-pixel against the resting native frame plus the extracted
`o18b` sprite using the existing black-key transparency rule. Off phases
must match the resting frame exactly. These are native construction checks,
not screenshots from the original game.

The following checkpoint covers native preparation/SFX/start ordering and
Square-stop feedback. Original RNG parity, asynchronous disc/cache behavior,
mixed gain/envelopes and same-scenario recorded onset/duration remain open.
In particular, the sparse missing-variant branch's original cached-bank playback
effect has not been established; native selection predicates alone do not prove
that audio edge case. Source ownership totals remain unchanged.

### Speech gain and start/stop checkpoint

Both `800314A0`'s deferred completion and `80031770/80031778`'s immediate
start read byte`80021D7D` and pass `min(setting*15,127)` to9180C. This is
Options **speech volume** (index2), independent of SF/X volume(index3,*12).
The local verifier checks the original load/shift/subtract/clamp instruction
words at31808/3180C/31820/31824/31828, not just a comment or native helper.
All1,185 populated original Cool Fact records have one tone,127/127
program/tone gain, root60 and zero fine pitch. Optional private tests verify
that header baseline, in addition to all2,465 logical index mappings.

Preparation now returns a single owned decoded clip without opening an audio
device or changing current playback metadata. The host prepares, releases the
previous voice, requests cue6, starts speech at the current speech volume, and
begins the eight-present feedback callback. PCM ownership moves into playback;
the catalogue and whole speech archive are not copied or cached. This preserves
the native call order, **not** the original asynchronous loading latency.
Raw WAV exports remain unscaled; a separately named playback export applies
program/tone/setting gain and authored pitch. Original SPU envelope/mixer
rounding and hardware output are not claimed to be reproduced.

Unlike SFX's zero-volume early return, the original speech start still calls
the voice player at gain0. Native speech therefore submits silent PCM with the
same duration and normal stop/selection/flash lifecycle. Square maps through
`59DB8(1) -> 313C8`: cue5 only when a voice was stopped; an idle stop is silent.
Host `isPlaying()` supplies the native active-voice predicate; sample-exact
completion-boundary equivalence with the original remains unverified.

The Windows audio suite now prints ten checks with private assets (seven
asset-free). Additions cover256 speech settings, stop-feedback combinations,
24 signed synthetic gain vectors (including non-full program/tone gain),
every truncation of a synthetic clip, invalid tone/gain fields, preparation
isolation, and72 private clip/volume vectors. Every exported PCM sample is
checked against an independent integer gain formula, but the ADPCM decoder
is shared: this is not an independent decode oracle.

The65-frame host gate uses speech settings9/8/4/0/9 twice; it verifies
prepare/cue/start/flash ordering, both silent starts, ten active stops with
cue5, ten idle stops without a cue, and retained parent/child state. Settings
are restored in memory and never saved by the capture harness. CLI records
decoded bytes, physical record, speech setting/gain, start, stop and flash
completion events. No source-accounting percentage is increased by these tests.

### Live SFX gain and mute checkpoint

Recomp `8002F124/8002F12C` reads byte `80021D7E` on each call, returns early
when zero, otherwise passes `min(setting*12,127)` to `8009180C`. Ghidra's
`feonly_sfx_trace.txt` agrees. Both host cursor routes now pass Options SF/X
volume (option3), rather than a hardcoded9. A muted request skips bank reads
and WinMM submission; it neither stops nor replaces an existing cue. Visual
stat flashes are independent of that audio suppression. CLI logs include the
setting, effective gain and `submitted`/`suppressed` outcome.

`nba97_recovered_audio_tests` is a Windows, asset-free CTest target. It checks
all256 byte inputs to the scale/clamp helper, an invented bipolar ADPCM fixture
at12 levels, gain-before-pitch ordering, unchanged authored pitch/duration,
default9 export compatibility, and mute without any assets/audio device. With
`.local/assetpacks/menu` as its argument, it also checks all12 populated cursor
cues at12 levels (144 cue/level vectors). Every raw exported signed sample is
checked against the program/tone/playback integer gain calculation; every
pitched sample is checked against that gained signal's interpolation. UI
levels are0..9;10/11 additionally exercise the arithmetic clamp boundary.

These tests share the native ADPCM decoder and validate the **native gain and
resampling contract**, not an independent original SPU decode/mixer oracle.
The46-frame host verifier additionally tests live8->96 volume, muted card and
stat cues, retained audio metadata and a visual flash while muted. It restores
in-memory settings without saving them. Its local report records the audio
test binary, source and original bank hashes. `verify_reorder_rosters.ps1`
runs the asset-free and (when local packs exist) private vectors as well.

Source instruction ownership remains unchanged. Original recorded waveform,
SPU envelope/mixing, device gain and onset comparisons remain **not verified**.

### Compare controller and corrected layer interpretation

`src/recovered/roster_compare.c` models the normal Re-order child in12 bytes;
this excludes the borrowed535-slot draft, shared catalogue and renderer memory.
It does not emulate a MIPS context or duplicate player records. The eight
asset-free controller scenarios cover both parent phases, independent identities,
Cross active-side switching, five-row synchronized scrolling, layer wrapping,
extent-dependent scroll reset, team-slot retention/backward clamp, conditional
free-agent scanning,100-slot free-agent wrapping, stale inputs and no draft writes.
The full-cycle test visits every slot on both sides of29 synthetic teams.
Two additional child scenarios use the synthetic and local catalogues to verify
actual draft identities and exact parent-state restoration after Compare browsing.

Source anchors: `8005A074` (entry), `8005A1EC` (normal layer2/limit3, team limit29),
`80059F20`/recomp fragment `80059F30` (input routing), `80059A88` (Cross toggles
active side), `80059928` (player wrapping), `80059ABC` (team scanning),
`8003AB64` (scroll both groups), and `80059610`/`800594F0` (layer/extent change).
Special-season team eligibility, special free-agent cycle locks, Trade writeback,
presentation/timing and audio are **not** implemented by this normal-context API.

Compare recovery exposed an earlier incorrect annotation: `8003B26C(0x1B)`
redraws the layer-label text object, not a sound slot. Code, CLI and source
accounting now name that object correctly. The original also resets scroll only
when the descriptor extent changes; native View now preserves scroll for
layer2↔3, with forward/reverse and changed-extent regressions. This adds no
instruction credit and proves no audio parity.

Implemented Compare graphics evidence: graphics state35
table`800978C4`, title`ba02` at(170,15), Z2PORT87x51 photographs at(54/386,22),
ZSET4 frames/plates, separate team-paletted background halves, centered labels
atx256 and left/right values atx128/384. Stat rows starty135 with14-pixel pitch.
The layer-label object's table index10 resolves through`8009AF40` to`8009D34C`;
the stat descriptor families are`800A56D0`/`800A5648`/`800A54C8`.
The968-byte private pack is extracted by`tools/extract_compare_assets.py` and
verified byte-for-byte against those source descriptors before capture. It is
bounded/versioned and contains no player data; both players share the catalogue.
Five synthetic extraction tests and four runtime pack checks cover malformed
inputs, supported fields, layer/row bounds and private-pack loading.

`3D434`'s selector pairs start at118/135 and874/891, y116;`39BA8` shifts the
shared primitives +/-500 so only the active pair is visible. The verifier checks
Cross changes only those original glyph footprints. Original palette update
`2FF80` indexes team29 directly into ZTMPAL record29 (`xeaP`), while name helper
`4ECA8` separately remaps29->31. Captures verify the free-agent palette and two
independent team halves rather than assuming the name/palette indices match.

Window input: S opens Compare; C/Space changes active side; arrows browse players
or scroll both stat lists; J/K scan teams; Q/E change stat layer; F opens Help;
Enter/X/Esc return. No cool-fact archive or speech controls belong to Compare.
Source-selected audio IDs are dispatched and logged, including endpoint sound
suppression. No original waveform comparison is inferred from those logs.

Remaining reference work includes entry/exit composition timing, the original
two-tick player change, original-reference palette crossfade phases (native
indexed fading is now implemented; see the checkpoint below), stat-row flash/scroll motion, held-key repeat and matching
recorded sound onset/pitch/gain. These are tracked as open fidelity work, not
hidden behind the passing still-image/controller tests.

### Sanitizer environment note

One default-PIE ASan rerun on local WSL/GCC11.4 hung in a repeated fatal-signal
loop. Debugger evidence showed dynamic-loader symbol resolution for
`pthread_self`, not a normal test assertion; the underlying cause is not proven.
The failed run is not counted as passing. A separate test-only non-PIE build,
still instrumented with both AddressSanitizer and UndefinedBehaviorSanitizer,
passed all three suites ten consecutive times with a20-second per-test timeout.
Shipping build flags are unchanged. Local diagnostic notes are retained in
`.local/reports/reorder_sanitizer_runtime_note.txt`; the1.76GB repetitive failed
test log was removed, not source/assets/saves.

After adding the Compare controller, the same non-PIE ASan+UBSan build passed
all four suites ten times each (40 runs), again with a20-second per-test timeout.
The normal Windows build and Linux four-suite run also passed. This is memory/
undefined-behavior regression evidence, not original-game fidelity evidence.

Latest Compare UI/pack checkpoint: Windows build/application self-test and all
five Linux suites passed. The non-PIE ASan+UBSan build passed those five suites
three consecutive times each (15 runs). The46-frame private compositor verifier
passed independently; no source instruction credit was added by these tests.

### Original Atlanta still: portrait transparency correction (2026-08-27)

The user prepared the original Atlanta Re-order screen. Its window capture is
preserved locally at
`.local/verification/reorder-original-ready-20260827/original-parent-window.png`.
This is a scaled emulator-window still, not a raw logical-frame sequence or an
audio reference. It exposed patterned plates behind both portraits that were
covered by opaque black pixels in the native decoded photographs.

The existing source-derived compositor already draws `110p`/`111p` before the
Z2PORT portraits and frame. Record 1 (Mutombo) supplies palette index 255 at the
top-left texel; its original 16-bit CLUT value is `0x0000`. The generic decoder
had emitted opaque black there. CLUT index 0 instead contains `0x8000`, so neither
an index-zero rule nor RGB-black color key would be correct.

`tools/decode_reorder_portraits.py` now restores alpha from the original palette
word after RGB decoding and before removing the padded column: zero is
transparent; nonzero remains visible for the opaque portrait draw. All 493 local
derived PNGs were regenerated, yielding 1,161,225 transparent logical texels.
Original archives are read-only inputs. The decoder logs the count and rule.
Three asset-free tests cover all 65,536 palette words, nonzero transparent
indices, opaque black, RGB/input preservation, row padding and invalid extents;
they run in CI and the Re-order verification script.

All 65 native screen checkpoints passed after regeneration, and visual
inspection of `entry.png` shows both patterned portrait backings. That checkpoint
uses Chicago, not the Atlanta original still: it is construction evidence, not
a same-team image-parity result. Existing compositor checks alone did not catch
the old fully opaque decode; the new palette regression is a separate check.
The earlier live Help recordings retain their old portraits and are not rewritten.

Still unresolved from the original baseline: both list-scroll indicators versus
only the active native column, the Help1 footer, and matched entry/Help motion
and audio. No original-reference scenario or instruction credit is added by
this correction. The live application's cached portraits need a reload before
the regenerated files become visible there.

### Both-list scroll markers (2026-08-27 follow-up)

The Atlanta original still shows a down arrow on both six-row lists, not just
the focused one. Recomp `8003DD38` iterates the list pages, sets the active-page
byte before calling `8003A224` at `8003DD5C`, then restores it at `8003DD84`.
The marker refresh routine deletes/recreates only objects `page` and `page+2`;
the other page's objects survive. Ghidra's `compare_label_layout.c` independently
exposes the same refresh loop. Rendering only the current page erased that
persistent-object distinction in the native compositor.

`nba97_reorder_screen_markers` now supplies the four composed marker poses:
original ZFONT0 glyphs `8B`/`8C`, x46/256, y116/196 with per-object offsets.
Each list uses its own top index; up is visible above top0 and down below top9.
Changing focus alone does not hide either pair. This is a steady-state renderer
adapter, not emulation of original object allocation or transition timing.

Regression coverage: 200 combinations of both scroll tops and active page
(800 marker checks), distinct offsets on all four objects, and null input.
Three actual compositor frames additionally compare all 12 glyph footprints
against the loaded original font, including expected absence. Entry has two
markers; replacement-scrolled and swapped have three. CLI `REORDER` logs both
pairs' visibility, and `REORDER-MARKER-VERIFY` logs the pixel checks.

Rebuilt successfully; Windows CTest15/15, Linux CTest13/13, all65 native screen
checkpoints and the full private Re-order/save regression script passed.
`entry.png` visually shows both down arrows and both patterned portrait plates.
Instruction accounting remains875/875; no new credit is inferred from these
presentation tests. The live original remained at Atlanta; the automated F
press still did not open Help. Matched original Help motion/audio, Help1 footer
and the other reference scenarios remain open. No emulator settings, original
assets or original save data were changed for this check.

### Source-selected Help1/Help2 graphics (2026-08-27 follow-up)

The missing number is part of an original sprite, not native text or a
controller-number indicator. Recomp `8003D5F0` gates on controller byte+0x13,
reads the selected row's descriptor byte+9, and replaces graphics object4 via
`8003186C`, indexing the four-byte tags at `8009B230`. The first two tags are
`hel1` and `hel2`; `8003D65C` performs the corresponding context-change update.
A fresh read-only Ghidra export confirms both functions and the replacement
helper in `.local/ghidra/reorder_help_footer.c`.

Graphics state12 object4 is `help` at(235,217), enabled, depth1. The source
Help table at`800B0F5C` has descriptors`800B0F68`/`800B102C`, so the first and
replacement contexts use the original ZSET4 `hel1.png`/`hel2.png`. All three
sprites (`help`, `hel1`, `hel2`) have the same68x10 extent. The native selector
uses the same descriptor state as the Help modal; it does not append a numeral,
recreate the font or change footer geometry. Missing numbered assets fail
explicitly. CLI records the source path and selected footer on state changes.

View state36 and Compare state35 both have a null second Help pointer. They
correctly retain the unnumbered graphic; the change is scoped to Re-order.

Tests cover first/replacement/return selection and all254 invalid byte indices.
The private verifier checks the original tag table, graphics record, numbered
eligibility for all three screens, and loaded-sprite opaque pixels in11 parent
frames spanning entry/replacement/swapped and Help/View/Compare returns.
The rebuilt65-frame gate and Windows15-suite CTest passed. The replacement
capture visibly shows HELP2. Source accounting is unchanged, and original
modal/transition/audio timing remains unverified by these static tests.

### Compare indexed palette transition checkpoint

Recomp8002FF40 and fresh read-only Ghidra export
`.local/ghidra/compare_palette_fade.c` agree: interpolate masked WORDS using
signed truncation toward zero, masks001F/03E0/3C00, and copy target bit8000.
The four-bit blue mask is intentional source preservation. Shifting channels
first or averaging decoded RGB changes the rounding. 8002FF80 applies factors
0..16, snapshots the last blended160-word palette on a target change, and
maintains independent left/right halves. Same-target requests do not restart.
8002FE58 installs direct initial palettes. The native C state occupies1286 bytes.

`decode_team_backgrounds.py` additionally writes `indexed.n97pal` inside the
existing private background directory. Its16-byte little-endian header is
`N97P`, version1, strips4, palettes33, width128, height240, reserved0 (six u16s).
It contains33 original ordered four-byte tags plus160-word palettes, followed
by four records of30720 index bytes and96 local CLUT words; total134356 bytes.
Extraction rejects unexpected order, encoding, dimensions and public output
paths. Native loading rejects wrong size/header/order before rendering. This
preserves palette-index identity; no reverse RGB lookup or generated art is used.
The foreground and existing opaque background RGB expansion remain unchanged.
Original PSX transparent/semitransparent primitive blending is not established
by this palette arithmetic check.

Compare now uses this pack and advances both halves on the host logical tick,
including under Help. Logs expose target changes, interrupted fades and applied
factors0/8/16 without repeating settled state. This changes Compare only; the
older View Rosters RGB crossfade is a separate migration target. Native17ms
timing/catch-up is not claimed to reproduce original VBlank or frame-pump order.

Evidence:208896 masked-arithmetic vectors; same-target, interrupted and two-half
controller scenarios;18 full synthetic indexed render frames (all256 indices
including untouched local colors); malformed-pack guards;82 private UI captures.
Seventeen Compare frames independently check14144 exposed background pixels
against raw CLUT math and require the inactive half to remain unchanged. Private
lossless PNG inspection copies are emitted for factors0/8/16. The existing
return-state, modal bounds, portrait, audio-dispatch and persistence regressions
remain separate. This is source/native evidence, not a paired no$psx recording;
original audio onset, motion cadence and two-tick player refresh were still
pending at that checkpoint; the next checkpoint implements the text barrier.

### Compare player-cycle presentation continuation

Fresh recomp/Ghidra evidence (`.local/ghidra/compare_refresh_order.c`) confirms
59928 updates the requested player ID, calls39574(0,2), then59808. The latter
refreshes three header descriptors24..26 or30..32 plus visible value descriptors.
The selector dispatches the directional cue only after this callback returns
(3E240/3E310 ->2F124). It cannot process a second menu action during the callback.

The first39574 iteration builds graphics before310D8 observes the new player ID.
310D8 disables the old object and queues an asynchronous asset request via38CD8;
30E78 re-enables it on successful decode/checksum completion. The queue uses a
source-clock deadline (60 for an existing portrait slot), not a promise that the
new portrait arrives on either text-wait frame. Exact clock/I/O latency and
hidden-portrait intervals still require original recordings. The native path
retains its first frame, loads the local PNG after the first completed present,
and retains old text until the second; it does not simulate the CD scheduler.

`Nba97CompareRefresh` adds14 bytes alongside the12-byte selection state. The host
uses a paint acknowledgement and a subsequent logical tick, never a catch-up
loop, so a debugger pause/minimized window cannot consume unseen phases. The
same continuation drives the deterministic compositor captures. Help, side/team/
layer/player/scroll changes and return keys cannot escape the pending callback.
The palette pump advances with these presentations. Text publication and cue
dispatch happen once, after the second. Team scans do not acquire this wait.

Another corrected source interpretation: context+0x708 equals0x6CE+29*2, the
derived free-agent count.59928 suppresses cycling when team29 has exactly one
player, clearing the cue latch. It does not suppress ordinary one-player teams.
The older View Player adapter's misleading mode comment was corrected; its
public predicate names remain unchanged for compatibility.

Tests cover116 normal-team sequences (29 teams, both sides/directions), four
free-agent sequences (100 entries then1), and an ordinary one-player roster.
Each normal sequence checks both retained-text phases, all blocked input masks,
single cue emission, unchanged draft/top and immediate team-scan refresh. Three
actual host round trips check blocked input/audio and two completions each.
The85-frame verifier additionally compares old graphics -> new portrait/old text
-> refreshed text, checks original opaque portrait pixels, unchanged inactive
half, and CLI frame/completion/sound order. This is native/source evidence only;
post-callback repeat timing, real CD delivery, View Player host scheduling and
paired original motion/audio remain pending. No instruction ledger credit added.

### Compare post-callback pacing: held player cycling connected

Recomp3AE4C/3E38C supplies a directional post-callback delay7/5/3/1 at repeat
counter thresholds16/28/38, followed by one input-poll presentation. The normal
frontend path records the accepted controller/mask twice (normal branch plus
common tail): fresh/reversed input ends at2, repeats advance4 capped at48.
The C pacing adapter is tested for40 accepted actions,120 delay/poll
presentations, reversal, idle reset and invalid state. The host now retains the
post-selection wait after its two-presentation text callback and blocks other
menu actions until it ends. Three private host round trips each check the
initial7+1 presentations. This is not the entire original input scheduler.

The first private audit failed because it incorrectly called manager+0x10 a
held-input flag. Fresh read-only Ghidra and recomp2C668/2C610 disprove that:
3D930 counts descriptor signed types>=64; all57 Compare types are below64, so
3AE4C checks the selected display object's animation before its poll.2C610 reads
flags+0x3B, unsigned progress/limit bytes+0x26/+0x27, and signed bytes+0x29/+0x2A.
A pending0x10 channel replaces the0x08 result rather than OR-ing it. There is
no controller read in this predicate. The verifier now asserts zero bypass
descriptors and records this dependency instead of treating it as key release.

`nba97_compare_animation_pending` has16,777,216 flag/byte test vectors plus
independent-channel cases. Follow-up recomp/read-only Ghidra resolves the
left/right callback's selected-object wait:59808/3B26C rebuild the selected stat
value using2C6B0, initializing flags+3B to zero.2C244(copy=2) copies only the
old flags masked byC7 (the ANDI at8002C37C is304200C7). This removes both18 wait
bits regardless of the prior counters, tested across all256 input flags.
The new value becomes the descriptor's latest text object. The arrow cue in
3D534/2ADEC is a different object and uses color flags, not this geometry wait.
The private verifier checks the original instruction word as well as the
descriptor-type contract. This is a scoped lifecycle proof for player cycling,
not a general implementation of the original text-object renderer.

Held left/right is now enabled after the two-present callback, post delay and
poll. Runtime reads current mapped key levels, ORs chords rather than applying
direction priority, and ignores input while the game is background/minimized.
Opposing directions wait until one is released; the surviving direction starts
with a reset counter. Help/Cancel still held at the terminal poll are dispatched
after the callback, not lost. Windows keyboard autorepeat is ignored. CLI logs
the mask, counter, delay, callback text publication, cue and clear selected-text
wait without spamming unchanged chord polls.

A distinct host scenario completes40 held-right actions with80 callback plus120
post/poll presentations (200 total), checks private-database identities and wrap,
then tests reversal, opposing direction release, Help return and held Cancel.
It checks44 total callbacks/88 phase completions and44 ordered selector cues,
with the535-slot draft and parent editor unchanged during browsing. These
render/continuation tests inject input masks; they do not measure real keyboard
sampling, physical focus changes, monitor presentations or acoustic output.
The85 original compositor checkpoints keep their separate assertions; the
verifier requires the extra host run rather than weakening old event counts.

Verification: rebuilt Windows17/17 and Linux15/15 suites pass;85 private screen
checkpoints pass. No paired original recording was added. Remaining: original
motion/audio capture; exact arrow color-flash cadence; other selector callbacks'
repeat/scroll scheduling; physical input/focus tests and View Player migration.
The native17ms logical clock and presentation acknowledgements are not proof of
original VBlank timing. No instruction ledger credit added for this slice.

### Compare arrow color-flash continuation

Recomp2ADEC/2AC2C/2AE5C and fresh read-only Ghidra exports recover the arrow's
separate color state. The player-change callback completes, its selector sound
dispatches, then3D534 flashes the active side's left/right glyph (8D/8A). Four
independent15-byte states continue updating while hidden or beneath Help.
The renderer modulates the original font glyph RGB with neutral128, preserving
alpha, rather than replacing it with a generated arrow or flat luminance tint.

The bounded transition has four interpolated updates toward120/102/0. Update5
enters a ten-update hold, update16 enters return, updates17..20 return to128,
and update21 clears the phase. Retrigger during fade-in is ignored; during hold
it resets elapsed time; during return it restarts the fade. The source's unusual
red-channel retention when copying start green/blue is preserved and tested.

Native tests exercise256 equal-channel initial colors across21 updates, settled
state and all three retrigger stages. A real host callback adds22 captured phases
(0..21), independently checks1892 original-glyph pixels, and requires every
other pixel and the535-slot parent draft to remain unchanged. The private font
metadata derives the tested right-arrow bounds130,116 through142,127; no guessed
rectangle or screenshot threshold is used. Fade/hold/return image hashes are
checked separately. CLI logs expose trigger/cue and phase transitions, not every
settled tick. The host run now has45 callbacks/90 phase completions: the previous
44 callbacks plus this one flash primer. The40-action held-input test still
requires200 presentations after the normal-mode counter correction below.

Verification: Windows17/17, Linux15/15, the complete local Re-order regression,
and107 private compositor frames pass (85 previous checkpoints plus22 phases).
Native starts the arrow's color fields at128. Original2C6B0 initializes primitive
RGB and flags but does not initialize all object start-color fields; their first
flash may depend on reused allocation contents. This is an explicit unresolved
runtime dependency, not a claim that the original initial color is128. Expanded
8-bit native modulation also does not establish PSX5-bit quantization/dithering,
original VBlank timing, hidden-object cadence or acoustic onset. Paired original
reference cases remain0/14; no instruction ledger credit was added.

### Counter audit correction: normal branch falls through the common record pass

While tracing other Compare selector timing, the recomp exposed an omitted
second repeat-counter update. Fresh read-only Ghidra output
`.local/ghidra/compare_repeat_counter_audit.c` confirms it. For context+720==0,
3AFD0..3B034 compares the accepted mask/controller, increments2 or resets0,
then publishes the new mask/controller. The jump at3B030 enters3B0B0, whose
common block compares them again and increments2 (if below48). The second
comparison therefore matches even after a reversal. The nonzero+720 branch
skips the first block; it is not the normal-mode adapter implemented here.

Correct normal-mode sequence:2,6,10,14,18,22,26,30,34,38,42,46,48... . Forty
same-direction callbacks therefore use4 delays of7,3 of5,2 of3 and31 of1.
Their post-delay plus mandatory poll totals120 presentations, plus80 callback
presentations gives200. The earlier162/242 totals were wrong: the tests copied
our single-pass interpretation. Those results must not be cited as original
timing evidence. Earlier sections now show the corrected current counts.

The native C adapter now preserves both record passes without changing its
four-byte state. The focused suite adds150 prior-mask/even-counter/direction
vectors,65534 rejected non-direction masks, reversal/release checks and the
40-action host sequence. CLI pacing lines identify two normal record passes.
The private screen verifier hashes the audited mode branch and both complete
record blocks, including their connecting jump and delay-slot mask publication;
this anchors the interpretation to the original code without publishing it.

Rebuilt Windows17/17 and Linux15/15 suites and107 private screen frames pass.
All45 host callback/cue events and90 callback phase completions remain checked;
only the erroneous post-delay schedule changed. Source accounting remains
875/875 for its existing ten owners, not a new percentage or a claim that all
selector timing is finished. Up/down geometry, shoulder/side callback waits,
physical input sampling and paired original motion/audio are still separate
unfinished work. No original-reference case was closed by this correction.

### Compare generic callback post-wait and held input

Recomp3D930 dispatches the selected descriptor's+30 callback for masks intersecting
3E50 (ANDI at3E274). All48 selectable Compare stat descriptors route to59F20.
Its exact masks200/400 scan teams,1000/2000 change layers, and800 switches sides
in state23. Other masks clear the cue without changing Compare. After callback
return and optional cue6,3E388 unconditionally supplies5 to39574. Then3AE4C
pumps the next input-poll presentation. The five-frame delay therefore applies
even to silent no-ops; it is not a key-release wait and does not accelerate.
Fresh read-only Ghidra confirms the selector call/delay and team/side functions;
the split59F20 entry remains grounded in recomp and its original instruction
bytes rather than a fabricated Ghidra function export.

The C adapter shares the existing four-byte normal-mode counter/pacing state.
Generic callback requests use post_frames6; player callbacks retain their separate
two-present text continuation and accelerating directional post-wait. The host
now handles sampled masks directly at the poll, repeats held team/layer/side
buttons, and preserves mixed masks rather than selecting the newest key-down.
Generic chords take the callback path (usually silent no-op). Other contradictory
masks keep polling until a supported mask remains. Help/Cancel held at the terminal
poll are dispatched only after the wait. CLI reports callback mask/event, fixed
delay/poll count and completion; unchanged idle polls are not spammed.

The pure C test enumerates all65536 masks:65024 route through the callback gate,
five exact masks change Compare and the other routed masks retain state. Each
routed mask checks six completed presentations, pending-request rejection and
shared normal counter history. Host checks separately cover all five held routes,
four silent no-ops (including mixed direction/callback and opposing-team chords),
then Help and Cancel:16 waits,96 presentations,12 cue6 dispatches and four silent
callbacks. Injected input cannot change the composed frame, child state, audio
record or draft during any wait. Child return changes only the declared child-
result/input-barrier fields; roster/editor content stays intact.

The earlier first/replacement/swapped capture routes now also respect the callback
waits before sending their next action. The palette sequence is captured during
the team callback's wait and remaining fade, BEFORE the subsequent layer change.
An extra settled-frame capture retains the exact factor16 equality assertion;
comparing factor16 with a later changed layer would be an invalid requirement.
All108 captures pass, including17 raw-palette factors,22 arrow phases and the
new settled frame. Windows17/17 and Linux15/15 pass. The private verifier checks
48 descriptor pointers, the ANDI word and hashes of both audited callback blocks.

This does not implement up/down geometry transitions, the general selected-text
animation scheduler, original asynchronous CD delivery, physical keyboard/focus
testing, PSX VBlank cadence or paired acoustic/video capture. Native polling is
still presentation-driven with the17ms host clock. These source/native checks add
no original-reference or instruction-owner credit; the broader goal remains open.

### Compare sequential stat-scroll presentations

Recomp3AB64 invokes3A914 (Down) or3A6BC (Up) for group0, then group1,
restoring the original active side. Each successful move calls3A650, which
creates the entering row, schedules clipping and pumps39574 once. Fresh
read-only Ghidra confirms these routines and2BBA0/2B5AC. Clipping updates
geometry and UVs before relative translation; it does not stretch the font.
2C1EC sets the outgoing object's lifetime to0, and2D3B0 frees it before
the next geometry update. Relative translation duration1 moves a full14 pixels
in its first update. This is not a multi-frame smooth slide.

The original ZFONT1 pack has167 directory records, all with positive height
at most14 after applying the transposition flag. That is a precondition for
the native projection: an entering glyph is fully collapsed during its group's
preparatory presentation, then restored while moving into place. The host
rejects a replacement font exceeding14 pixels instead of silently assuming
the same effect. No generic PSX text-object engine or runtime emulation is used.

| Presented phase | Left values and shared labels | Right values |
| --- | --- | --- |
| First internal pump | Previous five rows | Previous five rows |
| Second internal pump | New five rows | Previous five rows |
| First post-callback frame | New five rows | New five rows |

The14-byte C refresh continuation now represents this ordering as well as
player-cycle refresh. Cue3/4 dispatch occurs after both internal pumps return.
The primary Up/Down marker pair is rebuilt after group0's pump; the direction
marker flashes only if it exists at the new top. Two additional15-byte tint
states reuse2ADEC/2AE5C semantics and original glyph RGB modulation. No stat-row
gold pulse is added: Compare manager+18 suppresses that generic selector effect.

Normal Compare manager+0D is7;3B15C shifts it right once, producing a fixed
three-presentation post-delay.3AE4C adds the next input-poll presentation.
The duration1 geometry is already complete before that poll. This does not use
the player-cycle C7 rebuild shortcut. Held Up/Down preserves the shared two-pass
input counter but does not acquire Left/Right's accelerating delay. Bottom-Down
has four post/poll frames, no internal callback pumps and no cue. Top-Up is
different: its callback is null, so only the one input-poll frame remains.

Verification:18 C controller scenarios now include236 successful scroll paths
across all four layers/both active sides and16 endpoint cases. All65536 masks
are checked against the scroll pacing gate. The actual host exercises45 actions,
251 logical presentations and40 cue dispatches, including holds through both
ends, reversal, and blocked input during callback/post-delay. Six new captures
check the actual left/label/right pixel groups independently, plus restoration
after Down then Up. An independent decoder also checks the Down glyph's first
two flash updates and neutral rebuild after Up. This caught and corrected a
native draw-order regression: shared labels must not paint over the marker.
All114 private screen checkpoints pass; Windows17/17 and
Linux15/15 pass. Eleven audited original code blocks and the local font hash are
recorded in the private report. Source hashes anchor the audit; they do not
automatically prove semantic equivalence. Existing875/875 owner accounting is
unchanged and this adds no original-reference credit.

### First-row callback initialization resolves the top boundary

The earlier raw-index concern missed runtime initialization. Static descriptor
data assigns3AB64 to both scroll callbacks on all48 rows. However,5A1EC loads
manager+88 (descriptor0), stores zero to descriptor+20 at5A270, then loads
manager+10C (descriptor33) and stores zero to its+20 in the return delay slot
at5A27C. Recomp and a fresh read-only Ghidra export agree. Inspecting only the
static callback table or3A6BC is insufficient.

3D930 checks the selected descriptor's Up callback before setting cue3 or
dispatching/delaying. Entry initializes cursors to0/33; successful Up moves each
cursor to its new top, and a changed layer extent resets both to their minima.
The side switch59A88/39BA8 does not change those indices. Thus top-Up cannot
reach3A6BC's raw-zero guard on the right through normal Compare navigation.
Bottom-Down still dispatches and clears its cue inside3A914.

The pacing API now takes the state **before** navigation: an Up landing on0
still needs delay3, unlike an Up requested while already at0.320 held top-Up
polls cover both sides and all four layers, along with input-history increments,
busy rejection, Down reversal, Up-to-zero, bottom Down and malformed-state
atomicity. No persistent state bytes were added. Host CLI reports
`dispatch=null-up-5A1EC delay=0 poll=1` (fields may be separated by other data).
The prior260-presentation expectation was wrong; three top-Up requests each
lost the erroneous three-frame delay, producing251, not extra test credit.

The private gate hashes initialization and selector control flow, checks the
seven initialization words including the delay-slot store, and checks all48
static callback pairs. Hashes anchor the reviewed source, not runtime proof.
The14-case original-reference contract now includes top-Up on both sides and
after a down/up round trip, with required separate column-top annotations.
Original VBlank/display-buffer cadence, initial allocator tint state, physical
keyboard/focus behavior, general text scheduling and paired video/audio remain
open. Source resolution does not turn these native regressions into end-to-end
original matches.

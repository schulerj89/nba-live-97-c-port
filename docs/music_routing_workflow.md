# Native music routing boundary

`src/recovered/music_routing.c` recovers FEONLY `8002F258`, `8002F330`
and the state/call ordering of `8002F36C`. This is a portable routing owner,
not a complete stream backend or evidence that the host plays every track.
Audio devices, decoders, buffer ownership, asynchronous effects and the
source stream/CD/SPU dependencies remain native adapter responsibilities.

## Selection and retained original behavior

Initialization chooses the initial menu resource without a random draw. The
next-selection branch first consumes a nonzero one-shot override; otherwise
it chooses the special `ZTPAUSE` resource when `ED2AC` is nonzero; otherwise
it calls the existing **16-bit title/Cool Fact RNG** once and indexes the
sixteen source filename slots using the low four bits. It never draws from
the separate six-word cursor/match RNG and must not reseed the title RNG.

The original table contains repeated resources, including `ZTPAUSE` in normal
selection. Immediate repeats and unequal selection weights are retained. The
private extractor preserves all sixteen slots in source order; the native
adapter maps each slot to an owned nonzero resource token. It must not use a
shuffle bag, remove duplicates or exclude the special resource.

`ED2AC` is not established as a gameplay Pause button flag. The inspected
FE resource transition `31A88` writes it as `resource_state == 0x24`, the
View Player state. That transition also blocks selection while loading,
requests a50-tick fade/phase4 where required, and saves/reduces/restores music
volume around state0x24. Those caller effects are not implemented by this
routing core and must not be invented from its `pause` input field name.

Original quirks remain visible in code and tests:

- Deadlines use strict **signed absolute** comparisons, including their wrap
  behavior. Equality does not expire a deadline.
- State3 continues checking the other conditions after setting state4.
- Source `6B6A0` returns only `-14`, `1`, `3` or `4`, never zero. The caller's
  zero-status branch is retained even though this callee cannot take it.
  Mapping that callback to a Boolean `isPlaying` would change original behavior.
- The synthetic zero-status branch enters state11 with inhibition set. No
  automatic clearing is added; it requires a separate caller effect before
  state11 can run. Exercising this branch with a test hook is not evidence of
  an original reachable path.
- The30 pump/refill pairs in state2 remain ordered, and subsequent time reads
  happen after those calls. Separate clock calls are not collapsed.

## Interface and integration

`nba97_music_routing_init` and `nba97_music_routing_step` accept owned state,
source inputs, an extracted resource mapping and a synchronous callback.
Callbacks have named source boundaries and preserve their argument order.
All required pointers are validated before effects; source/state/resources/RNG
must not overlap. Opaque resource and stream/voice tokens are not dereferenced
as PS1 addresses. Initialization retains fields outside its write scope.

The source clock is the free-running120Hz IRQ6 clock, not frontend presentation
count. `8DA5C` reads `D9AB8`; callback `78628` increments it. The existing
`288EC -> 7844C(120) -> 7F388(timer2)` audit establishes the producer. The
26500-tick playback cap is approximately220.833seconds and the120-tick retirement
timeout is1second. Fade parameters use a separate audio service: `7A6A8`
normalizes callback counts by multiplying by100 and dividing by its configured
rate, while `7A81C` advances fades on every third service. A fade argument of60
must not be converted using the120Hz frontend clock. Actual fade curves
and callback cadence are separate dependencies, not supplied by these units.

The current source `FINISHED` helper `6FCF0` reads unsigned byte `E45E7`.
The container reader recognizes `SCEl` in `6DBC0` and `6E4D8`, transitioning
the stream to phase7 under their respective conditions. The complete chain
from container completion to buffer drain/voice completion and that byte is
still open. Do not treat a file end or FIFO exhaustion as an independently
verified substitute for this whole backend.

Run the extractor from the repository:

```powershell
python tools/extract_music_resources.py --disc .local/input/nba-live-97-slus-00267.bin --overlay .local/extracted/FEONLY.BIN --output .local/assetpacks/menu
```

It verifies the exact source disc/overlay/resource hashes and fixed CNK format,
then writes all five CNKs, `music_slots.bin` (16x13bytes), and
`music_routing.json`. Outputs must stay below this repository's `.local`;
source/output and output/output aliases are rejected, including hardlinks.
Different existing outputs are refused before publishing any file. Identical
outputs are retained without changing their timestamps.

The manifest contains initial/special resources, sixteen filenames, per-track
hashes/counts/rates/durations and source identity. No music/table bytes are
embedded in public code. The source frontend entry and UI adapter must install
the extracted mapping, preserve the shared RNG, provide the actual lifecycle
effects and deliver natural completions before claiming multi-track playback.

## Verification receipts and limits

Private evidence is in
`.local/verification/native_completion/music_routing/`:

- `oracle.py`/`report.json`: original binary instructions, correct FE base,
  all65536 RNG seeds plus2176 lifecycle cases;7566 matched callback events
  including state at each call. Audio/clock dependencies are explicitly hooked.
  This is not a live original playback capture.
- `tests/music_routing_tests.cpp`: synthetic lifecycle, callback ordering,
  guards, precedence, clock equality/wrap and retained source quirks.
- `tests/test_extract_music_resources.py`: seven synthetic format/privacy/
  alias/preservation tests without original asset fixtures.
- `decode_report.json`: all five existing native full decodes; exact declared
  frame counts and private PCM hashes. `portable_ffmpeg_report.json` proves
  the portable `ea_schl.cpp` refactor produces the same bytes for all five.
- `chunk_report.json`: one `SCCl`, all `SCDl`, and a terminal `SCEl` for every
  track. Every original ADPCM frame flag is2. The padded tail exceeds declared
  samples by15/16/4/20/19 frames respectively. Flags do not justify treating
  every file as an endless PCM loop.

Independent FFmpeg8.1 decoding preserves original channel ADPCM bytes in mono
VAG wrappers because its direct demuxer rejects this fixed EA header. All
70751260 compared stereo samples have the correct lengths after declared tail
trimming, but native and FFmpeg waveforms are **not bit identical**. Maximum
absolute differences are78/64/63/64/70 signed16 units; RMS differences are
19.95/20.42/17.65/20.93/21.36. Neither output clips on these tracks.

The difference is entirely explained by predictor rounding: native uses
arithmetic shift of `(sum+32)`, while FFmpeg uses signed `sum/64`. A private
diagnostic changing only that expression matches FFmpeg exactly for all five
tracks. See `rounding_report.json` and the
[FFmpeg8.1 decoder source](https://github.com/FFmpeg/FFmpeg/blob/n8.1/libavcodec/adpcm.c#L2567-L2604).
No public rounding change was made. FFmpeg is an independent decoder, not an
original SPU waveform oracle; hardware interpolation/mixing/onset parity and
capture/listening remain unverified.

## Other original audio remains open

`inventory.json` records raw subheader observations: `Z0ZSONGS.XA` contains
eight file1 audio channels0..7; DITA..DITF each have eight channels and DITG/H
each have sixteen. This is a channel inventory, not proof of that many unique
or source-reachable songs. TITLE and VIC also contain video sectors and one
observed audio channel each. No XA decoder or speculative playlist was added.

The known XA selector `2941C` sets filter file1/channel `(selector % 8)`;
its alternate media branch indexes a bank by signed selector/8, while the
ordinary branch uses its single loaded position. `297A8` and `298E8` enable
distinct optional paths, and `29640` can consume the six-word RNG. Their
ordinary frontend reachability was not established by the earlier audit.
GAME overlay owners must be audited separately before applying FE addresses.
Gameplay effects/crowd/speech, stream transitions, all original reachable
music, natural end-to-end host playback and full-match audio remain open.

# Cursor audio scalars and shared RNG

2026-08-30. The native cursor path now uses the original integer pitch and
staged7-bit gain calculations, and advances the shared six-word RNG for each
accepted unmuted cue before device submission. This is a bounded source/state
correction. It does not recover the original voice allocator or SPU waveform.

## First mismatches

The first state difference on an accepted cue was the missing `7A538` call.
`2F124 ->9180C ->9267C` allocates a voice, then calls `93190` at929D8. Even with
zero random-gain amplitude, `93190` calls `7A538` before multiplying by zero.
The source advances all six words at800C73E4; the native player previously
advanced none. Circle's cue6 at4F954 therefore changes the state used by its
first candidate call at4F96C. The correction is at the accepted-cue boundary,
not inside the random-candidate helper or a later renderer.

The pitch producer also differed. For authored+100 cents, source72048 returns
2168 against base2048; the previous native exponential ratio was1.059463094359
instead of1.05859375. Negative pitches differ too: -100 gives1940 and -400
gives1628. The original256-byte lookup remains a private asset, not public code.

Gain had an earlier integer boundary than the old combined PCM multiplication.
For cue6 at setting9, source effective volume is63 and both volume registers
are8127. Native previously used a combined multiplier0.502201004402 without
the intermediate truncation. The recovered fixed-domain stages are:

~~~text
authored = floor(program_volume * tone_volume / 127)
effective = floor(authored * playback_volume / 127)
left_register = right_register = effective * 129
~~~

Combining the divisions is incorrect even before PCM rendering: the invented
program41/tone44/playback108 vector gives authored14/effective11, not12.

## Native boundary and ownership

`recovered/frontend_audio.c` owns the pure scalar projection. It validates7-bit
volumes, authored cents[-1200,1200], and exactly256 unsigned table entries;
refusal preserves output. It has no file, RNG, allocation, device or PCM effect.
The complete131-instruction pitch owner is retained despite the bounded domain.

`RecoveredAudioPlayer` validates immutable BNKl/PATl/tone/TMxl/envelope data,
decodes the selected original ADPCM interval, scales PCM by effective/127 and
resamples linearly using pitch_register/2048. Those last two operations are the
declared native rendering policy, not SPU interpolation/envelope/mixer recovery.
The existing Cool Fact gain and exponential-pitch policy is unchanged.

The accepted callback runs once after validated PCM preparation and before
`playPcm`. A later device-open/prepare/write failure does not undo its RNG draw.
Valid unmuted zero-effective-volume cues still draw. Muted cues return before
asset reads or interruption of an existing cue; invalid/null IDs and rejected
preparation do not draw. Inspection, preparation and exports do not draw.
Failed playback preserves prior published clip metadata while the accepted
event remains visible in the host draw counter/error trace.

Native preparation acceptance is an explicit substitute for successful source
voice allocation. The original has24 voices; the native cursor player interrupts
one WinMM buffer. Voice exhaustion, overlap, stealing, priority and hardware
failure outcomes are not equivalent. Stricter malformed-asset refusals are native
safety policy, not reproduction of original corrupt-data execution.

Supported assets have one full-range mono16-bit codec6 tone at22050Hz, centered
pan, zero random amplitudes/maps/reverb/modulation, default bend, constant127
envelope and master127. All12 real cursor programs meet this domain. Unsupported
variants, truncated/self-relative ranges, misaligned payloads and sample counts
exceeding compressed capacity are refused. No generic fallback is substituted.

## RNG lifetime

Cold boot801E1BF4 loads FEONLY at80015000. Entry7B79C clears BSS fromD9BDC to
FF5C8, leaving the initialized six-word data intact. The early validated original
RAM snapshot matches that static seed. Native now loads the small private seed
at frontend bootstrap and preserves it across Team Select/User Setup/menu
reentry. It no longer resets the stream on the first Team Select asset load.
Both actual native cursor-play call sites use the same acceptance callback.

The title/Cool Fact16-bit29B20 RNG remains separate and unchanged. Capture
receipts expose all six shared words and a cursor-only draw counter; candidate
generation also advances the six words but does not increment that counter.

Full startup history is still unproven. A conditional XA/media consumer29640
uses the same RNG only with enabled flags and a signed-expired16-bit timer.
Both flags are zero in static data and early RAM; no normal Setup route to its
enable routines was established. No speculative draws were added. A static
preference/card-init call graph can also reach2F124 before bank initialization;
its feasibility/bank state is unproven. Warm overlay reloads remain separate.

## Source ownership and evidence limits

New reviewed instruction credit is zero. Full extents below are supporting,
overlapping shared owners, not additive whole-game progress. Bank, allocation,
font/presentation and hardware boundaries are hooked in controlled source
fixtures. Complete paths through these owners remain outside the claim.

| FEONLY owner | Native scope | New credit / full instructions | Dependencies/evidence | Remaining uncertainty |
|---|---|---|---|---|
|8002F124|Mute/setting and cue dispatch|0 /21|9180C; all256 settings and host tests|Live cue timing|
|8002F178|Bank/master bootstrap evidence|0 /56|92D70,92CB0; private bank/transfer proof|Original lifetime/allocator|
|8009180C|Program selection boundary|0 /53|9267C; all12 populated and116 null slots|Other bank forms|
|8009267C|Authored gain/pitch and accepted-cue draw|0 /318|93190,76334; real/synthetic MIPS cases|Other tones/maps/voice allocation|
|80093190|Unconditional gain-random call with amplitude0|0 /35|7A538; cue/Circle source traces|Other random amplitudes|
|80076334|Effective7-bit gain|0 /76|9267C;1352 synthetic gain cases|Variable envelope/master|
|80070E54|22050-Hz base pitch2048|0 /394|72048; source scalar cases|Other formats/rates/voice state|
|80072048|Signed table-index pitch projection|0 /131|Private C6D60 table; all2401 supported cents|Wider octave/overflow branches|
|80093158|Default centered pitch bend|0 /14|72048; real-bank scalar cases|Nonzero bend range|
|80071EBC|Centered equal left/right volume|0 /99|Final attributes; source scalar cases|Other pan/envelope behavior|
|80071D8C|Volume/pitch attribute fields|0 /76|SpuVoiceAttr source fixtures; exact scalar metadata|Other masks/voices/addresses/ADSR and hardware submission|
|8007A538|Shared six-word recurrence|0 /52|93190,4F934; exact state receipts|Complete consumer history|
|8004F934|Cue before first candidate and rejection retries|0 /41|2F124,7A538; four seeded actual dispatches|Full original presentation/runtime|

No pre-existing Team Select or instruction-semantics denominator/credit changes.
The71D8C denominator covers the complete304-byte extent ending at71EBC, not just
the tested attribute stores. Full hardware submission behavior is not claimed.

## Reproduce and verify

Run `python tools/extract_cursor_audio.py` against the existing private FEONLY,
or the normal asset extraction script. It validates full source-owner hashes
and writes `menu/zcursor_pitch.bin` (256 bytes) and `menu/frontend_rng.bin`
(24 bytes) only below.local. Input/output collisions, hard-link aliases and
resolved output escapes are rejected before writes. No source/table/seed bytes
are committed.

Build Debug/RelWithDebInfo and run CTest, then
`scripts/verify_team_select.ps1 -SkipBuild` for each configuration. The script
also runs cursor and unchanged speech numeric checks against private assets.
`cursor_rng_cases.json` uses hand-specified seeds, actual Circle dispatch and
independent original-MIPS expectations, including rejected candidates:

| Seed | SF/X | Cue draws | First accepted team | Six-word state after acceptance |
|---|---|---|---|---|
|1,2,3,4,5,6|0|0|21|21,20,18,15,11,7|
|1,2,3,4,5,6|9|1|28|92,71,51,33,18,8|
|29,0,0,0,0,0|0|0|4|36,6,5,4,3,3|
|29,0,0,0,0,0|9|1|4|36,6,5,4,3,3|

The last two cases converge because mute rejects29 and30, whereas the unmuted
cue consumes29 before candidate30 is rejected. Neither changes presentation
count or the separate title RNG.

Validation on2026-08-30:

- 45/45 CTest tests pass in Debug and RelWithDebInfo; all98 state/frame scenarios
  repeat within each configuration and match across them, including owned match
  snapshots and four seeded dispatch receipts. Real saves/config remain unchanged.
- Public scalar tests cover2,097,152 gain tuples,541 additional scalar vectors
  and396 refusals. Cursor PCM/parser tests cover168 invented level vectors;
  actual assets add144 cue levels and existing speech regressions.
- Actual-MIPS comparison covers2401 pitch values,1352 synthetic gain cases and
  132 unmuted real-bank scalar cases. Independent native export comparison
  matches36 cases/72 WAVs/640,440 samples, including all five scalar fields.
- Private WinMM stubs verify accepted-cue ordering even on forced device
  failure and zero effective gain, without opening a device. Extractor probes
  verify10 refusals before writing and successful256+24-byte output.
- The independent capture RNG verifier agrees with10,042 original-MIPS cases
  and12 stored vectors. Equal-but-corrupt capture words are rejected, including
  an all-zero stream; repeated artifact equality alone cannot satisfy the check.
- Create Player retains27/27 repeated scenarios,753/753 projected vertices,
  251 primary packet/order records and zero missing sampled texels. Rosters
  menu's six audio exports retain valid bank/pitch/count/repeat behavior.

Private final Team Select runs: Debug `run-20260830-191913-ea17a276`, release
`run-20260830-191950-7924b576`; Create Player `run-20260830-191946`; Rosters audio
`cursor-menu-20260830-191945`. Logs use.local/logs/cursor_scalars_*.
Source/export receipts are under.local/verification/gameplay/audit_b/audio_selection;
host/extractor probes under.local/verification/team_select/audit_c/cursor_audio_probe;
lifetime evidence under.local/verification/team_select/audit_a/rng_lifetime.

Original synchronized audio, physical controls, arrow/text lifetime and full
Team Select runtime still require verification. Gameplay launch remains blocked
by its explicit native dependency boundary, not silently replaced with a demo.

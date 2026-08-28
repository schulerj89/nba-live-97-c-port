# Frontend title motion: native integration, reference timing pending

This is a dependency of the Re-order original-animation goal, not additional
credit in its875-instruction owner ledger. Re-order, Compare, Team Rosters and
View Player now use recovered C corner updates and a native single-quad renderer.
This is not a claim of frame-for-frame original animation or GPU equivalence.
Other frontend titles and older standalone View capture callers still use the
explicitly labelled legacy approximation; do not use those as new motion proof.

## Original evidence

Primary evidence is the private FEONLY recomp. A read-only Ghidra cross-check
is retained at `.local/ghidra/title_motion_20260827.c` (SHA256
`a9b86e8c9cc605c0c197b1746cdbe8f3db0475e2eb61f51be93c677b24e01847`).
Geometry cross-check: `.local/ghidra/title_geometry_20260827.c`, SHA256
`1f62d14f870b8bd92010e09362d8785e5a29c3971e52b13ecc32338c581070fe`.

| Source | Recovered behavior |
| --- | --- |
| 31A88 calls31F48;326E8..327A4 | Registers objects with flag0x100; stores their baseline rectangle and object index in two68-byte records starting800FF508 |
| 32BF0/32BF8,32CA4..33450 | Alternates the two object slots; an absent second slot consumes no corner RNG; writes each updated corner to both primitive buffers |
| 29B20..29B60 | Zero seed becomesA5A5; shift left, XOR1D87 when original bit14 is set, retain low16 bits |
| 32CCC..32DAC and33090..33170 | Eight ordered RNG calls; each coordinate is baseline+(random&3), not cumulative drift or signed -2..2 offsets |
| 395AC..395D0 | Normal selector presentation consumes one RNG result before optional title update; suppression at800C18EC still consumes that pre-draw |
| 395D0..395F4 | When context+2168 is nonzero, presentation waits for counter>=2;3F868 initializes the field to1, but other paths override it |
| 3282C and36898 | Separate presentation callers update the title directly, without39574's pre-draw, then wait for counter>=2 |
| 8ACC8/8ACD8,8B228 | Counter read/reset/increment dependency; presentation resets it after submission |

The raw layout tables each contain **one** flag0x100 object:

| Graphics layout | Table | Object | Tag | Position |
| --- | --- | --- | --- | --- |
| Re-order12 | 80096BC4 | 5 | ba22 | 156,10 |
| Team Rosters16 | 80096794 | 5 | ba35 | 142,10 |
| Compare35 | 800978C4 | 5 | ba02 | 170,15 |
| View Player36 | 80097A24 | 5 | ba41 | 40,18 |

Thus normal title updates alternate with an absent second slot. Do not
interpret the two primitive buffers as two horizontal texture chunks.
The earlier renderer comment incorrectly cited3186C (sprite replacement)
and34A5C (UV adjustment) as proof of its animation and128-pixel split. That
claim has been removed. The33DD4 path writes one FT4's texture coordinates:
normal flag0x100 presentation uses width-1/height-1 as final UV coordinates.
The title rectangle uses full asset width/height. ba22 is202x54, ba02 is167x54,
and ba41 is221x42. The two40-byte primitive records are display buffers.

## Implemented and tested

`src/recovered/frontend_title.c` implements the RNG step, baseline-relative
corner arithmetic, alternating slot behavior, and selector pre-draw/suppression.
The two-coordinate-set representation omits PS1 pointers/primitive buffers.
The RNG state is supplied by the caller so host integration cannot
silently substitute an unrelated random stream.

`nba97_frontend_title_tests` checks:

- All65536 RNG seeds against an independent instruction-arithmetic transcription,
  including the bit14 (not bit15) branch and zero fallback.
- Both slot counts across all65536 seeds and four updates: draw order, retained
  inactive object, alternating phase and no cumulative movement.
- Every signed16-bit coordinate with multiple draws: exact halfword wrapping.
- Selector suppression/pre-draw, absent-slot draw consumption and init guards.
- Host repaint stability and alternating phase retained across child/parent changes.
- Synthetic textured rectangles at widths1,127,128,129,167,202,221,255:
  transparent texels, last column and no artificial128-pixel seam.

These tests do not authenticate an original runtime sequence. The new suite
passes on Windows and Linux. Overall suites currently pass19/19 and17/17
respectively; no visual/reference percentage changes.

## Native host policy and verification

`FrontendTitlePresentation` preserves global alternation across layout changes.
Drawing does not advance state. The host requests nominal30Hz title updates only
after the preceding frame was painted, without catch-up after a stall. This is a
native policy, **not a measured original counter cadence**. Entry first displays
the baseline rectangle; original first-frame timing is not verified.

The native stream starts at zero, invoking the recoveredA5A5 fallback on first
use, and is shared between these titles and Cool Fact selection. Other frontend
random consumers have not been migrated. Original initial seed, prior draw
history and exact3282C/36898-versus39574 caller scheduling remain unverified.
CLI `TITLE-MOTION`/`TITLE-PRESENT` records layout, phase, RNG, total draws and
eight coordinates; ordinary sessions log initial updates and periodic samples,
recording sessions log every update.

The private screen gate adds9 Re-order frames to the120 existing checkpoints.
An independent arithmetic oracle checks actual host coordinates, nine-versus-one
draw consumption, alternate-frame pixel holds, visible changes and no changed
pixels beyond the original title extent plus3. Repainting twice must preserve
the image, RNG, parent selection and draft. Existing parent-return assertions
remain intact. These deterministic dispatches do not validate real-time pacing.

## Original frame-step observation (2026-08-28 UTC)

The existing Chicago Re-order session was advanced ten times with no game input:
Run > Run one Frame once, then its observed `KP_Divide` shortcut nine times.
Each stopped debugger view and emulator view was retained unchanged under
`.local/verification/reorder/original-frame-step-20260828/`. The manifest records
file hashes, sizes, manually transcribed PCs and per-run cycle counts. Original
execution was resumed with Run afterward; no breakpoint, RAM, save, binding or
setting changes were made. The attempted breakpoint dialog remained empty and
was cancelled because automated text entry failed.

`tools/inspect_title_frame_steps.py` independently compares a declared visible
title rectangle **within this original sequence**. It confirms identical pixels
at steps2–5 and6–9, with new shapes at2,6,10. Thus two complete observed holds
last four emulator frame steps. Stopped PCs include395E0/395E8/395EC and8ACC8/
8ACD0, corroborating execution of the recovered presentation counter wait.
Read-only Ghidra cross-check `.local/ghidra/title_clock_20260828.c`, SHA256
`e29f5a2b774b7141f2bbd7c66a921c519414e7e33897f67af071145946aeabfd`, confirms
8B104 registers8B228 through7F600 and8B228 increments800D9AD0.

This is stronger than a synthetic cadence test, but remains narrow evidence:
the window captures are1150x912 and include debugger occlusion below the title.
They are not native framebuffer dumps, continuous video or audio. The rectangle
was selected retrospectively from visible bounds, not searched for a native
match. No image is resized or normalized; the full captures remain intact.
The first step starts mid-frame, and no original RNG/vertex state was dumped.
Four-step retention supports the recovered two-counter-wait/alternate-slot
model; it does not establish exact native wall-clock scheduling or raster parity.
The inspector explicitly awards zero paired-scenario credit. Its four synthetic
tests cover hold grouping, static/different patterns and malformed/tampered data.

Run locally:

```powershell
python tools/inspect_title_frame_steps.py .local/verification/reorder/original-frame-step-20260828/observation.json --output .local/reports/title_frame_steps_run.json
```

## Remaining original-reference work

1. Extend the observed idle four-step retention to entry, modal and child paths;
   measure the original presentation counter, entry phase and call-path timing;
   migrate remaining consumers only with source evidence. Test native real-time
   scheduling under modal/transition/stall conditions against that recording.
2. Compare distorted textured quads with original GPU output. Native affine
   interpolation/nearest sampling is not PS1 fixed-point raster precision.
3. Capture matched original/native motion and audio. Built-in no$psx Utility
   inspection currently exposes still screenshot/dump exports, not a continuous
   video/audio item in that menu. The original was resumed via Run afterward;
   its roster, settings and saves were not changed. Capture geometry512x224
   versus512x240 and original brightness remain separate unresolved issues.

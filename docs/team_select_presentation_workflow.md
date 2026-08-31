# Team Select completed-frame ownership

2026-08-30. This checkpoint fixes native Team Select frames being recomposed from
state changed after the original presentation boundary. It covers ordinary
polling, random candidates and Team Select Help. It does not complete the source
text manager, GPU submission, original timing, arrow history or physical input.

## First mismatch and bounded correction

At the first Left, the source `39574` presentation precedes `3AE4C` input sampling
and the `3D930` callback. The home team in that submitted frame is still3. The
native poll updated the logical team to2 correctly, but the later generic
`rebuildMenuFrame` immediately rendered team2. The first differing datum was the
team passed to the renderer for the completed poll frame, not the navigation
result. Random candidate changes and Help input had the same ownership leak.

`updateTeamSelect` now composes the frame after title/tint/palette advancement
and before callback, random or Help input continuation. C++ retains that owned
pixel frame in the existing `menu_frame_`; ordinary rebuild/paint cannot publish
the later logical state. The next accepted presentation shows the changed state.
Initial entry composition remains uncounted and is invalidated on every entry.
The capture metadata records logical state and the distinct shown state; shown
fields only describe Team Select and remain historical on other pages.

Help needs an additional split. The pure C
`nba97_help_prepare_presentation(state, shown)` advances growth/shrink geometry
before drawing and tells the caller whether input may be sampled afterward.
Its distinct snapshot preserves the terminal growth box without text even
though the continuation is ready to create text. The API rejects null or aliased
state/snapshot pointers without mutation. The existing combined tick API and
other Help owners are unchanged; this is not a phase fix for every frontend page.

For the actual Team Select rectangle, source geometry yields this sequence:

| Presentation | Submitted Help content | Continuation after presentation |
|---|---|---|
| Invoking poll | No modal | Open Help and select cue7 |
| First growth | Width38, no text | Continue growth; no input sample |
| Growth13 | Full250x110 box, no text | Create text and enter changed-input barrier |
| Next mandatory barrier | Full box with text | Compare remembered input |
| Acknowledgment | Existing full box and text | Remove text, mark shrink phase and select cue8 |
| Next shrink | Reduced box, no text | Continue shrink; no input sample |
| Terminal shrink | No box or text | Enter final changed-input barrier |
| Final barrier | No box or text | Return only when the sampled mask differs from the acknowledgment |

The mandatory text barrier may also acknowledge on that same completed frame
when the changed mask is nonzero. It does not require an extra acknowledgment
frame. The final barrier likewise accepts a changed nonzero mask; release to
zero is not required. Cue8 occurs before the first shrinking presentation, not
at the final return.

The native host's frame composition is a presentation ownership boundary. It
does not prove that the OS displays the pixels before audio hardware receives a
cue, or that Windows paint timing matches original VBlank/GPU behavior.

## Source owners and accounting

Full source extents remain intact. These are overlapping shared dependencies;
the table is not an additional whole-game denominator. This checkpoint adds
zero reviewed instruction credit. Controlled fixtures hook font/GPU/input
boundaries; no claim covers every branch of these functions.

| FEONLY owner | Bounded scope | New credit / full instructions | Shared dependencies and evidence | Remaining uncertainty |
|---|---|---|---|---|
|80039574|Presentation before caller continuation|0 /276|3AE4C,3D930,4F934; extracted host phase probe|Full list/GPU/absolute phase|
|8003AE4C|Input follows completed poll frame|0 /210|39574; existing poll tests plus98 captures|Queued text, driver/cache timing|
|8003D930|Callback mutation follows poll frame|0 /828|3AE4C, callbacks; extracted host probe|Other states, text/arrow lifecycle|
|8004F934|Each wait shows its current candidate before selecting another|0 /41|39574,7A538;78-frame host check|Shared RNG history and live timing|
|80030430|Modal geometry initialization|0 /213|30784,30C0C; original-MIPS Help fixtures|Other modal kinds and GPU ownership|
|80030784|Growth before drawing; terminal full box|0 /150|30430,30C0C;342 modal cases|Full primitive lifecycle|
|800309DC|Shrink before drawing; terminal hidden frame|0 /140|30C0C,40A1C;342 modal cases|Full primitive lifecycle|
|80030C0C|Geometry update and modal submission|0 /52|30784,309DC;10,854 frame comparisons|GPU commands/pixels|
|80040A1C|Text creation/removal and barriers around Help|0 /364|30C0C,3B194; Team Select kind-zero route|Other dialog choices and text allocation|
|8003B194|Mandatory frame before changed-input test|0 /21|39574; Help and existing barrier probes|Physical masks and driver refresh|

Team Select and frontend-input ledger totals are unchanged. Help helpers are
shared with prior frontend recovery, not re-credited here. Source hashes and
full function extents are retained privately in
`.local/verification/team_select/audit_a/arrow_motion/function_inventory.json`.

## Verification and limits

- Public `frontend_help_presentation_test.cpp`:310 assertions for geometry,
  visible snapshots, input eligibility, held/change/return barriers and guards.
- Private actual-MIPS versus compiled C:33,726 assertions across342 complete
  modal cases and10,854 visible frames. Font/GPU/pad boundaries are controlled.
- Private extracted actual host:15 phase cases/204 assertions, including all78
  random owner frames, callback/Help ordering, repaint gates and exit cleanup.
  The renderer records arguments; this is not original pixel or OS evidence.
- Combined capture suite:98 scenarios, repeated in Debug and RelWithDebInfo and
  equal across configurations. Localized pixel checks require the old logo on
  the poll frame, new logo on the first wait, terminal growth without text and
  retained acknowledgment text until the first shrink. Counts and shown state
  are constrained independently; a private verifier check accepts the valid
  capture and rejects eight altered metadata cases.
- 44/44 CTest tests pass in both configurations. Create Player retains27/27
  repeated scenarios,753/753 projected vertices,251 primary packet/order records
  and zero missing sampled texels. Real save/config bytes and timestamps are
  unchanged. Existing metadata checks retain their previous credit.

Private final runs: Team Select Debug `run-20260830-190353-43889099`, release
`run-20260830-190432-243cdebd`; Create Player `run-20260830-190432`. Build/test logs
use `.local/logs/team_frame_phase_*`. Original Help comparisons are in
`audit_a/arrow_motion/help_native_results.json`; host extraction and verifier
mutation results are in `audit_c/visual_phase_probe/`, under the private Team
Select verification root. Original bytes, frames and fixtures remain ignored.

The arrow/motion audit additionally passes19,733 controlled source assertions,
but does not prove the first arrow's stored tint color. Ordinary Setup's nine
type40 cards and four disabled arrows allocate no text in26 source assertions;
other upstream producers and Team Select's pre-arrow allocations still need a
complete history or an actual valid `3D51C` node capture. Do not seed arrows from
an unrelated cold free-slot snapshot.

A separate audio audit found three source-proven scalar mismatches for the next
bounded checkpoint: accepted unmuted cursor cues consume one shared six-word
`7A538` draw; `72048` uses integer pitch quantization; and `76334` truncates an
intermediate7-bit gain. Bank/program/tone and compressed intervals match source.
Those scalar differences are not fixed by this presentation change. Original
voice allocation, full RNG history, SPU waveform and live cue timing remain
pending, along with the synchronized physical Team Select walkthrough.

Subsequent cursor-audio checkpoint: the three scalar/RNG differences are now
corrected in the bounded native path. See cursor_audio_workflow.md; the original
hardware, allocation and full-history limits above still apply.

Subsequent text-placement checkpoint: separate labels/values and all four arrow
poses now survive callbacks and advance at the existing presentation boundary.
The full state3 caller has two graphic descriptors, so it bypasses the selected
text-head wait even when that head has pending movement. No additional entry
delay is due. The uncounted native entry preview projects a copy, leaving live
placement pending. See team_select_text_workflow.md for source evidence and the
remaining allocator/first-flash history limit.

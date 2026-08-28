# Trade Players: implementation and verification

Trade now opens from the Rosters card in the native Windows build. Recovered
behavior lives in `src/recovered/roster_trade.c`; C++ owns assets, the window,
child screens, audio and durable storage. No emulator runs inside the port.

**Functional milestone accepted on2026-08-28:** after the original transfer /
discard / reopen comparison and live recording passed, the user requested
completion if the final checks passed. The final Trade suite passed48 host
checkpoints and6,525 real-data slot pairs; Windows CTest passed22/22. This closes
the requested working Trade milestone, not an original-fidelity or full-function
instruction-matching claim. The follow-ups below remain explicitly uncredited.

### Completion audit

| Requirement | Inspected evidence |
|---|---|
| Original screen/assets and menu entry | Original Trade layout/CLUT/font/portrait packs; live Start -> Rosters -> Trade route; private matched-state screenshots |
| Team/player navigation, pick/cancel, validation and mutation | Recovered C callbacks and shared mutation/compaction;11 controller groups;48 host checkpoints;6,525 exact-outcome slot pairs; fresh live Down/C/X validator |
| Help/View/Compare returns | Both original Help pages; original View/Compare keep/ignore captures; host child ownership, release barriers and12 undo-checkpoint combinations |
| Native saves/discard/Reset | Host commit/reload, failure/retry and post-replacement sync-warning checks; original later-transfer discard/reopen retains earlier swap; isolated Reset restores defaults |
| CLI, bounded accounting and regression workflow | Live cue/selection/route logs;39/39 entry-wrapper ledger only; Trade script, Windows tests, synthetic live-evidence tests and CI integration |
| Original comparison and honest limitations | Private original screenshots and side-by-sides; captured native app-only audio; explicit unresolved timing/rendering/held-input and waveform limits below |

## What works, and what the numbers mean

| Evidence | Current scope | Not a claim of |
|---|---|---|
| C controller tests | 11 groups, including all225 synthetic slot pairs, selector sound/no-op routing and12 original undo-quirk return combinations | Original execution equivalence |
| Private host regression | 48 captured checkpoints; handlers, assets, child keep/ignore/cancel returns, original undo-quirk exits, empty-slot transfer/discard/reopen, save-failure/retry, post-replacement sync warnings, restart, discard/decline, Reset | Live Windows input or exact original timing |
| Live native smoke test | Keyboard Start -> Rosters -> Trade; fresh Down/pick/cancel validator passes on1016 private frames with app-only audio | Live mutation/save coverage, right-team timeline metadata, full-roster or original timing equivalence |
| Real database matrix | 6,525 cases:225 slot pairs on29 adjacent-team pairs; exact outcome/phase, moved identities, counts, snapshot and untouched-roster checks | All29x28 team combinations or every gameplay context |
| Pack tests | 2 C++ parser groups;5 Python extraction cases | Asset redistribution or screenshot fidelity |
| [Instruction ledger](trade_rosters_progress.md) | 39/39 entry-wrapper instructions source-accounted;10 native entry/contract scenarios | Percent of Trade complete or callee-body completion |
| Original reference | Charlotte/Seattle entry, both Help pages, View/Compare returns, occupied swap, empty-slot transfer/discard/reopen and post-View cancel/reopen retained ownership captured | Matched audio/video sequence; all hidden roster state or every return/transfer case; pixel equivalence |

These are separate measurements. Working code earns no automatic whole-block
credit, and native tests are not a MIPS differential oracle. Run the scripts
again for fresh results rather than treating this table as a live test run.

## Recovered contracts and discoveries

| Original owner | Instructions in inspected function | Native implementation / boundary |
|---|---:|---|
| `80056CD0` entry | 39 | Right-then-left normalization; LEFT collision repair; actual kind1/1 callback bindings and signed selector-return transport. All39 wrapper instructions source-accounted through explicit callee contracts. |
| `80056494` shared constructor | 276 | Two kind1 lists,535-slot snapshot,30 rows,6 visible per side. Trade graphics/controller state13; Re-order specialization remains separate. |
| `80056B44` first callback | 67 | Injury gate before first selection; Trade permits an empty first slot. |
| `80056C50` second callback | 32 | Real function starts atC50, notC58. Read-only Ghidra disassembly recovered the missing entry. Delegates to569BC. |
| `80055EF0` scan | 115 | Independent active-list scan; wrap0..28, skip opposite team, preserve cursor/top. Second-stage Trade can change teams. |
| `8005A3FC` View return | 79 | Changed selection offers original keep/ignore dialog; Cancel and free-agent selection never write back. Reentry separates team collisions. |
| `8005A6F0` Compare return | 100 | Changed distinct-team pair offers keep/ignore; accepted pair restores both cursors/top. |

The six non-wrapper rows are **inspected dependencies, not newly credited
instructions** and are not silently added to the39-instruction denominator.
Shared helpers568E4/569BC/556B0/558E0/55AF8 perform selection, validation,
mutation/compaction and refresh. Re-order's membership-preserving policy is
unchanged; Trade owns a separate cross-team draft.

The constructor now binds two typed native input callbacks, and second-stage
validation consumes its two kind1 fields. Selector results remain signed:
1 accepts,-1 cancels,2 opens View,3 opens Compare;0 means the native frame pump
is still running. Host child routing and exit checks consume these values;
save failure restores the previous running state. Tests cover all signed
halfword values as a transport contract, not as reachable game outcomes.
The frame pump replaces the original blocking call; its allocation, return
timing and post-child transaction behavior are not credited callee bodies.

Primary evidence is private `recompiled_full.cpp`, backed by read-only Ghidra
exports under `.local/ghidra/trade_*_20260828.*`. The callback preparation script
creates transient function boundaries in a **read-only** project session.

## Assets and rendering

- Layout`80093330[13] -> 80096D24`: original ZSET4`ba38` at155,10, depth3;
  portrait frames30/368,15; Z2PORT portraits54/386,22,87x51; original frame pieces.
- Original two CLUT halves follow separate teams; original ZFONT0 row glyphs
  and ZFONT1 headers/modal glyphs. Row baselines112+16*n, x60/270. Natural
  lowercase positions, jersey sentinel handling and surnames come from the database.
- Empty rows use private string`80024E60` at offset45, from3CD74..3CD90.
- `tools/extract_trade_assets.py` extracts21 bounded UI/preference records
  (938 bytes on this disc) and4 Help descriptors (764 bytes) into ignored
  `.local/assetpacks/trade/`. No replacement artwork or captured-screen rendering.
- Empty-slot notice AFC22 substitutes the original action strings from
  `800264EC` (View) or `800264F8` (Compare), selected by callback `80054B94`.
  Parser tests require the substitution, reject a missing subject, and host
  checkpoints exercise both notices without opening a child or changing the draft.
- Help13 pages0/1 useB10D0/B11AC; child Help35/36 usesB2194/B22F0.
  Original green modal grows/shrinks and has a release barrier. Keep/discard
  dialogs use original red descriptorsAEE88/AEEF6/AF4F8.
- Existing recovered sound bank supplies direction, select, reject, Help and
  confirmation cues. CLI identifies cue ID/rate/gain. Original Trade-specific
  waveform/timing comparison is still pending.

## Save design and safety

The editor holds three fixed535-slot arrays (durable baseline, original undo
checkpoint and working), totaling3,210 bytes of slot IDs, plus counts,
cursor/tint state and borrowed immutable player metadata. No player biography,
font, image or audio is copied into a save. The existing versioned N97ROST v1
codec stores sparse slot overrides with base identity and checksums; see
[save format](roster_save_format.md). Cross-team swaps need no new format.

Post-trade View/Compare host checkpoints explicitly check the updated draft's
team ownership and selected player IDs. Returning from either child must not
mutate working slots, publish the live roster or increment save generation.
This is native draft isolation evidence; the intentionally preserved original
undo-checkpoint quirk is documented below.

Original `view-after-trade.png` now confirms Jim McIlvaine on Charlotte after
the Mason/McIlvaine swap: jersey22, starting PF (the destination lineup slot),
Charlotte team strip/name, and season values80/6/182/2.2/1195/14.9. The matching
native checkpoint agrees on those visible fields and portrait identity.
`view-after-trade-side-by-side.png` remains private. Framing, brightness and
independent title animation still differ; this is visible draft ownership
evidence, not completed child-return/rollback evidence or pixel parity.

Each successful mutation validates population/contiguity/ownership before
being shown; failure rolls back the draft. View/Compare see an immutable draft
projection, not the accepted save. Enter from first selection atomically commits
then publishes. Precommit failure retains the exact draft; a postcommit sync
warning must not retry an already published edit. External baseline changes
fail closed. Cancel restores the latest original undo checkpoint, then commits
any retained pre-child changes on exit. Reset durably restores original slots.

Native policy: dirty is a full-table comparison, not solely the original16-bit
change counter (which can wrap). The host currently supplies normal frontend
mode0/no injury context; synthetic injury/minimum tests do not implement a season.
Initial remembered native teams are Charlotte/Seattle; this is not a claim of
recovering all original game-setup context defaults.

### Intentional original-game bug: child visits renew the undo checkpoint

Original constructor `80056494` creates
a fresh535-slot snapshot and zeroes its local change counter each time it runs.
The frontend dispatches child returns back through the parent constructor.
The original execution test now confirms the changed exit path: after the
Mason/McIlvaine swap and View Player visit, the user pressed Right Shift to
return, then Right Shift again. The game returned to the Rosters card menu
without another discard prompt. Screenshot `post-view-cancel-menu.png` records
the final screen (Reset is enabled); the absence of an intermediate prompt
is the user's report, not something proved by a still frame. Reopening Trade
then showed McIlvaine still on Charlotte and Mason still on Seattle in
`reopened-after-view-cancel.png`. This confirms retained visible ownership.
The user explicitly requested that this original bug remain in the port.

The native implementation now distinguishes the original
undo checkpoint (rebased on child re-entry) from the durable-save baseline
(unchanged until leaving Trade). Cancel rolls back only edits after the undo
checkpoint; any earlier retained edits must be committed once on exit, with
the same failure/retry/sync-warning safeguards as Start. Child entry/return
never autosaves. `selector_result=-1` remains the original cancel route even
when retained earlier changes require a native persistence commit. A comment
at `nba97_trade_return_child` explicitly warns not to "fix" this quirk.

The C regression tests12 combinations: View/Compare, first/second selection,
and Select/ignore/adopt returns. Each checks retained earlier edits, new edits
after the checkpoint, decline/confirm discard, count restoration and unchanged
durable baseline. Four host routes additionally exercise successful cancel
publication, a failed cancel save followed by retry, post-replacement sync
uncertainty with signed cancel result, restart and Reset. Help is explicitly
checked not to renew the checkpoint because it does not re-enter the constructor.
Compare and second-stage quirk coverage is source-backed/native-tested; the
original live quirk capture so far is the first-stage View path only.

## Repeatable checks

```powershell
./scripts/verify_trade_rosters.ps1 -RequireAssets
# Existing build, clearly marked as not proving build freshness:
./scripts/verify_trade_rosters.ps1 -SkipBuild -RequireAssets
```

The host script reserves a fresh private directory and passes `--capture-trade`.
That mode explicitly skips the active save and creates its own store. It tests
both empty-slot notices, both Help pages, second-team scan/cancel, child team/layer
navigation, keep confirmations, swap, injected partial-write failure, retry,
fresh store load, discard and Reset. It records source/executable/frame hashes
in `.local/reports/trade_screen_run.json`. Main save/profiles/assets stay untouched.
CI runs only synthetic extraction/parser/controller tests, without game data.

### Live input smoke test and recorder correction

The August28 live run used an exclusive `live-ed2a40024afc4321a73398a0d946d536`
directory under private Trade verification for settings, profiles, roster saves
and logs. The app was launched normally, intro skipped with Space, title entered
with Start, then Rosters reached with Down/Right/Right/Start. Trade was entered
through its card, not a capture-mode shortcut. Actual injected Down/C/X inputs
moved to Mason, selected McIlvaine as the replacement, then returned to FIRST
without mutation. CLI records the12-vblank card flash and source cues4/6/10.
The window was closed after recording; no normal user save was used.

The passive recording has1132 valid512x240 frames,18 input events and a31.507s
span. It includes the card entry and selection/cancel actions, but no audio.
Inspection exposed stale state metadata: `nativeRecordState` unconditionally
read Re-order even on Trade, and the Help-event slot hash had the same owner bug.
**Do not use that initial recording's team/phase/cursor/player or slot-hash
metadata as Trade evidence.** Pixels, input events and the separate Trade CLI
trace are retained without rewriting the original capture.

The host now selects Trade's parent state and full working draft for recording.
Every one of the48 host checkpoints asserts the recorded team/phase/child/Help,
both cursors/viewports/player IDs and independently encoded535-slot hash.
`TRADE-RECORD-VERIFY` is required by the host reporting gate. This verifies the
corrected producer at handler checkpoints; the fresh live check below also
exercises selection/cancel. V1 has only one team field (Trade's LEFT team); the right team
and counts remain in CLI logs. Child states represent the suspended parent,
not the child's current browsed pair. Existing Re-order-only Help inspectors
must not award Trade round-trip credit without a Trade-specific validator.

The fresh `live-fixed-250c572d514c4348a01c517d92171e99/frames` capture passes
`tools/inspect_trade_recording.py`:1016 presentations over26.853s, with Down at
frame332, C at550 and X at755. The script validates complete PPM/input/audio
artifacts, then requires settled Trade FIRST, occupied first-row selections,
exact Down/C/X presses and releases, and unchanged state between the expected
row/phase transitions. Five synthetic regression groups reject stale metadata,
wrong screens, changed opposite selections, missing/extra/unreleased keys,
invalid boundaries and repeat presses. The test is part of the Trade script.
It does not infer full roster conservation from selection metadata or claim
right-team/child-browsing coverage. The live test never entered a mutation/save.

That recording also contains the native process mix (PID21860),1,291,200 sample
frames in2,690 packets. The audio inspector reports nonzero PCM and coverage of
all video boundaries, but unavailable device positions prevent proving sample
continuity; A/V latency and original waveform parity remain unverified. No
microphone or system-wide capture was used. Re-run the bounded check with:

```powershell
python tools/inspect_trade_recording.py .local/verification/trade/live-fixed-250c572d514c4348a01c517d92171e99/frames
```

The post-replacement sync-failure checkpoint additionally proves the new roster
and one generation increment are already published, a fresh store reads the
same slots/generation, and acknowledging the warning exits without retrying.
This exercises the actual host notice/exit path, beyond store-level unit tests.

The continued-reference fixture starts from the retained Mason/McIlvaine trade,
opens Compare on Divac/Mason, then selects the last empty Charlotte slot and
transfers Mason from Seattle. Six host frames cover Compare, empty receiver,
second selection, transfer result, discard prompt and completed discard/reopen. Assertions require exact
membership, counts+1/-1, no premature save, and restoration of the retained
roster after discard. Original captures now corroborate visible selection and
transfer and completed discard/reopen outcomes below.

Original `compare-retained.png` and its private side-by-side now confirm the
continued fixture's Compare entry: Divac/Charlotte versus Mason/Seattle,
jerseys12/14, both starting C (lineup slots), portraits, and visible season
values79/79/1020/12.9/2470 versus82/82/1196/14.5/3457. Selector arrows and
team labels are present on both. Known framing/brightness and independent
title-phase differences remain. No transfer execution is credited from this
Compare screenshot.

Original `transfer-receiver-empty.png` confirms the next input state: the
last empty Charlotte slot is selected, with Burrell/Goldwire/Lohaus/Addison
and two empty rows visible, up-arrow only on the left, Mason selected on the
right, two empty slots per team, and the original no-portrait placeholder.
The private `transfer-empty-side-by-side.png` agrees on these fields. This
also confirms the Compare-to-Trade return kept the retained roster before
the user scrolled down. Transfer execution and its audio are separate checks.

Original `transfer-second.png` and its inspected side-by-side confirm second
selection: Seattle's header is active/yellow, Mason is selected, HELP2 is shown,
and Charlotte's empty receiver/viewport and2/2 empty counts are retained.
After the user's next C press, `transfer-complete.png` and its inspected
`transfer-complete-side-by-side.png` corroborate the native outcome: Mason is
appended after Addison on Charlotte (jersey14, PF), Seattle compacts to Perkins,
Kemp, Schrempf, Hawkins, Payton and Ehlo in the visible rows, and empty counts
become1/3. Charlotte is active again with HELP1, its final empty slot still
selected/no portrait, and Perkins' portrait appears on the right. The native
checkpoint's empty-row highlight is yellow while the original still is white;
the unsynchronized captures do not establish highlight timing equivalence.
Framing and brightness differences remain. These observations corroborate one
visible transfer, not all535 hidden slots, original persistence or rollback.
Original `transfer-discard.png` now confirms Right Shift opens the red discard
dialog over the transferred roster, defaulting to "don't exit". The inspected
side-by-side agrees on wording, choices and retained visible ownership. Native
framing/gradient and unsynchronized choice-highlight tint remain different.
The user then chose "cancel and exit" and reopened Trade. Original
`transfer-discard-reopened.png` and its inspected native post-discard/reopen
side-by-side confirm Mason is back on Seattle, McIlvaine remains on Charlotte,
empty counts return to2/2, Divac is selected, both lists resume at their first
row, and HELP1 is shown. Thus this later transfer is visibly undone while the
earlier retained swap survives. The host checkpoint separately asserts the
complete535-slot retained table, counts and cursors; the original screenshot
does not establish all hidden slots or memory-card/disk persistence.

The August28 stock-database matrix produced4,517 occupied swaps,1,826 transfers,
182 both-empty no-ops and0 minimum-eight rejections. Minimum rejection therefore
has synthetic controller evidence, not coverage from this stock-data matrix.
Successful outcomes require the expected team membership/counts and FIRST phase;
rejections require unchanged slots/counts and SECOND phase. Unrelated teams and
the535-slot snapshot must remain unchanged in every case. Conservation alone
would not catch an implementation that silently did nothing.
Five synthetic host-report tests reject missing evidence markers, duplicate
outcome summaries, wrong totals, absent success classes and incorrect selector
cue routing. This verifies the
reporting gate, not original-game behavior.

### Selector sound routing

The recovered `8003D930` selector writes sound IDs to its `+1B` latch before
calling the row callbacks: up3, down4, left2, right1, pick/View/Compare6.
The callback can suppress the cue for a refused action. `80055314` overrides
replacement cancellation to10 after its input-change barrier. The native Trade
host previously swapped the row/team cue families and used8 for this cancel;
it now consumes `nba97_trade_event_sound` and logs `TRADE-CUE` with event,
raw input and cue. Help, warning dialogs and final exit own separate sounds.
Pure-C tests cover both list phases, boundaries, child/barrier no-ops and
rejected scans. The host verifier independently requires all nine accepted
event/input mappings in actual dispatch logs and rejects unexpected cues.
This establishes source-backed cue selection, not original waveform parity.

### Reference capture conditions

Use [the verified no$psx mappings](nopsx_controls.md), not the native bindings.
On August28, Right Shift (Select) opened the original dirty-Trade red discard
modal after the Mason/McIlvaine swap. Its screenshot is private
`original-20260828/discard-prompt.png`; the visible choices are "cancel and exit"
and "don't exit". This proves prompt entry, not completed rollback.
The matching native `discard-after-trade` checkpoint uses the same traded
pair and viewport. The private `discard-prompt-side-by-side.png` was inspected:
wording, line breaks, choice order and visible player ownership agree. Native
vertical framing and darker two-triangle gradients remain visibly different
under the reference settings below; no pixel-parity claim follows. The host
also checks declining the prompt leaves the full draft and live roster intact.

The August28 original reference was captured with no$psx settings
`Video Brightness = Bright`, `Render Quads as = Quads (better than real)`,
`Video Output = Auto`, and `Game Screen Filter = None (fast)`. These settings
confound pixel/color comparison: an enhanced quad gradient is not the native
two-triangle gradient. Do not tune recovered colors to these screenshots.
The original green modal corner RGB values recovered from `80030440` remain
TL/BR `(10,20,10)` and TR/BL `(0,150,0)`. No emulator preferences were changed.
Neutral brightness and hardware-like polygon rendering must be recorded for
a later controlled comparison; exact rasterization/quantization also remain open.

Private entry/Help captures and labeled side-by-sides remain under
`.local/verification/trade/`. The two Help pages agree on visible wording,
icon sequence and phase-specific team-header colors. Framing, brightness,
gradient shape and unsynchronized title phase remain distinct observations,
not a visual similarity percentage.

Replacement cancellation returned to Charlotte/HELP1 with Divac still on the
left and McIlvaine on the right. The native matched-state checkpoint additionally
asserts both cursors/teams and the entire draft/live table are unchanged; the
original screenshot establishes visible identity/state only, not all535 slots.
View opened on Divac with matching visible name, jersey, position, six stat values,
portrait and vertical team slice. Framing/color differences remain as above.
The original View return dialog was captured on Anthony Mason: default choice
is ignore changes. Confirming that default returned to Divac/Charlotte/HELP1.
Native host checks now exercise both ignore and keep, plus unmodified Compare
cancel, rather than inferring those paths from a successful adoption alone.
Original Compare also matched the initial Divac/McIlvaine pair, plural keep
dialog/default ignore, and accepted Mason/McIlvaine return. The left list resumed
with Mason at its top and an up-arrow, matching the native cursor1/top1 checkpoint.
An actual original Mason/McIlvaine swap was then captured and compared to the
native `traded` checkpoint: identities and portraits exchange sides, natural
position/jersey labels follow each player, empty counts remain2/2, and the left
list retains its top1 viewport. This proves that visible occupied-swap scenario,
not every original transfer, save or cancellation path.

A45-second no$psx-only process-mix capture is retained privately. Its packet/WAV
validation passed, with no microphone or system-wide fallback. No trade action
was verified during that recording, so it is not credited as selection-sound
or original/native waveform parity. Device sample positions were unavailable;
packet timestamps alone do not establish sample continuity.

A second bounded75-second process-only recording, `transfer-selection-audio-01`,
completed during preparation of the empty-slot reference. The last inspected
original frame still showed first selection; no selection action was verified
within that recording. It must not be credited as the first-pick sound or
waveform parity. Future cue capture needs an observed action within its bounds.

## Fidelity and broader-coverage follow-ups (not completion credit)

1. The first-stage View undo quirk is preserved and has original cancel/reopen
   evidence. Extend original checks to Compare/second-stage checkpoint returns,
   and additional transfers (one empty-slot transfer and its completed
   discard/reopen are visibly corroborated above); injected keys still do not reliably reach this instance,
   so the user supplies original inputs while screenshots are captured.
2. Capture matched animation/audio sequences. Existing title/RNG and menu-cue
   services are reused; exact seed history, selector pacing/flash and transition
   timing are not proven by still frames or sound-ID logs.
3. Resolve presentation framing before numerical image comparison: earlier
   no$psx evidence is512x224, native buffer512x240. Do not tune coordinates or
   crop/search offsets to inflate a score. The labeled side-by-side uses a
   manual window-client crop for inspection only, no fidelity percentage.
4. The39-instruction wrapper audit is closed through scoped native contracts.
   Extend constructor/callback body accounting only after explicit per-block
   evidence; no callee-body credit follows automatically from these contracts.
   Keep original instruction bytes/decompiled code private.
5. Add broader nonadjacent-team, repeated/mixed transactions and live-input
   pacing scenarios as gaps are found. Special-season contexts remain outside
   the native frontend currently implemented.
6. Parent Trade currently dispatches one action per fresh Windows key-down;
   Windows autorepeat is suppressed and `nba97_trade_frame` only advances
   tint/release state. The recovered `8003D930` loop's `8003AE4C` poll and
   `80039574` delay path are not integrated as held-parent navigation here.
   The three-press live scenario does not cover holding a direction. Recover
   and test that behavior separately; do not call it exact input fidelity.

The requested functional milestone is complete under the user's final
acceptance instruction. These follow-ups must remain visible and must not be
silently counted as implemented or original-verified.

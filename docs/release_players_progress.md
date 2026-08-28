# Release Players: implementation and acceptance

Updated 2026-08-28. No persistent goal was created.

Release is wired through the native menu, with the original local assets, single-stage
release, donor navigation, injury/minimum/full-pool refusals, Help, View, Compare,
accept/discard, failure-safe saves, restart and Reset. No game assets are published.

Accepted as **functionally complete** after the manual reference round trips.
Presentation-fidelity follow-ups below remain open; this is not 100% equivalence.

## Bounded instruction accounting

| Owner | Original entry | Accounted | Pending | Total |
|---|---|---:|---:|---:|
| Entry / first vacancy / constructor | `8005721C` | 53 | 0 | 53 |
| Input / refusal precedence / release | `80057084` | 102 | 0 | 102 |
| Receiver cursor / conditional scroll | `80056FF4` | 36 | 0 | 36 |
| Release availability | `80057B6C` | 17 | 0 | 17 |
| **Direct source scope: 4 bodies, 35 blocks** | | **208** | **0** | **208** |

This is reviewed **source-contract accounting**, not binary/CFG matching or a
100% fidelity claim. Original NOPs/delay slots count once; native ABI/state replace
register/stack mechanics. Shared engine, rendering, mutation and save bodies are
not counted again. Exact intermediate animation and audio remain separate below.

The primary evidence is the private FEONLY recomp. A fresh read-only Ghidra export
confirms all four bodies and blocks. The true callback starts at `80057084`;
the recomp fragment `8005708C` omits its first two input loads. Entry is split at
`80057224`; both fragments belong to the same 53-instruction wrapper.
Per-block contracts and named native tests live in
[release_players.json](../config/decomp/release_players.json).

## Important recovered behavior

- Frontend state **17 / 0x11**, not15. Dispatcher `8003F7C8` calls `8005721C`.
- Entry scans the **first** free-agent sentinel, not the occupied count.
  Receiver top is clamp(vacancy - 4, 0, 94); donor15/free100, kinds1/0,
  first callback57084 and NULL second callback. Receiver is passive.
- Availability checks exactly `table[534] == 0xffff`, after the mode2 restriction.
  It is not a donor-minimum check or a general vacancy count.
- Confirm on an empty donor is silent. Otherwise refusal order is injury
  (nonzero mode AND injuries enabled AND injured), **exactly8** donor players,
  then **exactly100** free agents. Preserve equality, not invented <= / >= rules.
  Notice descriptors: `800AEBEA`, `800AEB54`, `800AEC1E`.
- Success delegates to `558E0` transfer, `555F4/5539C` compaction/starter repair,
  `55AF8(2)` refresh and `56FF4` cursor advance. Original starter repair uses
  the already-decremented donor count; do not silently extend its bench search.
- Receiver scrolls when cursor-top >=4, except top94. Native continuation preserves
  the nine-frame wait and restores the receiver before increment. Final cursor
  99->100 is kept as a sentinel without indexing beyond the100-slot array.
  The original unbounded full-pool re-entry scan is not reproduced as unsafe memory.
- Refresh happens **before** cursor advance: the released player's portrait remains
  on the right after release. Original no$psx screenshot shows Longley retained
  there while the receiver has moved past him.
- **Compare starts both identities on the selected donor.** `8005A074` does not
  increment the source-side index for parents16/17. Original no$psx confirmed
  Robert Parish on both sides after entering Compare from Release.
- Original Enter from Compare returns to Release with Parish/donor selection
  unchanged and resets the right portrait to the empty-slot asset. Captured as
  `compare-return.png`; this confirms receiver re-entry, not Cancel retention.
- After that Compare return, the user pressed Right Shift (Select/Cancel) and
  reported no prompt; the next capture confirms Rosters with Release highlighted
  (`compare-then-cancel-menu.png`). On reopening with C, Longley remains a free
  agent, Chicago still has one vacancy, and Parish is selected. Captured as
  `compare-cancel-reopen-longley-retained.png`: the Compare/Cancel retention
  round trip is confirmed in the original session, not an emulator restart or
  a memory-card save/load test.
- Child cursor/team adoption requires parent13 (Trade). Release does not adopt.
  Shared constructor re-entry renews undo, but never the durable-save baseline.
  The pre-child-edit retention quirk is preserved and commented. Both Compare
  and View routes now have separate original-runtime retention observations
  and native regression coverage.
- Ordinary dirty-cancel setup: after reopening, the user released Parish with C
  without visiting a child. Original capture `parish-released-before-cancel.png`
  shows Wennington replacing him at starting center, Chicago two vacancies,
  free agents32 vacancies, and Longley/Parish in the receiver list. Parish's
  portrait stays on the right. Right Shift then opens the bounded red discard
  dialog with "cancel and exit" / "don't exit", captured as
  `ordinary-dirty-discard-prompt.png`. This confirms the ordinary dirty path
  prompts, unlike the Compare-return path. After the user chose Up then C
  (cancel and exit), `ordinary-discard-exit-menu.png` confirms return to Rosters.
  Reopening confirms Parish restored at starting center, Longley retained in
  free agents, Chicago one vacancy and free agents33 vacancies. Screenshot:
  `ordinary-discard-reopen-parish-restored.png`. This completes the original
  current-visit rollback check; earlier retained changes are not discarded.
  Native scenario `compare_retained_then_reopen_discard_only_new_release`
  verifies the two-visit sequence and equality of all535 restored slots.
- Ordinary accept setup: after the verified rollback, the user pressed C to
  release Parish again, then Enter/Start without a child visit. Screenshot
  `ordinary-accept-exit-menu.png` confirms return to Rosters. Reopening with C
  confirms Wennington starting, Longley and Parish in free agents, Chicago two
  vacancies and free agents32 vacancies. Screenshot:
  `ordinary-accept-reopen-parish-retained.png`. Ordinary accept/reopen retention
  is confirmed in the original session, not original memory-card persistence.
- View retention setup: from that reopened roster, the user pressed C to release
  Wennington and D to open View Player. Original screenshot
  `view-after-wennington-release-salley.png` shows John Salley, number22,
  starting C for Chicago. This verifies the child opens on the replacement
  starter, not the released player. Enter returns to Release with Salley still
  selected, Wennington following Longley/Parish in free agents, Chicago three
  vacancies and free agents31 vacancies. The right portrait is reset to the
  empty-slot asset (`view-return-wennington-released.png`). The user then
  pressed Right Shift and reported no confirmation; `view-then-cancel-menu.png`
  confirms return to Rosters with Release highlighted. Reopening with C shows
  Salley still starting, Wennington still following Longley/Parish in free agents,
  Chicago three vacancies and free agents31 vacancies. Screenshot:
  `view-cancel-reopen-wennington-retained.png`. This independently confirms the
  View/Cancel/reopen retention quirk in the original session, not original
  memory-card persistence or emulator restart.

## Assets and persistence

The extractor validates all22 records in state17 layout `80097104`, including
`ba23=(140,10)`, `help=(235,217)`, both87x51 Z2PORT portraits, original frames
and indexed backgrounds. Text/fonts come from local packs/ZFONT0/ZFONT1.
Release Help comes from `800B152C`, rect121,80,270,125, seven lines, with View/
Compare child descriptors. It uses the bounded original modal, not a full screen.

Original Help capture `reference-20260828/release-help-open.png` confirms seven
rows in this order: horizontal D-pad/scan teams, vertical D-pad/view team roster,
Cross/select player to release, Square/view current player, Circle/compare,
Start/continue and keep changes, Select/cancel all changes. Visual review against
native `run-5pcyuqpf/host/help-open.png` confirms the same text and icon ordering,
separate icon/text columns and a bounded green panel without keyboard additions.
Different background teams and capture scaling prevent whole-frame equivalence.
The original green panel has stronger diagonal color bands and brighter shading;
the native panel is darker/smoother. This is an observed remaining mismatch, not
an accepted exact match. Recomp `80030430/80030440` and private Ghidra
`reorder_presentation.c` set G4 corner colors; the native renderer explicitly uses
an approximate interpolation. Rasterization/color conversion still needs tracing
before changing shared Help shading. A still image does not verify growth timing.

After Enter closes Help, `reference-20260828/release-help-return-unchanged.png`
shows Release still open with Salley selected as Chicago's starting center,
Chicago three vacancies, free agents31 vacancies, Longley/Parish/Wennington
still visible on the right, and the empty-slot portrait unchanged. The visible
roster/selection and viewport match the pre-Help checkpoint; Enter dismissed Help
without also accepting/exiting Release. This is a screenshot observation, not a
comparison of every roster slot in original RAM.

Private packs: `release/ui.n97trade` (25 records) and `release/help.n97ui`
(three descriptors). No screenshot-as-background or invented notice strings.

Saves reuse the shared versioned `RosterSaveStore`: immutable player catalogue,
535 roster slots, generation/base identity, conflict protection, atomic replacement,
backup/recovery, and Reset. No Release-only format or asset duplication.
One changed-roster test save was **568 bytes**; size is data-dependent.
Drafts publish only on successful exit transactions. Failed writes preserve draft
and live roster. All regression saves are isolated under `.local/verification`.

## Verification

```powershell
./scripts/verify_release_players.ps1 -RequireAssets
python tools/verify_release_players.py --check
python tools/verify_release_screen.py
```

The metadata-only `--check` does not execute tests. The wrapper builds/runs them.

- 15 portable native scenarios, including **8,400** size/slot/free-capacity mutation
  combinations, every entry vacancy, exact refusal precedence, injury modes,
  full-pool sentinel, child re-entry, accept/keep/discard and population conservation.
- **435 real-catalogue donor slots**:362 releases and73 empty no-ops, all checked
  against the original local preference data and whole-population invariant.
- **30 native host/framebuffer checkpoints**: menu route, navigation, bounded Help,
  View/Compare and browsing returns, release, keep/discard, injected save failure/
  retry, restart, minimum refusal, retained pre-child draft and persistent Reset.
- Shared regression protection:24 Windows tests,26 Sign host checkpoints,
  48 Trade host checkpoints plus6,525 real-data Trade pairs.
- Public Python tests reject stale/invalid accounting, bad asset layouts, missing/
  duplicate checkpoints, wrong selector cues, development gates and extra failures.

Fresh hashes/reports: `.local/verification/release-core.json` and
`.local/reports/release_screen_run.json`. Original reference PNGs remain private
in `.local/verification/release/reference-20260828`.
CLI categories include RELEASE-ENTRY, ASSETS, PORTRAIT, CUE, MUTATE, REFUSE, COMMIT,
SAVE-FAILED and CHECKPOINT, plus shared child/confirmation diagnostics.

## Remaining presentation-fidelity follow-ups

1. Compare and View -> Cancel -> reopen retention, ordinary dirty discard and
   ordinary accept/reopen are original-runtime confirmed. Original Release Help
   text, icon ordering and bounded-panel structure are visually checked. Resolve
   the observed green shading/banding mismatch before accepting exact Help visuals.
2. Match intermediate glyph-scroll frames and input/transition cadence. The
   nine-frame continuation is implemented, but the shared native row renderer
   currently jumps to its new top; do not call that original animation equivalence.
3. Capture and compare original release/refusal/scroll audio. Cue IDs are tested;
   waveform identity and scheduling are not established by screenshots.
4. Capture matched team/player states for side-by-side visual review. Current
   original/native screenshots establish layout/identity observations, not a
   numerical screenshot-fidelity score. No$psx injection remains unreliable;
   use manual keys from [nopsx_controls.md](nopsx_controls.md).

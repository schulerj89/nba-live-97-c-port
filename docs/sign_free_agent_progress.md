# Sign Free Agent: bounded implementation and verification

The Sign card now opens a native screen using the original private assets.
This is a C controller with the existing C++ host, not an emulated screen or
a screenshot presented as the game.

## Source accounting

| Owner | Original entry | Accounted | Pending |
| --- | --- | ---: | ---: |
| Entry / destination normalization | `80056F9C` | 22 | 0 |
| First selection | `80056D6C` | 53 | 0 |
| Second selection | `80056E40` | 87 | 0 |
| Menu availability | `80057B00` | 27 | 0 |
| **Bounded total** | **4 owners / 33 blocks** | **189** | **0** |

[The ledger](../config/decomp/sign_free_agent.json) maps disjoint original
instruction ranges to native behavior and regression scenarios. The verifier
checks ownership, bounds, completeness, scenario references and report freshness.
A fresh private Ghidra inventory matches the four bodies and 33 block ranges.
This is reviewed semantic accounting, **not machine-proved instruction
equivalence, completed transitive dependencies, or 100% feature fidelity**.
Shared constructor, selector, mutation, child and save bodies receive no extra
credit in this denominator. The [initial audit](sign_free_agent_audit.md)
preserves the original baseline and dependency list.

## Implemented behavior

- Fixed free-agent team29 with 100 slots; receiver has 15; six visible rows.
  Receiver object offsets start at100. Left/right scans the receiver in either
  phase. Empty source, full receiver and occupied destination have distinct
  original notices, with the original refusal ordering.
- Signing inserts into the compacted receiver roster, compacts the free pool,
  refreshes counts/portraits, and emits cue6 once. The special selector-action2
  branch and the validator's exact `-1` comparison are retained.
- Both original Help panels, View and Compare round trips, and cancellation.
  `8005A3FC` / `8005A6F0` permit child selection adoption only for parent state13
  (Trade), not state14 (Sign). Browsing in a Sign child therefore does not offer
  Trade's adoption dialog. On re-entry, dispatcher `80040154` passes the saved
  **left** team29 to Sign's entry, not the receiver. Normalization changes the
  receiver to Chicago3 (or the context team in mode2), while preserving the
  saved cursors and selection phase (`56254` / `560BC`). This original quirk is
  commented and tested; the first-phase View return was observed in no$psx.
  Returning through the Rosters menu instead resets the list cursors: `57CE4`
  writes both sentinel fields, and `56494` initializes their first rows.
- The shared constructor re-entry undo checkpoint is preserved and commented:
  pre-child changes become the new undo baseline. Original Sign evidence now
  confirms signing Adams, viewing Alston, returning, cancelling without a
  discard prompt, then reopening with34 empty free slots and Alston first.
  The native regression checks the retained change survives a new save-store
  load, not just a UI re-entry. A later Reset restores the private defaults.
- Normal/special-context availability preserves the restriction byte, first
  free-slot sentinel and vacancy count. The high-level meaning of the special
  restriction byte is unresolved; it is not assigned an invented season label.

## Assets, saves and logging

`extract_assetpacks.ps1` creates separate ignored Sign UI and Help packs
(`ui.n97trade`: 1,232 bytes; `help.n97ui`: 786 bytes for the audited US overlay).
Title `ba30`, original fonts, portraits, backgrounds, palette records and menu
audio use the existing local packs. View Player uses the original `xfrZ`
free-agent strip (39x156 at296,35), not the unrelated `xeaP` palette identity.
The host regression checks all5,669 opaque strip pixels against the private
decoded asset when Help is not covering it. Parsers validate bounds, required routes
and screen identity. No game assets are tracked.

Signing uses the shared [versioned roster save format](roster_save_format.md):
isolated 535-slot draft, original-data identity, validated ownership/counts,
transactional commit and restart loading. It does not create a second database
or change private base assets. Reset restores original team and free-agent slots.
CLI includes `SIGN`, `SIGN-CUE`, `SIGN-AVAILABILITY`, `SIGN-SAVE-FAILED`, and
`SIGN-COMMIT`, alongside shared portrait/palette/modal logs.

## Repeatable regression workflow

```powershell
./scripts/verify_sign_free_agent.ps1 -RequireAssets
# Optional original inventory check after a fresh private Ghidra export:
python tools/verify_sign_free_agent.py --check --fresh-inventory .local/ghidra/sign_inventory.json
```

Without `-RequireAssets`, tests use synthetic data and do not need a disc.
The private host run creates a new isolated save below `.local/verification/sign/`;
it never reads or overwrites the active roster save. Evidence records executable,
source, input-pack, trace and framebuffer hashes; failures do not yield a pass.

Verified 2026-08-28:

- 14 named core scenarios, including **43,500 synthetic slot pairs** (29 teams
  × 100 free slots × 15 destination slots), 3,886 successful transfers and
  39,614 refusals, checked against an independent full-table oracle.
- **26 native host checkpoints**: entry, both Help panels, receiver scan,
  free-agent View/Compare, second-stage child browse/return, empty/occupied
  refusals, tail scrolling, signing, save failure/retry, restart, discard, Reset,
  and original Sign/View/Cancel retention and cursor-reset behavior.
- 10 adversarial Python ledger/host-evidence tests; synthetic Sign asset parser
  route/truncation checks and Help-state14 parser coverage.
- 23/23 registered CTest tests. Existing Trade host regression also passes
  48 checkpoints and 6,525 real-database slot pairs after the shared changes.
- Live Windows keyboard smoke test: title Start, menu navigation to Sign,
  and F opening its Help1 modal, with screenshot and CLI trace captured under
  `.local/verification/sign-reference-20260828/`. No roster edit was saved by
  this smoke test. The harness remains the evidence for save/failure/restart.

## Original-reference status and remaining limits

Original Sign Help1/Help2, second-selection and occupied-destination refusal
screenshots were captured privately. The refusal confirms the empty-position
requirement with the extracted `800AED88` text and red warning modal.
The original first vacant Charlotte slot and successful Adams signing were
also captured: Adams occupies that slot, Alston becomes the free list's first
row, vacancies change from33/2 to34/1, the receiver viewport is retained, and
the screen returns to Help1. This agrees with the native transfer/count/phase
contracts; the native tail-slot case separately checks backwards insertion.
Help2 visibly uses the smaller green modal and the same extracted
wording/icons; native frames were inspected. These are independent animation
phases, not synchronized pixel equality. A private side-by-side image is saved
as `.local/verification/sign-reference-20260828/help-second-comparison.png`.
The original is visibly brighter and fills more vertical area in this comparison;
Help background shading also differs. Display cropping, palettes and shading
need separate measurements rather than assuming these are animation differences.
CLI tests check sound IDs, not waveform
or timing equivalence. Exact palette/animation/audio comparison,
last-occupied-source behavior and second-phase/Compare variants of the
child-return quirks still require original reference observations; those variants
have source and native-test evidence, not independent original execution proof.
Original/native Alston frames are also paired privately in
`view-alston-comparison.png` alongside the Help and refusal comparisons.

The emulator accepts physical controls, but supported automated game-key
injection did not register. See the [bounded input diagnosis](nopsx_controls.md#sign-reference-session-input-diagnosis-2026-08-28).
The cause is not proven and no claim of repaired emulator automation is made.
Do not convert these pending reference checks into a guessed completion percent.

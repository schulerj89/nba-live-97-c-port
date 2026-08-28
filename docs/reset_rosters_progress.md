# Reset Rosters: bounded audit

Updated 2026-08-28. No persistent goal was created. This audit does not reset
the user's active native save or claim whole-feature original-game equivalence.
The user-assisted original normal-roster acceptance checkpoints are recorded below.

Normal-roster Reset was already wired during the Re-order persistence work:
original private confirmation/fonts, safe default choice, cancel, confirm,
post-confirmation card flash, transactional save, failure/retry, restart and
relocking. The new work separates that functionality from instruction review.

## Fixed instruction scope

| Owner | Entry | Accounted | Pending | Total |
|---|---|---:|---:|---:|
| Card callback / overflow decision | `80057960` | 0 | 66 | 66 |
| Restore default slots | `80057864` | 0 | 63 | 63 |
| Availability gate | `80057C48` | 20 | 0 | 20 |
| Context-aware default comparison | `80058104` | 32 | 0 | 32 |
| Created-player reconciliation | `800582C4` | 0 | 56 | 56 |
| **Five bodies** | | **52** | **185** | **237** |

These are reviewed source contracts, not matched machine code or a feature
completion percentage. Existing normal restore/callback behavior is not credited
automatically. Shared modal, selector, pointer rebuild, ownership/counts, ranking
and save bodies are outside this denominator. Their absence from the count does
not mean their behavior can be ignored.

Recomp is the primary source. A read-only Ghidra export independently confirms
all five bodies and blocks, including the true callback entry `80057960`.
Its first two input loads were misclassified as data before recomp fragment
`80057968`. `tools/ghidra/PrepareResetCallbackHeadless.py` creates only transient
annotations; the private database is not saved. Private evidence:
`.local/ghidra/reset_audit_20260828.c` and `reset_inventory_20260828.json`.

## Findings and scope limits

- Reset is disabled when the selected roster table equals all 535 original
  slots, or when special-active is nonzero and signed special-kind is exactly1.
  A save file's existence does not enable the card.
- `58104` compares the inline context table for signed frontend states7/27;
  every other state uses the normal table pointer. This choice is now explicit
  in portable C. All65,536 states, all65,536 active/kind byte pairs, every slot,
  equal tables and null guards have regression coverage. The normal frontend
  still calls the normal-table convenience API; this does not implement seasons.
- Raw Cross800 opens descriptor `800AEDD2`: bounded red confirmation,
  rect121,75,270,110, five body lines and two choices, Cancel selected for
  preference0. Text and original ZFONT1 come from local asset packs.
- The confirmed branch flashes the card *after* the first dialog. It counts
  up to40 created records through `4AEBC`; when count+67 exceeds100 (34 or more
  created players), it opens a second descriptor `800AFAAE`. Result2 skips the
  restore/reconciliation calls but still refreshes ownership/counts and requests
  menu update10. The exact second-dialog labels and player ordering require
  further extraction/review; do not invent a deletion policy.
- Default restore copies29x15 team slots and100 free-agent slots, resolves
  team pointers, rebuilds counts/owners and refreshes derived ratings. The native
  store already restores canonical slots and rebuilds derived state safely;
  full original ranking and created-player branches are not claimed complete.
- `582C4` has distinct policies0/1 and calls `58184` to insert created players.
  The port has no created-player catalogue yet. Do not treat dropping those
  records as an acceptable substitute, or publish a full Reset completion claim.

## Verification and persistence

```powershell
./scripts/verify_reset_rosters.ps1
# Optional: all isolated save/reset host scenarios and private image/audio checks
./scripts/verify_reset_rosters.ps1 -RequireAssets
# Quick Release -> Reset only, after building; private assets required
python tools/verify_reset_release_host.py
```

`tools/verify_reset_rosters.py --check` validates metadata only; `--native-test`
executes the portable tests. `--fresh-inventory` compares against the read-only
Ghidra export. The public ledger contains addresses/counts/contracts, not game
bytes, original dialog text, images or audio.

The existing `verify_reorder_save_host.py` passed28 fresh-process scenarios in
this audit, including cancel, reset-confirm/reload/relock, precommit failure and
retry, postcommit sync warning, and unchanged-default locking. It verifies actual
native key handlers and compositor output, primary/backup bytes, generations,
and preservation of active saves, profiles, settings and private input assets.
Its result is `.local/reports/reorder_save_host_run.json`, not original execution.

The short `verify_reset_release_host.py` also passed eight fresh-process cases:
default baseline, one-player Release/Accept, released restart, Reset cancel,
cancelled restart, Reset confirm, restored restart and locked Reset. It uses
actual native editor/confirmation handlers and local asset packs. It checks
player identity/ownership, pool counts, all-slot default restoration, default
Cancel, modal bounds, cue dispatch IDs, save generation/CRC, no Cancel rewrite,
the 68-byte empty override, retained pre-reset backup and unchanged active saves.
Confirmed Reset must focus View Rosters (index4); Cancel must retain Reset
(index3). This agrees with the original reference session below. Report:
`.local/reports/reset_release_host_run.json`. Three synthetic evidence-parser
tests run in CI without assets; private host execution is local only.

This short test advances logical modal ticks and completes the menu flash
offline; it does not measure real-time flash duration, device playback or
original waveform equivalence. It starts from clean defaults instead of the
already-edited original session. Native compositor captures were inspected for
the restored roster and red Reset/View selection, not scored as pixel parity.

Reset writes a versioned68-byte empty override, retaining a pre-reset backup;
it does not delete the primary and accidentally reactivate an older backup.
Profiles, settings and source assets remain unchanged. Failed transactions retain
the accepted roster; a postcommit warning must not replay the reset.

## Original reference checkpoint (2026-08-28)

After the user pressed C once on Reset, the original game displayed a bounded
red confirmation over the roster cards, with Cancel highlighted by default.
The body describes restoring default rosters and moving created players into
the free-agent pool. This agrees with the recovered descriptor and default
choice; it does not yet verify cancellation or restoration. The private capture
is `.local/verification/reset/reference-20260828/01-confirm-cancel-default.png`.
The original modal also has diagonal shading bands; exact native shading remains
unverified, not credited as matching from this observation.

The user then pressed C again on the default Cancel choice. The dialog closed
and Reset remained selected/yellow, while Player Injuries remained red. Capture:
`.local/verification/reset/reference-20260828/02-cancel-return-reset-enabled.png`.
This verifies the cancel return and visible availability, not the exact roster
contents; checking the released players remains a separate checkpoint.

The user reopened Release with Left then C. Capture
`.local/verification/reset/reference-20260828/03-cancel-preserves-releases.png`
shows Chicago with three empty slots and the free-agent pool with 31 empty
slots. Longley, Parish and Wennington are visibly still in the free-agent list;
Salley remains Chicago's starting center. This verifies that cancelling Reset
preserved those three prior releases. Restoration was checked in later captures.

After returning, reopening Reset and pressing Up, the user left the restore
choice highlighted yellow and Cancel white. Private capture:
`.local/verification/reset/reference-20260828/04-confirm-restore-selected.png`.
This establishes navigation from default Cancel to restore, not confirmation,
restoration, animation timing or audio parity.

After the user confirmed with C, the dialog was gone, Reset was red, and the
selection had moved to View Rosters (yellow). Player Injuries remained red.
Private capture:
`.local/verification/reset/reference-20260828/05-confirm-return-reset-red-view-selected.png`.
This establishes the post-confirmation card state and selection fallback; the
restored player contents still require inspection. A settled screenshot does
not establish flash duration or whether any transient intermediate screen ran.

Reopening Release after confirmation shows Chicago full, with Longley restored
to starting center, and the free-agent pool reporting 33 empty slots. Capture:
`.local/verification/reset/reference-20260828/06-restored-chicago-top.png`.
Parish and Wennington are below the visible Chicago rows in this capture.
The pool changed from 31 empty to 33, not 34: this test used
an already-modified original session, so do not characterize Reset as merely
reversing the last three releases or assume a three-slot pool delta. The source
contract is restoration of the entire default table. Exact cause of this
session's net pool delta remains unverified.

Scrolling Chicago shows Parish and Wennington back on its roster, alongside
Buechler, Simpkins, Salley and R. Brown. Capture:
`.local/verification/reset/reference-20260828/07-restored-parish-wennington.png`.
Together with Longley's restored first slot this completes the three-player
normal-roster reference check. No additional emulator inputs are needed for
this checkpoint. Created players, other frontend contexts and exact media
timing are not covered by these seven settled captures.

## Next acceptance checkpoints

1. Normal original cancellation/restoration/relock reference captured; short
   native Release/Reset regression passes. Keep these distinct from full fidelity.
2. Review the66 callback and63 restore instructions against native boundaries.
3. Keep created-player reconciliation/overflow explicitly pending until its
   data model and source dependencies are implemented.
4. Red modal shading, exact flash/growth timing and original waveform comparison
   remain presentation-fidelity work, not inferred from passing state tests.

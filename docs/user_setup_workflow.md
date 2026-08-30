# User Setup: editor and native persistence checkpoint

2026-08-30. The native route now includes assignment, profile editing, both Help
pages, capacity/full/duplicate warnings, delete choices and native profile
transactions. This does not complete Team Select or gameplay. Original runtime,
visual, timing, audio and physical-input comparisons remain pending.

## Recovered behavior

Team Select Start waits for changed input before entering state5 owner80037010.
Its original layout uses ZSET1/ba39, Pal0 frames and cnt3/cnt2/cnt1 controller
markers. Keyboard input represents physical controller0 only; injected topologies
exercise other rows without inventing multiplayer keyboard bindings.

Local sides are0 away,1 neutral,2 home; accepted assignments encode2 away,
0 neutral,1 home. Left/Right clamp. Joining checks exactly five other occupants.
Up/Down cycle FE(None), FF(Start New) and occupied fixed slots. Profile cycling
excludes other active claims; joining and delete-successor selection also inspect
neutral claims. Source80035D80 establishes initial selectors FE. Initial static
assignment bytes are extracted, but their full pre-entry mutation history is
unverified. Later entries retain accepted session assignments.

Start aggregates complete normalized masks across connected controllers. Only
exact80 confirms. A/Z/S/X retain shoulder bits, so Start+shoulder cannot confirm.
Any editor or joined FF blocks readiness; all-neutral and joined FE are allowed.
Select clears selectors/editors and abandons unaccepted sides, retaining accepted
assignments and Team Select focus. Result6 prepares an owned partial match
snapshot and latches Start; it does not launch gameplay.

Cross on FF claims the first eligible empty slot and starts draft A. Existing
profiles copy their exact names. Up/Down cycle the extracted68-character alphabet.
Cross duplicates the current character when appending, only below13 characters
and measured width96. Character replacement trims the tail at width106 while
retaining the edited character. Left/Right move the cursor; Square deletes at it
(one character resets to A); Circle backspaces before it. Start checks exact,
case-sensitive duplicates before asking the host to save. No uppercase-normalizing
profile CRUD is used by this editor.

The source stale existing-editor flag survives new-claim/disconnect paths. New
acceptance clears semantic profile fields only when that flag is zero; existing
acceptance preserves identity, creation time, statistics and all59 control bytes.
The portable editor rejects malformed empty/NUL-cursor drafts. Original fallback
overrun behavior and Square's unused byte14 tail copy are explicitly unclaimed.
Normal bounded logical names match the independent original-MIPS oracle.

## Modal and presentation ownership

recovered/user_setup.c owns state, editor and repeat decisions. UserSetupSession
owns raw masks, fixed profile IDs, source-ordered polling and suspended passes.
UserSetupAssets reads private descriptors, text, sprites, alphabet and preference.
Rendering uses original fonts; only the selected editor character pulses, and
underscore displays as equals at the cursor with unchanged advance.

State5 calls40FCC/40A1C directly and inherits context+724 and+71B rather than
resetting them or storing the invoking key there. The ordinary Team Select
Start path supplies80. A different
held key can therefore dismiss Help/notices after growth without a zero release.
Prior0 is also supported. Only the invoking controller acknowledges the modal.
Notices open with sound5 and close with8. Delete opens12, Up/Down emit3/4, and
Cross confirms after eight presentations with6 then8. Start/Select do not choose
or cancel Delete. Choice0 deletes; the extracted preference0 starts on cancel1.
Choice text positions depend on the initial selection, not later navigation.
Deletion waits for shrink and the final changed-input barrier before mutating.

The first modal presentation advances rectangle growth, matching30C0C/30784.
After a modal returns, the same host update completes the invoking controller's
placement tail, then resumes remaining controllers and
then the ordinary36898 presentation. It does not insert an extra39574 RNG draw.
Other modal updates use39574; ordinary frames use direct32BF0. Absolute original
growth/poll/presentation phase, GPU timing and text-node lifecycle remain pending.

The120Hz clock comes from IRQ6 callback80078628. The controller pass uses signed
wrapping elapsed >6; global Start/Select runs first. Repeat36B80 waits60 then12
ticks and retains history across entry. Disconnected visible controllers clear
when reached; excluded slots clear after the pass. This ordering affects capacity.
Topology-change debounce now follows the source: entry99/-1, exact8000 driver
words, four deferred observations and adoption on the fifth. Observation occurs
before global input and the row clock gate; modal continuations retain the old
row order while connectivity stays live. Entry composition primes one observation
that the first step consumes without querying twice. Rebuild hiding and timed
row restoration now use retained portable placement targets. A hardware change
does not bypass the current row's clock/modal boundary; see
user_setup_placement_workflow.md. Actual original presentation timing remains
pending.

Select stores shared controller8/mask100 and returns to dispatcher3FD10. The host
presents, then waits for the complete physically connected aggregate to differ
from100 before returning to Team Select. It neither polls another row pass nor
adds a second cleanup presentation. A changed nonzero mask may satisfy the
barrier. Global accepted Start leaves shared history unchanged.

## Native save boundary

See user_profile_save_format.md for v2. V1 remains readable; import and no-op
acceptance do not write. Changes migrate to v2, which older writers cannot read.
Fixed slots and deletion holes survive restart. Renames preserve native IDs,
creation times,16 statistic fields,59 controls and the raw validity byte.
These statistics are native fields, not a claimed mapping of all retail bytes.

Acceptance writes the native file before closing the editor. This is a native
persistence policy, not the original memory-card save schedule. A failed write
retains the draft and opens an explicitly native failure notice. Precommit
failure rollback, stale-writer rejection and backup protection are tested;
power-loss and ambiguous OS replacement outcomes remain unverified.

## Verification and accounting

Extract with tools/extract_user_setup.py after Team Select extraction. Run
scripts/verify_team_select.ps1 with fresh isolated settings/profile/created/roster
paths. It rejects unsafe defaults and fingerprints all real save/config files.
The88 capture scenarios include held input, exit barriers, topology changes,
both Help pages, editor controls, duplicate/full/
capacity warnings, failed-save retry, rename/no-op, delete cancel/invalid input/
eight-update delay/final barrier, restart, abandonment, shoulder chords and
owned ordinary-exhibition snapshots with retained controls/current saved rosters.
Twelve controlled-clock placement checkpoints additionally cover hidden entry,
rebuilds, delayed disconnect/reconnect, Help return and retained editor state.
Repeated native frames measure regression stability, not original GPU equivalence.

Public tests cover all6,561 assignments, eligibility, repeat/clock ordering,
editor alphabet and width boundaries, inherited acknowledgments, modal controller
isolation, and manually packed v1/v2 save fixtures. Private independent checks
pass4,554 valid editor cases,17 legacy cases,33 boundary cases and27 malformed
rejections. Profile probes preserve48 statistic values and177 control bytes with
holes1/7/19;24 concurrent-writer trials have no lost updates.

The ledger retains the entire1716-instruction state5 owner and bounded2194 owner/
1270 shared totals. All new instruction credit remains zero pending per-block
review. Missing original evidence is never promoted by a native capture hash.
Current validation results and private run identifiers are recorded in
team_select_workflow.md.

## Next boundary and original probes

The recovered77-instruction61674 finalizer now feeds a semantic owned snapshot,
preserving FE/FF live controls, fixed profile controls, current saved roster order,
ranks and settings. See match_snapshot_workflow.md and gameplay_first_path.md.
Extension/presentation settings, special/created membership, raw unused roster
bytes and the gameplay consumers remain separate boundaries.

Original stops:800373B4 initialization,800374E4 aggregate input,
800375EC/800375F4 readiness,8003768C accepted return. Dump80021EA6 length8,
80021DDE length8,800F93F0 length32,800F96E4 length32,800FF498 length112 and
800D9AB8 length4 from a freshly validated RAM backing. Record one input at a time.

# Re-order Rosters completion and local-save design

Status: **milestone accepted with reference limitations**, 2026-08-28 UTC.
The user accepted the current presentation/audio and requested closure provided
the scoped instruction work was finished. The final rerun confirms875/875
accounted instructions across all ten inventoried owners, zero pending, with
native Re-order and28 isolated save-host scenarios passing. The current screen
gate passes129 checkpoints and the Windows suite passes19/19.

This closes this Re-order goal, not the whole game or full transitive call graph.
Exact original audio/video/timing parity is accepted as an unverified limitation:
the14 paired-reference scenarios are NOT marked passed and no pixel/binary match
is claimed. The strict reference verifier remains strict; its pending result is
separate from this user-accepted milestone. Further capture work is optional
future fidelity work, not a reason to keep this goal running.

The chronological plan/checkpoints below retain earlier pending/active wording
for provenance; this acceptance supersedes that wording for goal closure only.

Implementation checkpoint, 2026-08-27: The bounded save codec is now
implemented and tested, with canonical catalogue identity, immutable defaults
and prepared database publication. Disk transactions, startup load, Accept/error
UI and normal-roster Reset are connected and tested. Help and View have native tests
and captures; Compare now has C navigation, its own asset-backed host screen,
Help, local portrait/field checks and tested draft returns.
Original-reference comparison and the other feature gates remain
open. No original assets or existing saves are changed.

## Goal and scope

Current scroll checkpoint: Compare now stages both stat groups through two
internal presentations, followed by three delay presentations and one input
poll. It retains the14-byte continuation and4-byte pacing state; two additional
15-byte tint states animate the primary vertical markers.114 private captures
include six scroll phases with pixel checks for old/old, new/old, new/new rows.
The controller suite covers236 valid scroll sequences and16 endpoints;
45 host actions cover251 presentations and40 cues. The top-boundary source
question is resolved:5A1EC clears the Up callbacks on descriptors0 and33,
so3D930 never dispatches3AB64 at the top. Its raw-index underflow path is not
reachable through normal Compare input. Top-Up now waits only for the next
input poll, while bottom-Down retains callback delay3 plus poll1.320 held
top-Up polls across both sides/all four layers guard that distinction; the
controller suite now has18 scenarios. This supersedes the260-presentation
expectation, which incorrectly delayed top-Up. Eleven original code blocks
anchor the screen gate. Original-reference credit remains0/14: source-backed
dispatch is not a measured original animation/audio match. The reference
navigation case now explicitly requests top-Up on each side and after returning
to the top, retaining both column indices rather than a single shared summary.

Latest resumed checkpoint: goal status is active. Compare's held left/right
cycling now uses the recovered post-callback delay. Source2C244(copy=2) clears
the selected rebuilt text's geometry-wait bits, resolving the specific animation
dependency that previously kept it disabled.40 host actions check200 completed
presentations; reversal, release, opposing directions, Help and Cancel retain
the draft. The predicate has16,777,216 vectors plus256 rebuild-mask checks.
The normal-mode counter audit corrected an earlier single-pass interpretation:
3AE4C records input in the normal branch AND its common tail. Fresh/reversed
input ends at2; repeats advance4 up to48.150 state vectors and three hashed
original control-flow blocks guard this behavior. The former242-presentation
result encoded the incorrect one-pass model and is superseded, not extra credit.
Compare's source-derived arrow color flash is now connected after callback/sound
dispatch. The private screen gate passes108 frames:85 earlier checkpoints plus
22 arrow phases and one separately settled palette frame, with1892 original-glyph
pixel checks and no outside changes during the arrow test.
Four independent15-byte arrow states preserve fade/hold/return and retrigger
behavior. Native neutral initial color is explicit; original allocator color,
GPU quantization and live timing still require original capture.
This does not close any of the14 paired original-reference cases or add ledger
credit. See the final pacing section of `reorder_child_verification.md` for scope
and remaining animation, polling and original-recording limitations. Windows17/17,
Linux15/15 and the complete local Re-order regression pass at this checkpoint.
Team/layer/side callbacks now wait five completed presentations plus one poll,
including silent no-ops routed through59F20. The existing four-byte pacing state
also drives held callback input. An additional16 host waits check96 presentations,
12 selector cues/four silent no-ops, Help/Cancel and retained draft state. Up/down
geometry/scroll timing is superseded by the checkpoint above; paired original
recordings, raw endpoint behavior and general text-object scheduling remain open.

Finish Help/View/Compare round trips, persistent roster edits/Reset, and matched
original animation/audio/reference checks. Reuse recovered C behavior inside
the C++ platform shell; no runtime PS1 emulation. Keep the existing875/875
ten-owner source-accounting result separate from these unfinished feature gates.

Implement in small, separately verified slices. Before adding instruction credit,
inventory each newly involved original function/callback and its dependencies.
Do not inflate the existing denominator or label port-specific file I/O as
recovered original instructions. No single blended completion percentage.

## Evidence and current gaps

- Title recovery follow-up: `frontend_title_recovery.md` records32BF0/32BF8's
  actual per-corner RNG and alternate-slot logic. Re-order/Compare/Team Rosters/
  View Player now use the recovered C state and one textured quad, removing the
  invented128-pixel split. Native phase/RNG logs and9 dedicated frame checks
  supplement the120 earlier screen checkpoints. Nominal30Hz scheduling, initial
  phase/seed history and native raster precision are not original-reference proof.
  See that document for remaining caller/clock/random-consumer work.
- Original title cadence observation: ten actual no$psx frame steps now retain
  paired debugger/window observations. Pixel equality confirms two complete
  four-step title holds, consistent with the recovered counter/alternation path.
  `inspect_title_frame_steps.py` reproduces this limited observation without
  modifying images or claiming native/audio parity. All14 full paired-reference
  scenarios remain unverified. Original execution was resumed unchanged.
- Original audio capture is now available through a bounded no$psx-only process
  recorder using the same PCM/timestamp path as native capture. Live PID/executable
  validation and process-isolation tests pass; a5-second original idle baseline
  is retained privately. It has no annotated cue or matched video, so it adds no
  paired-reference credit. See `reorder_reference_workflow.md` for the command,
  capture provenance, isolation measurements and remaining game-input limitation.
- Original replacement View observation (2026-08-27): Rodman identity/stat text
  appeared with a "please wait" graphic before his photo/city strip. The settled
  original bitmap and provenance are recorded in `reorder_reference_workflow.md`.
  This exposed and led to removal of synchronous photo loading. The recovered
  C visibility subset now follows310D8/30E78: hide photo on a changed request,
  enable photo/city on successful completion, retain the city during later
  changes. The real `wait` sprite at334,35 remains behind the photograph.
  A bounded native worker decodes local PNGs; stale completions cannot publish
  after browsing/exit/reentry. Regression tests cover these branches and six
  additional loading checkpoints. Original latency/audio parity remains open;
  no simulated CD delay or extra owner-ledger credit is claimed.
- `src/recovered/roster_reorder.c`: child requests already validate identities
  and return original result2 (View) or3 (Compare). View dispatch is wired;
  Compare dispatch is also wired to its own state35 screen.
- `src/win32_main.cpp::handleReorderKey`: D opens View using the isolated draft;
  S opens Compare. Help uses original descriptors for each selection stage and
  for both child screens.
  Accept durably saves a validated candidate before publishing to the live database.
- `beginFrontendTransition`: every Rosters entry currently calls
  the recovered535-slot default-difference predicate. Reset is available after
  accepting an edit and after loading it; successful Reset disables it again.
- `RosterDatabase::applyReorderScreen`:535-slot baseline check, unchanged
  free-agent tail, per-team membership validation, then roster-state candidate
  and derived-index rebuild. Player names/stats/attributes are now immutable and
  shared by draft/accepted candidates; team metadata is still copied with team
  lists. Finish the catalogue/state split in B1, without reintroducing player clones.
- Private recomp `80057C48`: disables Reset under the special-context byte
  condition, otherwise calls `80058104`. The latter compares535 halfwords
  against the table at800C0CAC; states7/27 select a different working source.
  Recover the baseline's initialization and full Reset dispatch/write path
  before treating all future modes as equivalent to restoring the asset pack.
- Private recomp `80058184/800582C4` also handles created-player/free-agent
  reconciliation. Do not infer that Reset means deleting created players.
- `UserProfileStore`: an existing versioned, checksummed sectioned save with
  generation and temp/backup replacement. Reuse design conventions, not an
  unreviewed copy: bound reads, unknown-section handling, backup recovery and
  durable flush/replace behavior need explicit roster-store tests.
- Current Re-order timing includes native17ms general updates, nominal30Hz
  source-arithmetic title updates and180ms entry transitions. Help uses recovered growth/shrink
  geometry and input barriers; its original recorded cadence remains unverified.

## Persistence decisions

### Separate immutable data, accepted edits, and a screen transaction

```text
Private base roster pack (read-only)
        + accepted local override document
        = effective roster state
                  |
          isolated screen draft
                  |
        prepare + validate + durable save
                  |
          publish prepared state
```

Keep a single immutable player/team catalogue with names, historical stats,
ratings and asset references. Store mutable list ordering/membership separately.
Views resolve IDs through a read-only roster view, which may point at the live
state or the active draft. Help/View/Compare must see the draft without accepting
it, replacing the database, or reloading the screen from defaults.

Distinguish three baselines:

1. Original/default roster state: determines ordinary frontend Reset eligibility.
2. Last accepted durable state/generation: detects unsaved changes and conflicts.
3. Screen-entry snapshot: cancellation restores this, not factory defaults.

The original16-bit swap counter is retained for recovered screen behavior; it
can wrap. Never use it as the persistence dirty flag. Use content differences
and a separate64-bit save generation. A pair of swaps that restores the accepted
state should not rewrite the save or spuriously enable Reset.

### Save only overrides, in a bounded versioned binary container

Default development path: `.local/saves/rosters/default.n97rst`. Support an
explicit save-root/roster-set path so packaged builds can use a writable user
directory without depending on installation paths. One active roster set first;
the same format supports named sets later. No cloud service or SQL dependency.

Implemented v1 container:

| Component | Contents / policy |
|---|---|
| Header | Magic, major/minor version, bounded header/file lengths, flags,64-bit generation,32-byte base identity hash |
| `TEAM` section | Only changed teams; team ID, slot count, complete ordered list of stable player IDs |
| `FREE` section | Complete free-agent list only when different from the base; not emitted by ordinary Re-order |
| Future sections | Created-player definitions and field-level attribute overrides, each independently versioned; no speculative runtime behavior in v1 |
| Section envelope | Tag, version, required/optional flags, length, checksum; canonical order and duplicate rejection |
| Whole-file check | Covers header and sections; detects corruption, not malicious authenticity |

Use explicit little-endian field encoders, never serialize C/C++ object memory,
pointer values, STL containers or compiler struct padding. Store IDs, not names
or images. Rebuild ownership, counts, resolved pointers and derived ratings after
validation; do not save redundant caches. Preserve all empty slots and order.

Use32-bit wire player IDs to leave room for a separate created-player namespace;
reserve `0xffffffff` for empty. Original IDs keep their identity under the
identified base catalogue. Current recovered16-bit slot arrays remain compact:
the v1 adapter accepts only representable known original IDs and translates the
empty sentinel explicitly. Future created IDs require an explicit resolver/
adapter extension; never truncate them into a16-bit value or use vector indices
as persistent identities. No created-player allocation is implemented by v1.

Prefer full lists for changed teams over per-swap commands. A team list is tiny,
preserves ordering exactly and does not require replaying an unbounded history.
Do not compress this small data or add a database engine. Optional human-readable
diagnostic export can be added without becoming the authoritative save format.

### Memory and file-size targets

- Existing535-slot16-bit table: **1,070 bytes**. Four such tables (base, accepted,
  entry, working) total4,280 bytes before metadata; references can avoid extra
  copies. Do not repeatedly clone the full player/string/stat database.
- Proposed32-bit wire roster payload: **2,140 bytes** for all535 IDs.
- With a64-byte header,16-byte section headers,4-byte list descriptors
  and4-byte file checksum: a zero-override document is68 bytes, one changed
  15-player team is148 bytes, and all29 teams plus100 free agents are2,360 bytes.
  These sizes are now asserted by the codec tests. Backups roughly double disk use.
- Initial roster-only encode/decode working-buffer target: under16KiB, excluding
  shared catalogue and renderer. Record allocations/peak bytes in regression
  output rather than asserting whole-process memory usage meets this bound.
- Bound input before allocation: initially1MiB total,64 sections, known list
  counts/capacities, bounded future strings/record counts. Raising limits needs
  an explicit compatibility review, not trust in a file's claimed lengths.

### Compatibility and updates

Identify the base by a canonical roster-catalogue digest: stable IDs and relevant
baseline records/list data plus a defined catalogue namespace. Keep extractor
pack-format version separate so repacking identical logical data need not break
saves. Store the actual source-pack hash in private diagnostics if useful.

Wrong base or unsupported required section: report clearly and leave the file
untouched. Never silently remap IDs or replace an incompatible save with defaults.
Unknown optional sections must be preserved byte-for-byte within bounds on
rewrite; unsupported major/required-section versions remain read-only. Test this
before calling the format forwards-compatible. Migration is explicit, bounded,
backed up and published only after validation; retain old-version fixtures.

Keep roster sets separate from user profiles, settings and future season/career
saves. Initially the active frontend roster is shared, not owned by whichever
profile was last selected. Future careers reference or snapshot a specific roster
generation; frontend Reset must never rewrite an existing career. Created-player
references will need explicit tombstone/deletion policy, not implicit deletion.

### Commit, failures and Reset

Port policy: autosave only at explicit Accept/Continue, not on every selection,
swap, child-screen visit or cancellation. This replaces memory-card persistence;
it is a documented native-port decision, not claimed original memory-card behavior.

Commit sequence:

1. Keep the draft open. Validate IDs, membership/capacity rules, base identity
   and expected accepted generation; prepare replacement state/indexes and
   serialized bytes before touching the live state.
2. Acquire a per-roster-set writer lock and recheck disk generation/content.
   Another app instance or external modification produces a visible conflict,
   not a last-writer-wins overwrite.
3. Write a unique temporary file beside the destination, flush through the
   platform file API, verify it, and atomically replace the destination while
   preserving a validated last-known-good backup. Test first-create separately.
   Review platform durability guarantees; C++ stream flush alone is insufficient
   for a power-loss guarantee. Never overwrite a good backup with corrupt input.
4. Publish the fully prepared in-memory state using a no-fail swap; only then
   exit the editor/show saved status. A failure before commit leaves accepted
   memory and primary unchanged; backup may already contain the old primary. A crash
   after disk commit recovers the new generation on restart.

On startup, validate into an isolated candidate before publication. Missing file
with no backup means defaults; a missing/invalid primary with a compatible valid
backup offers/logs explicit recovery. Unsupported newer saves are not corruption:
do not auto-downgrade them through backup fallback. Reject truncated/oversized,
duplicate, invalid-ID and conflicting-membership files without mutating assets.

Reset is a normal durable transaction with confirmation and the original
special-context availability restriction. For v1 roster ordering, restore the
original roster lists; preserve profiles/settings/stats/assets. Write a valid
empty-override document with an incremented generation rather than simply deleting
the primary file. This prevents a stale backup being mistaken for the active save
on the next clean load. Preserve one pre-reset recovery copy and log recovery
explicitly if ever used. Cancel or failed Reset leaves everything unchanged.
Any broader created-player/attribute Reset semantics await original-code evidence.

## Small implementation slices and acceptance gates

| Slice | Deliverable | Required evidence before closing |
|---|---|---|
| A1 | Inventory Help/View/Compare dispatch, returns, assets and original contexts | Recomp-backed function/callback list; Ghidra resolves ambiguities; no instruction estimates guessed |
| A2 | Compact parent return-context + draft-aware roster view | First/replacement phase, both cursor/top pairs, team, IDs, tint/input state retained; no draft publication |
| A3 | Original-size Help modal and View Player return | Private font/icons/modal assets; empty-slot rejection; held-button release; original return behavior compared |
| A4 | Compare screen and return | Recover real layout/data/controls, not two improvised player cards; same-ID pair, edits, scrolling and both phases tested |
| B1 | Separate catalogue/state and define save codec | Existing535-slot/ownership/accept/cancel tests stay green; bounded deterministic round-trip fixtures and memory counters |
| B2 | Save/load and failure-safe transaction | Restart restores accepted order; draft/cancel/no-op writes nothing; injected failures, recovery and writer conflicts tested |
| B3 | Original Reset gate + durable reset | Enable after a real difference, remain enabled after reload, disable after successful reset; special-context gate; confirmation/cancel/failure/restart tests |
| B4 | Format evolution safeguards | Wrong-base refusal, unsupported required data, optional-section preservation, migration fixtures and malformed-input tests |
| C1 | Original reference scenarios | Matched team/player/order, inputs, animation phase, frame geometry and audio capture parameters recorded privately |
| C2 | Correct measured visual/audio/timing differences | Title/row pulse, modal open/close, transition, held-repeat and each sound event compared; explicit residual differences |
| C3 | Combined end-to-end verification | Edit -> Help/View/Compare -> return -> accept -> restart -> Reset -> restart, plus cancel and failure variants |

The next target is C1-C3 original-reference comparison and correction. Child
views share player/team metadata while keeping their mutable roster state isolated.
The checkpoints below are chronological; later checkpoints supersede earlier gaps.

### First delivered slice: Help (not full A3 completion)

- Recovered `80040FCC` routes state12's active descriptor to two different Help
  records. The private pack also contains the source-selected View/Compare Help
  records for later child integration; loading those records does not implement
  the child screens. Extraction produces712 bytes under `.local`.
- Native C `frontend_help.c` specializes the no-choice, style-zero modal:
  initial `(246,110,20,10)`, independent geometry clamps, original input-change
  barriers, text removal before shrinking, open sound7/close sound8.
- C++ renders the private descriptor using original ZFONT1 glyphs, including
  the controller bytes and `1F` spacing operands. No keyboard captions or copied
  original game strings in source. Re-order controls: F/H/F1 open Help; a fresh
  game-button input dismisses it without selecting/cancelling the parent.
- Nine asset-free native checks, one local font/descriptor check, and five
  synthetic extraction tests pass. Windows and Linux native builds pass.
  Eight Help compositor captures cover initial/open/shrinking/returned states
  in both selection phases; returned frozen frames match their parent exactly.
- Existing64 Re-order scenarios remain passing. The ten-owner875/875 source
  ledger is unchanged; no new instruction credit or original-fidelity percentage
  is inferred from Help tests. See [child verification](reorder_child_verification.md).

### Second delivered slice: draft-aware View

- A16-byte C child context routes the actual result2 to state24, chooses the
  correct first/replacement slot, blocks parent input, and preserves parent
  cursor/top/draft on return. The Compare request context is also tested, but
  that is not an implemented Compare screen.
- `8005A3FC/8005A6F0` only adopt browsed child selections for parent13 (Trade),
  not parent12 (Re-order). The native return follows this distinction. Entry and
  return masks have input-change barriers; Enter/X/Esc return without selecting
  or cancelling the editor. C/Space plays a fact; D/S stops it; F/H/F1 opens the
  original-size child Help. Existing arrow/J/K/Q/E navigation is reused.
- `draftView` validates the entire535-slot baseline and per-team membership,
  overlays all staged teams, and shares the immutable player catalogue. The
  child's team lists/indexes are isolated; the live database is not temporarily
  replaced or edited to render the child. Held projections survive catalogue reload.
- Twelve asset-free child checks plus seven local-catalogue checks pass, including
  multi-team drafts, both stages, wrap/team/stat browsing, empty slots, stale or
  malformed drafts, and Accept/Discard after returning. The existing native
  full-app self-test and View Rosters12-scenario/18-interaction checks still pass.
- Twelve added window-compositor frames exercise actual key handlers before and
  after a swap. Source-selected player IDs, opaque portrait pixels, child Help
  bounds, and all three frozen-parent returns are checked. This proves the
  draft is displayed, not original-transition/audio/frame-sequence equivalence.

### Third delivered slice: Compare controller (A4 partially complete)

- Recovered normal Compare navigation is12 bytes and borrows the validated
  draft. Both identities remain independent; Cross switches the active side;
  up/down scrolls the two stat columns together. Team scans retain the slot or
  walk backward to a populated slot; the free-agent team is conditional on its
  first slot and supports all100 slots when populated.
- Eight focused asset-free controller scenarios pass. The child suite now has
  thirteen asset-free checks plus eight local-catalogue checks, including
  Compare draft identities and parent returns from both selection phases.
- Corrected the previously misidentified `8003B26C(0x1B)` call: it redraws the
  stat-layer label, not audio. View now retains scroll on same-extent layer
  transitions and resets it on changed extents, with new regression assertions.
- Compare's own graphics table, text-column math and original descriptor tables
  are recovered; extraction, host rendering/input integration and captures are
  still pending. S remains explicitly unavailable rather than opening a fake UI.
- No new instruction totals, durable-save claims or original-fidelity percentage
  are added by this controller checkpoint.

### Fourth delivered slice: Compare host screen

- S now pushes the real state35 composition; it does not open two View Player
  cards. The968-byte private Compare pack contains13 header/layer/position/name
  strings and55 original field descriptors. Runtime parsing is bounded to8KiB,
  validates version/extents/field IDs and rejects malformed or trailing data.
- Source graphics`800978C4`, ZSET4`ba02`, original frames/plates and Z2PORT87x51
  records are used. `3D930` font selector1 uses ZFONT1. `5A280`/`3CF70`/`3B26C`
  place labels atx256 and values atx128/384. Five stat rows usey135+14*n.
- Cross switches the small selector-arrow pair at118/135 versus374/391, y116,
  recovered from`3D434`/`39BA8`. The original normal Compare controller handles
  player/team browsing and synchronized stat scrolling/layers. Enter or Cancel
  restores the suspended editor without adopting browsed identities or saving.
- Recovered sound dispatch is up/down3/4, left/right2/1, generic callback6;
  Help7/8 and normal accept/cancel9/10. Endpoint scrolling suppresses sound.
  These IDs are not a recorded waveform/gain/timing comparison.
- The46-frame local verifier covers three Compare round trips, both portraits,
  active selector-only changes, independent team palettes, original Help bounds,
  ratings/attributes/bottom scrolling and populated free-agent browsing.
  It compares each return to the immediate pre-entry parent frame, since prior
  input-release ticks legitimately advance parent tint. Original assets stay unchanged.
- Five synthetic extraction tests, three synthetic runtime-pack checks plus one
  private-pack check complement the existing controller/child suites.

### Fifth delivered slice: bounded save codec (B1 partially complete)

- `roster_save_codec.cpp` encodes changed lists and decodes into an isolated
  document; no disk I/O or live publication. It conserves the535-slot base
  population, preserves empties, rejects duplicate/unsupported IDs without
  truncation, and uses explicit little-endian fields plus nested CRC32 checks.
- Required unknown/versioned data, wrong base and malformed data have separate
  errors. Unknown optional payloads and versions survive canonical rewriting.
  A32-byte base identity is currently supplied by the caller; the real canonical
  catalogue digest/adapter is still pending. No migration or recovery is claimed.
- Ten asset-free codec check groups include a frozen independent v1 fixture,
  148 truncations,1,184 single-bit changes and4,096 checksum-repaired mutations.
  The exact1MiB limit and overflow-at-limit regression are covered.
- Test-only allocation probes measure requested C++ heap payload, not total
  process memory. Windows Debug measured5,344 bytes peak encode and32 bytes
  decode for all lists; both are gated below16KiB. Stack, allocator bookkeeping,
  pre-existing documents/catalogue, rendering and future optional sections are
  excluded. The whole-working-set target is not yet established.
- Draft team names now share immutable ownership, like player data. Synthetic
  and private-catalogue regressions check pointer sharing and retained names
  across database reload. Mutable roster/index data remains isolated.
- Six CTest suites pass on Windows/Linux; the non-PIE ASan/UBSan configuration
  passes all six three times. Existing64 Re-order scenarios and46 compositor
  frames remain passing. The codec test is wired into local verification and CI.

### Sixth delivered slice: canonical base and prepared publication

- The immutable shared catalogue now owns the original535-slot baseline and its
  SHA256 canonical identity. Re-order, child drafts, accepted overrides and
  prepared Reset candidates cannot replace that baseline. Cancel still uses the
  separate screen-entry snapshot. The CLI logs the actual computed base identity.
- Canonicalization streams explicit logical fields, ordered by stable IDs,
  plus original team/free lists and special fallback IDs. Paths, file padding,
  string-pool offsets, record order and extractor container version are excluded;
  original source fields that influence behavior remain included. Synthetic
  v4/v5 repacks preserve the identity when their effective logical data matches.
- `prepareSlotTable` checks global population and contiguous occupied prefixes,
  then prepares roster ownership/counts/resolved pointers before publication.
  `swap` publishes with no allocations. Load now also prepares everything before
  changing live state, so malformed loads retain the old catalogue/base/pointers.
- Native tests cover known SHA256 vectors and streaming boundaries, an independent
  canonical-schema golden, repacking/relocation, five meaningful base changes,
  failed reloads, save-codec integration, in-memory Reset and transfers. Synthetic
  and private-catalogue candidates share metadata and original slots.
- Allocation failures are injected once at every allocation index until a full
  execution has no injected failure. Windows exercises65 preparation and320 load
  failure sites; publication requests zero allocations. This is not disk failure
  injection or a power-loss claim.
- The initial fault harness exposed MSVC14.38's debug `_Hash_vec` constructor:
  it allocates `_Container_proxy` inside `noexcept`, so injected OOM aborted the
  CRT and opened desktop debug dialogs. A captured native stack and local MSVC
  source confirmed the cause. Only the dedicated fault-test target now uses
  `_ITERATOR_DEBUG_LEVEL=0`; the app and other suites retain Debug checks. Test
  CRT failures are routed to stderr/nonzero exit, never interactive dialogs.

Wire offsets/canonical field order are in [roster save format](roster_save_format.md).
The B2/B3 host checkpoint below supersedes the library-only status. Original
reference C1-C3 remain pending. The goal stays active; neither source-backed
sound IDs nor exact frozen native returns establish original-game fidelity.

## Original comparison and reporting policy

### View source-gap checkpoint (2026-08-27)

- Recomp `80059ABC`/`80059ACC` and Ghidra agree: normal team scans retain the
  current slot, walk backward over empty slots, and preserve stat scroll.
  Fixed the native unconditional zero resets; added338 synthetic vectors,
  all occupied slots of all29 teams in both directions for both catalogues,
  and atomic empty-target rejection. The child suite now has26 named scenarios
  (17 asset-free,9 private). Free-agent/special eligibility in View is still open.
- `8005A538` now actually binds layout24 to View's stat-change bookkeeping;
  it no longer reports Compare's second-column refresh. Same/changed descriptor
  extents have explicit regressions.
- `8003D930` source-backed View navigation and return sound dispatch is wired.
  A layer-triggered scroll reset no longer generates a false Up sound/flash.
  The46-frame host verifier checks exact navigation sound sequences, retained
  stat tops and frozen parent returns, with lossless private PNG proof copies.
- This is source-backed correction and native regression evidence, **not** a
  new original emulator comparison or additional875-owner instruction credit.
  Original frame/tick/audio sequences and remaining transition timing continue
  in the fidelity gate. The no-facts modal is now implemented as described below.

### No-facts notice checkpoint

Subsequent source audit found and fixed a shared speech-index off-by-one:
`315BC` adds one to `player*5+variant` because physical record0 is reserved.
The no-facts predicate and actual decoder now share the corrected bounded view.
All2,465 logical mappings and six cross-player-boundary clip payloads are tested;
six private WAV exports and per-record provenance are retained by the53-frame
screen verifier. These are source/native extraction checks, not original mixer
capture parity. Reggie Geary still has no facts under the corrected mapping.

Recomp `59E14 -> 40A1C` supplied descriptor`800AFE06` and the shared prompt
at`8002502C`. The port now renders the small red warning with source-selected
text, ZFONT1, body/footer coordinates, sounds5/8 and input-change barriers.
The descriptor/prompt are extracted into a92-byte private pack; unavailable
speech records are distinguished from missing/corrupt asset data.

New parser/controller/font tests and seven additional host captures bring the
screen suite to53 frames. The end-to-end native scenario uses actual original
empty IDX entries for player62, blocks underlying navigation/exit while closing,
and returns to identical player/editor frames without modifying the draft or
saving. Windows11/11 and Linux/non-PIE ASan+UBSan10/10 suites pass; the28-case
save/Reset suite also remains passing. No875-owner source credit was added.
The screenshot is native proof, not a same-scenario no$psx comparison. Available
speech variant scheduling/icon flash and original frame/audio references remain
in the full goal.

### Live SFX checkpoint

Both cursor playback routes now consume the current Options SF/X setting.
Recomp `8002F124/8002F12C` proves the zero-setting early return and
`min(setting*12,127)` scale. Muting skips playback, preserving the existing cue
and independent visual stat flash. Diagnostic WAV exports keep explicit/default
volume parameters so saved comparisons remain reproducible (default9).

New Windows CTest `recovered_audio_gain` checks256 setting bytes, synthetic
gain/pitch samples and no-device mute. Its optional private bank test checks
144 cue/level vectors, including default-export compatibility and fixed pitch/
duration. The46-frame verifier also exercises actual host live/mute wiring and
records source, binary and audio-bank hashes. This closes the hardcoded SFX
setting gap only: these native checks do not establish original recorded
waveform/envelope/mixer equivalence, nor add Re-order instruction credit.

### B2/B3 host checkpoint (2026-08-27)

- Windows startup loads the accepted override (or explicitly logs backup
  recovery). Unsupported format/base mismatch leaves the save untouched and
  the editor unable to accept until the file/path problem is resolved.
- Accept prepares, validates and commits before publishing. A failed commit
  restores the entire editor state, including selection/tints/scroll; the small
  green native-port notice cannot accept or cancel the editor through its input.
  A committed sync warning reports the new generation and cannot replay the edit.
- `tools/verify_reorder_save_host.py` uses unique private fixture directories,
  actual host key handlers and compositor captures. It checks primary/backup
  bytes, generations, reload, no-op, cancellation, failure/retry, committed
  warnings, incompatible files, source assets and active save/profile/settings
  preservation. Ordinary capture/self-test modes never load the active override.
- Reset uses C `roster_reset.c`: `57C48`'s exact special-active AND kind1
  override, otherwise `58104`'s535-slot comparison. The current runtime is the
  normal frontend; season/state7/27 and created-player runtime paths are not
  silently treated as implemented.
- The Reset callback is at `80057960`. Recomp labels its body `80057968`
  and misclassifies the first two load instructions as preceding data. The
  read-only Ghidra project has no function at57960; use the recomp plus those
  original words, not a fabricated successful Ghidra export.
- `57960 -> 40A1C` references `800AEDD2`. The182-byte private descriptor
  has rect121,75,270,110, style1, five body lines/two choices. It states that
  created players move to the free-agent pool. Original initial preference0
  selects the last choice (Cancel). `extract_roster_reset.py` copies this
  descriptor only under `.local`; the renderer uses original ZFONT1.
- Confirmation opens with sound12, changes focus with3/4, accepts with6 and
  closes with8. Raw Cross800 selects; Start/Circle do not dismiss this original
  choice dialog. Native C/Space/Enter are keyboard aliases for Cross here.
  Text is removed before shrink; opener and return input-change barriers apply.
  Text colors use the existing recovered20-update pulse/eight-update unpulse,
  not a wall-clock sine. Source-linked dispatch is not waveform parity.
- The new sound assertion exposed an old decoder limit: ZCURSOR rejected ID12.
  Recomp91814 selects128 slots for legacy banks, otherwise the u16 count at+6.
  This bank has128 slots with populated IDs1..12. The loader now honors that
  count and rejects empty/out-of-range entries. The private host proof exports
  ID12 as a non-silent22,050Hz mono16-bit WAV with8,064 samples. Its PATl/tone/
  TMxl source path is verified; comparison with original playback is still open.
- Only confirmed Reset enters `3F324`'s card flash. The native normal-roster
  transaction restores29x15+100 original slots, rebuilds resolved pointers/
  owners/counts and invalidates derived ratings (full5DB34 ranking remains
  separately scoped). It writes a68-byte empty override at generation+1 and
  keeps the pre-reset backup; it does not delete the primary or source assets.
- Failure retains the accepted roster/generation and Reset eligibility.
  Success locks Reset again. Failed writes and postcommit sync warnings have
  distinct small green native messages; their wording is not original text.

Native tests cover the current normal-roster feature, not created-player
reconciliation, season-mode state, exact modal timing/rasterization or original
waveform parity. New Reset/shared-helper source is documented but has not been
given additional instruction-ledger credit. The existing875/875 remains the
ten-owner Re-order accounting figure, not the completion percentage of this goal.

Current host verifier:28 fresh-process cases, including one linked
edit/children/save/reload/Reset/reload chain, failure/retry/cancel and
postcommit-warning paths. Nine asset-free CTest suites pass on Windows and
Linux. The separate46-frame child/screen verifier remains a native regression
gate, not46 original comparison scenarios.

### File-store checkpoint (B2 library, not game integration)

The store holds a per-set OS lock, checks primary/backup content fingerprints,
prepares replacement state, flushes/verifies a unique same-directory temporary
file and publishes by rename. Validated old primary becomes backup first;
recovery never copies corrupt primary over a good backup. Defaults/no-op create
no primary. Unknown optional sections survive disk rewriting; incompatible
data, changed files and generation overflow are refused.

Windows requests file flush and same-volume write-through MoveFileEx; POSIX
uses file fsync, rename and directory fsync. Post-replacement failures return a
committed warning with new memory state, preventing duplicate application.
Ambiguous outcomes require reload. These APIs and tests do not certify every
device/cache/filesystem under power loss.

Tests cover16 precommit failures with retry, two postcommit failures, actual
Windows sharing denial, restart, empty-override Reset, backup recovery, oversized
input, stale writers, held OS locks and hardlink/directory asset aliases. Eight
native suites pass, including repeated non-PIE ASan/UBSan runs. The private
catalogue also passes disk save/reload/Reset with its source bytes unchanged.
All store writes are in isolated fixture directories. Runtime Accept/startup/
error/retry behavior, original Reset availability/confirmation and combined UI
captures remain open.

### Remaining original-reference work

The fixed 14-case capture contract, provenance requirements, comparator and
exit-status interpretation are in [reorder_reference_workflow.md](reorder_reference_workflow.md).
Use verify_reorder_rosters.ps1 -RequireReferences when auditing completion;
the normal native regression run explicitly reports that it did not check
original-reference media. Passive native presentation recording now exists
(--record-native-frames, F9), with original-size frames, actual timestamps and
raw input events and optional process-scoped mixed PCM with packet QPC timestamps.
It is not deterministic replay and cannot pass the paired reference gate by itself. Original collection remains
open; see the reference workflow for capture limits and interpretation.

Live recorder smoke test: Chicago first-player Re-order was reached through
normal title/setup/roster navigation, using isolated save/settings/profile paths.
F9 retained600 presentations over14.982 seconds,434 distinct RGB frames,
including Help opening (phase changes at frames251/263/264). Intervals were
11.922..47.647ms, median30.572ms. All600 submitted frames were written; no queue
overrun. The recording reached its cap before Help closed, so it is not a
complete Help round-trip reference or the Atlanta contract case. Raw evidence
is private under verification/native-presentations-20260827-130315, and
inspect_native_frames.py validates its structure/timing without awarding
original-reference credit. No accepted roster save was created in that session.

### Mixed-output capture and music isolation checkpoint (2026-08-27)

- Passive audio/video now share a QPC origin. Audio captures only the current
  process tree; no microphone/system fallback. First-packet startup and bounded
  tail draining prevent the previously observed uncovered first video boundary.
  Raw packets/flags and variable presentation times are retained, not normalized.
- A Chicago first-player Help recording retained1279 frames/32.021 seconds,
  including all modal phases and unchanged recorded cursor/top/player fields.
  However, checking PCM exposed a real failure: peak1/RMS0.48 despite CLI cues7/8
  reporting submission. This is preserved under
  `.local/verification/reorder-help-roundtrip-20260827`, not counted as sound proof.
- An independent two-tone test confirmed the cause: production Music=0 through
  waveOutSetVolume also muted the other WinMM streams on this system. Stable-window
  RMS fell from848.11 to0.48, then recovered after music stopped. Private before/after
  directories are music-mute-isolation-20260827 and music-mute-isolation-fixed-20260827.
- Frontend music now applies integer linear gain only to its own returned PCM
  buffers. Four1024-frame stereo buffers add16KiB; a dedicated worker refills FIFO
  order without mutating queued buffers or changing Windows session volume.
  Muting advances the music cursor; unmuting does not restart the track. Gain changes
  reach newly queued audio within the4096-frame queue (about93ms at44.1kHz).
  Decode still retains the original full PCM track: incremental asset decoding is
  not claimed. This native adapter does not establish original mixer/envelope parity.
- CLI reports stream startup/gain, background failures and queue starvation.
  In the after-fix two-tone capture the independent streams remain at848.11 RMS
  through music mute. Asset-free music tests exhaust256 gains x65536 sample values,
  immutable source, stereo loop wrap, mute/unmute cursor and malformed extents.
- The rebuilt game was navigated normally and recorded again under
  `.local/verification/reorder-help-audiofixed-20260827`:1309 frames/32.708 seconds,
 593 distinct images, cues7/8 logged, peak17048 and134270 PCM samples above1 LSB.
  All video boundaries have packet coverage and packet QPC residuals are zero.
  No music underrun or accepted roster save was observed. The visible Help cycle
  returns to the same recorded parent fields; the nonvisual return barrier was
  not caught between paints, so the stricter all-phase diagnostic stays unverified.
- `inspect_native_frames.py --require-help-roundtrip` now distinguishes complete
  observed parent cycles from idle/partial captures, missing phase evidence,
  unmatched inputs and changed parent snapshots. It never proves all535 slots or
  original fidelity. The first recording passes its phase diagnostic but lacks
  sound; the second has real sound but lacks one transient phase observation.
  Do not combine them into a fabricated complete reference.

Current native CTest count:15 Windows,13 platform-independent suites. Original
paired cases remain0/14; the ten-owner source ledger remains875/875. Next capture
work still needs matched original input/video/audio recordings and variable-time
comparison without resampling.

Help-event recorder checkpoint (2026-08-27): the passive recorder now records
changed native Help call boundaries, modal geometry, input/result and effective
535-slot draft hash on the video/audio QPC clock. Nine inspector regression
tests cover between-paint transitions, missing/broken chains, incorrect geometry,
changed parent/hash and missing input evidence; collector tests cover bounds,
equal-time ordering and incomplete-output failures. These are native tests, not
new original-reference or instruction-ledger credit.

The first live attempt at `.local/verification/reorder-help-events-20260827`
stopped after3581 submitted presentations with a writer-queue overrun, before
the intended Help interaction. It remains incomplete and earns no live-cycle
credit. A subsequent direct UI check opened the small green first-stage Help
modal and returned to the same visible editor; that interaction was outside
the stopped recording and must not be spliced into it. A complete live event
capture remains pending. No roster edits were accepted during this check.

Follow-up live capture (same date):
`.local/verification/reorder-help-events-short-20260827/capture` completed
normally after1314 presentations/33.097 seconds, including589 distinct images
and39 Help events. Atlanta/team0, first stage, both cursors/tops0 and player0
were reached through normal menu/team input. Both the event-cycle gate and
the video-only all-phase gate pass for this recording; the effective535-slot
hash stays `fa38f67450bc6557dffa4c32e8521c105c4be084df76b1e36a1d06cf9360d035`.
Audio contains1590240 stereo sample frames, peak17048 and134259 samples above
one LSB; cues7/8 were logged and all video boundaries have packet coverage.
WAV SHA-256: `edf2fa50cb8b67308233ecfd4e00515e0aab9fbba1c3c9ac9626dc44a7369830`.
The device still supplies zero positions, so sample continuity and exact A/V
synchronization remain unverified despite zero packet-QPC residuals. No original
pair was captured or compared; this closes only the first-stage native live
Help-event recording gap, not the replacement-stage or original-reference gates.

Latest speech slice: the host now uses source-backed five-variant selection
predicates instead of decoder-global round-robin. State36 object21 flashes
eight times over the retained object20 plaque; variant consumption is deferred
until that callback completes. The65-frame private screen gate includes eight
overlay phases and two complete speech cycles with input/parent-state checks.
See `reorder_child_verification.md` for source addresses, the sparse-data edge
case and explicit RNG/presentation/audio limitations. Speech now uses the
original independent setting2 scale(*15), prepares before cue6/start, and
selects Square-stop cue5 only for an active voice. Private tests cover72
clip/volume vectors; the host verifies zero-gain lifecycle and idle-stop silence.
These changes do not add
instruction-ledger credit or establish original runtime parity.

Use no$psx only as an external original reference/debugger, never as the port's
runtime. All extracted assets, dumps, screenshots, clips and audio remain local.

Resumed-goal palette checkpoint: the app goal is active, but its stored objective
was replaced by the literal `resume`. This document retains the actual unfinished
scope: Help/View/Compare round trips, durable saves/Reset, and original-reference
motion/audio verification. Do not treat the literal goal title as completion.

Compare now renders original indexed Bkga-d from a134356-byte local-only pack,
with33 ordered ZTMPAL palettes and96 fixed local colors per strip. Recovered C
preserves 8002FF40/8002FF80 masked-word rounding, target STP, the original3C00
blue mask, independent halves and interrupted fades from the current palette.
The host advances factors0..16 beneath Help too; its17ms scheduling remains
a native timing approximation. CLI logs entry/request/interrupt and factors0/8/16.
No global View Rosters RGB crossfade replacement was made in this slice.

Native evidence:208896 arithmetic vectors, interrupted/same-target/both-half
controller tests,18 full synthetic indexed frames, malformed-pack guards,
and82 private UI checkpoints (including17 fade captures and14144 raw-CLUT
pixel checks). Windows17/17 and Linux15/15 CTest suites pass. Original paired
reference cases remain pending; no extra instruction-owner credit is claimed.
See `reorder_child_verification.md` for format, source and remaining limits.

Compare refresh checkpoint: the native host now suspends player-cycle input
through two completed logical presentations before publishing header/stat text
and dispatching the selector cue. The first frame retains old graphics; local
portrait loading then completes independently of the retained text. A14-byte C
continuation is tested across116 normal-team sequences plus four free-agent
boundary sequences.85 private UI captures cover the split portrait/text phases,
input suppression and text-before-sound ordering. No original CD latency is
emulated or inferred: source310D8 schedules an asynchronous read whose completion
is separate from59928's two-frame text wait. Original recording comparisons,
post-callback repeat delay and the corresponding View Player host migration
remain open. Also corrected Compare's original single-free-agent suppression:
context+708 is count[29], not a generic season-mode flag.

Original-baseline follow-up (2026-08-27): corrected Z2PORT CLUT transparency,
both-list persistent scroll markers, and descriptor-selected `hel1`/`hel2`
footer graphics. Fresh read-only Ghidra confirms the footer replacement path.
Windows15/15, Linux13/13,65 native screen checkpoints and the full private
Re-order/save regression pass. Footer checks cover11 composed parent/return
frames; see `reorder_child_verification.md` for source addresses and limits.
The reference validator still reports14 missing paired scenarios, not14
successful comparisons. These presentation fixes do not close motion/audio
reference gates or add instruction credit.

Reuse the View Rosters verification conventions but create a separate Re-order
scenario inventory; missing references must fail a required-reference gate.

Capture a frame sequence, not only stills: title vertex motion, selected-row
color phases, exact modal bounds, opening/closing frame counts, transition order
and repeat intervals. Keep PSX logical-frame timing separate from host refresh
rate. Compare native semantic events/state and original call traces separately.

For audio compare sound selection/program/tone, decoded sample count/rate,
pitch, gain, onset and duration. Compare like-for-like digital PCM exactly where
the source pipeline permits; original emulator/device captures may have mixing,
resampling or latency. Declare alignment and tolerances beforehand, preserve raw
recordings, and report residual error. Do not normalize away wrong volume or
pitch, change thresholds to force a pass, or call similar waveforms an exact match.

Report per gate: source mapped; native tested; original reference captured;
comparison passed/failed/unverified; known differences. Exact original fidelity
is a target, not a promised result. Close the overall goal only after required
work/evidence is complete, or obtain an explicit scope adjustment for genuinely
unresolved fidelity differences.

CLI should include transaction/set IDs, generation, phase, parent/child route,
both cursor/top pairs, selected IDs, mutation count, durable-dirty versus
default-different, serialized bytes, save/backup/recovery outcome, Reset reason,
and audio/animation event ticks. Log transitions and failures rather than
reprinting unchanged state every frame. Never claim a file was saved before its
durable commit succeeded.

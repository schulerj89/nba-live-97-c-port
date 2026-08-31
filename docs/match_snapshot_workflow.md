# Owned ordinary-exhibition match snapshot

This checkpoint captures accepted match inputs without launching gameplay. It
uses current saved roster membership and settings, retains live controller maps,
and owns all published data. It does not execute an overlay or change the
Create Player model, animation, projection or texture paths.

## Native boundary

State5 result6 calls MatchSession::capture before the native Start latch is
restored. A successful capture replaces the previous snapshot and increments
its revision. Preparation, validation and allocation finish before either the
snapshot or live maps change. Refused readiness, cancellation, unsupported
special teams and invalid sources preserve the previous publication.

The snapshot contains:

- Both selected team IDs, four Setup choices and all eight accepted controller
  assignments/selectors. Local sides must agree with accepted assignments.
- All535 accepted roster slots, the immutable base identity, generation receipts,
  and each selected team's complete15-slot order. Valid player records and five
  team names are copied by value; no borrowed names or player pointers survive.
- Fresh ranks calculated from all29 accepted regular rosters. The five ranks
  replace only metadata[0..4] in the snapshot, corresponding to team+54..58.
  Immutable database metadata and save identity are never changed.
- Source roster count, active count min(count,12), twelve alias indices and
  initial lineup0..11. Remaining aliases point to player0 when count<12.
  Bench players13..15 remain owned; they are not silently discarded.
- Effective rules, custom-rule backup and all eleven separately mapped options.
  Finalization reapplies arcade/simulation or the saved custom backup even when
  a loaded settings file has inconsistent active rules. The source settings
  object is not mutated by capture.
- Owned live maps/provenance, selected stable profile IDs, and the complete
  created-player catalogue with an explicit unresolved-membership flag.
- The source-selected presentation byte and owned before/after shared RNG
  words, committed only after successful preparation. See
  [presentation selection](match_presentation_workflow.md).
- Known persistent fourteen-byte team strategy, copied from the session without
  reinitializing on confirmation. See [strategy lifetime](match_strategy_workflow.md)
  for the field-only gameplay projection/writeback and remaining extensions.

Ordinary exhibition retains source venue selector8001EC94=0 (u32), launch
control8001EDEC=0 (u16) and injury slotsFF/FF. These named invariants do not
establish demo, season, playoff or injury-substitution branches.

The rating adapter still requires contiguous8..15-player regular rosters. This
is a native safety boundary, not a restriction in the original arithmetic.
Interior holes, unresolved IDs, malformed created IDs and inconsistent inputs
refuse preparation. Teams29/30 explicitly remain pending; descriptor29 from the
roster editor must never substitute for an All-Star team.

## Control-map lifetime and private assets

FEONLY28800 calls61674(1) when resident word80021EE4 is zero. Fresh FEONLY data
contains zero;360D4/360E0 later writes one. GAMELOAD preserves that resident word
and the control maps. No direct reset writer was found in the bounded
FEONLY/GAMEONLY reference audit; warm and indirect paths remain unproven.

MatchSession initializes defaults once per fresh native process, lazily before
the first User Setup entry because no earlier native consumer exists. Reentry
does not reinstall defaults. Identical repeated initialization is a no-op;
changed defaults refuse. This is the bounded host lifetime policy, not a claim
about exact original initialization timing or every warm-return branch.

Subsequent captures use61674(0). Negative selectors retain prior maps. A saved
slot with nonzero raw validity copies its59 bytes; a cleared or disabled slot
copies defaults. A neutral controller may retain a deleted slot selector, so a
missing saved record is not an invented readiness failure. All36 statistic
prefix bytes clear. See match_controls_workflow.md for the full bounded core.

Extract locally before running User Setup:

~~~powershell
python tools/extract_match_setup.py
pwsh -NoProfile -File scripts/build.ps1 -Configuration Debug -AllTargets
pwsh -NoProfile -File scripts/verify_team_select.ps1 -SkipBuild
~~~

The extractor checks the FEONLY address producer and cold initializer, then
writes .local/assetpacks/match_setup/controls.n97ctl. The71-byte pack contains a
12-byte version/count/address header and the59 private default bytes. The reader
requires the exact extent; missing or invalid packs refuse User Setup entry.
No original control bytes are embedded in public sources or receipts.

## Verification and evidence limits

tests/match_snapshot_test.cpp uses a manually packed synthetic database under a
fresh ignored verification directory. It covers accepted counts8/11/12/15,
reorder/rank changes, metadata preservation, independent string ownership after
all source catalogue owners are destroyed, settings backup semantics, control
retention/deletion/defaults and atomic failure. Created metadata.roster_slot=5
does not cause a guessed insertion.

The76 combined host scenarios include four successful publications, refusal
before readiness, cancellation, disabled saved controls, FE retention, special
refusal and a saved/reopened roster. Two processes must produce identical
snapshot JSON as well as states/frames. The verifier independently checks
ordered prefixes, aliases, lineup, current ranks, option addresses, effective
rules, generations and the known0/8 roster swap. Real save/config bytes and
timestamps must remain unchanged.

The private raw-MIPS oracle executes GAMEONLY63D58/655B0 and FEONLY rule
dispatch/helpers with independent synthetic memory. It compares544 native
roster and768 rule projections,1272 guard/alias checks and13 option mappings
without a mismatch. Underlying original fixtures comprise512 roster and1024
rule cases. Executing PCs is a separate evidence tier from native ownership or
live runtime equivalence. It earns no instruction credit.

config/decomp/match_setup.json retains seven full function denominators:
253 roster-dependency instructions and148 rule-dependency instructions, all
zero credited. Only projections of these functions are implemented. Controller
owner61674 and rating helpers reference their existing inventories, and the
existing Rules records share ownership rather than adding global credit.

Snapshot checkpoint validation on2026-08-30:41/41 CTest tests pass in Debug and
RelWithDebInfo;62/62 host states/frames and both snapshot artifacts repeat in
each configuration and match across configurations. All preceding57 frames
remain unchanged. Create Player retains27/27 deterministic scenarios and its
753-vertex/251-primary-packet/zero-missing-texel checks. Existing instruction,
recovery and progress metadata totals are unchanged. The release executable
and desktop shortcut are refreshed; real saves/configuration remain untouched.

Private evidence: Team Select Debug run20260830-174936-6e6e2bd1, release
run20260830-175041-ca3a8470; Create Player run20260830-174953. Logs use
.local/logs/match_snapshot_*. Independent verifier mutation checks accept two
valid fixtures and reject39 altered receipts/ledgers. These artifacts remain
ignored and are not included in checkpoint commits.

Later input/polling changes expand the combined suite to76 scenarios and alter
presentation phases. See frontend_input_workflow.md and team_select_workflow.md
for that checkpoint; the unchanged-frame claim above belongs to the snapshot
checkpoint only.

## Remaining dependencies

The later team-header checkpoint adds all ordinary655B0 written effects as an
owned stage before65328/65DB0, with explicit unknown/opaque source words and
pending bit16. It does not create full headers or a playable scene; see
[team_header_workflow.md](team_header_workflow.md).

The snapshot still marks extension settings pending. The subsequent
[presentation checkpoint](match_presentation_workflow.md) implements21DF4 for
ordinary exhibition using the existing RNG. Seven paired team
settings21DE6..21DF3 are now separately owned by the strategy checkpoint;
unowned extension bytes21D80/85/96..21DA2 are not fabricated. Created-player catalogue
membership/resolution and relative-rank effects remain a separate bridge.
Special-team rosters/jerseys, season/playoffs, raw absent-player copy behavior,
remaining gameplay header/controller initialization and court/model/camera lifetimes
also remain pending. See gameplay_first_path.md for the recovered ordering.

Original comparison needs synchronized, overlay-identified stops at FE61674
entry/return and409D8, GAMEONLY63ED4 and655B0 return, then65328. Compare selectors
80021DDE length8, controls8001EF7C length0x3C0, resident settings80021D70
length0x170, roster copies8002208C length0xA50, aliases/counts and owned metadata.
Validate backing and code signatures first. Native artifacts do not prove
original visuals, timing, audio or physical input delivery.

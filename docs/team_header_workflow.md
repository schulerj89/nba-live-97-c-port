# Owned team initialization before period setup

The native accepted-match snapshot now owns every written effect of GAMEONLY
800655B0 for the ordinary caller's home/away side words0/5. This is a data stage,
not a playable match. Its exact boundary is after both655B0 calls and before
65328 controller assignment,65DB0 period preparation and65820 strategy application.

The portable implementation is recovered/team_header.c. C++ match_snapshot.cpp
supplies accepted roster counts, first-five lineup indices, current owned team
metadata, difficulty and injury sentinels. All outputs are owned values; side
and entity IDs replace native pointers. Copying/moving a snapshot cannot retain
references into the previous snapshot or source database.

## Source contract

The complete original owner is800655B0..80065820:156 instructions, including
three unreachable negative-clamp instructions behind unsigned-byte loads.
No instruction credit or whole-game completion percentage is added.

| Written effect per call | Extent | Recovered behavior |
|---|---:|---|
| Team header fields |47 bytes|Opposite/team-metadata/alias links, word08/0C, direction, counts, thresholds, defaults, saved starters|
| Availability status |12 halfwords|7FFF for occupied noninjured slots, FFFE otherwise|
| Entity table |5 words|Register entity IDs side-word+4 down to side-word+0|
| Entity opponent index |5 halfwords|Opposite side-word plus the same local index|

This owner does not clear the C4 header. The other149 header bytes are absent
from the semantic effect object; no zero-filled full header is claimed. Source
caller659F0 clears headers before these calls, but that is a separate boundary.
The core preserves all raw unsigned-byte inputs and arbitrary first-five
halfwords. Count0 still registers five entities. The native adapter retains its
existing ordinary-team, contiguous roster8..15 validation.

Counts66/68 are min(count,12). Saved starters copy the five lineup halfwords.
Fields34/38/39 begin7/7/5, even if a persistent strategy from an earlier match
contains other values. Direction10 is-85504 home and+85504 away at this stage.
Metadata byte+57 (owned metadata[3]) supplies field62=(120-2*rank57) modulo65536
and field72=rank57+28, halved for difficulty>=2. Metadata+54 (metadata[0])
supplies field74=(1260-32*rank54) modulo65536. These use freshly recalculated
ranks, not immutable stock ranks.

## Two source words are not roster bounds

The source loads20BEC[0]/[12] for home word08/0C and[12]/[24] for away.
Only indices0..9 are newly registered as entities. Home word08 is the entity0
reference written within that same call. The two other reads overlap fixed
profile slot0 at20C1C:

- Index12 is the retail profile prefix at+0. Native UserCareerStats has a
  different, explicitly nonretail schema. It cannot supply this word.
- Index24 is profile+30(hex): raw saved controls[14..17], little-endian.
  The read ignores controls validity, selected profiles and controller defaults.

The core uses Unknown, Null, Entity and OpaqueWord tokens. Unknown and Null
require payload0; Entity identifies0..9; OpaqueWord preserves any32-bit value.
No token is dereferenced. The native adapter represents a known raw zero word
as Null and nonzero controls words as OpaqueWord, even if the bits resemble a
PS1 address. Missing fixed slot0 stays Unknown here; this stage does not assume
that an absent native record proves a cleared original resident record.

Native table12 is therefore always Unknown at present. Pending bit16
(MatchTeamReferenceWords) keeps that limitation explicit alongside existing
extension-settings bit1. Current ordinary receipts report pending17. Neither
unknown data nor the observed zero words in an original automatic-demo epoch
are used to invent selected-match pointer values.

The core rejects invalid side pairs, token domains and null arguments without
changing output; overlapping input/output storage is staged safely. The C++
session prepares this stage before publication and commits no live RNG,
controls or snapshot revision on a failed capture.

## Verification and stopping boundary

Public tests use invented data only. team_header_test.cpp checks raw input
domains, low16-bit arithmetic, status/lineup/entity effects, pointer-token
preservation, overlapping storage and refusal. team_header_snapshot_test.cpp
checks asymmetric8/11/12/15 rosters, changed rankings/order, mirror teams,
fixed-slot profile provenance, saved-control validity independence, warm
strategy separation and ownership across source destruction/copy/move.
The Team Select verifier checks every emitted effect in all four accepted
host receipts. Original source execution and native integration are separate
evidence tiers; native screenshots do not establish retail runtime parity.

The independent private source oracle checks the complete write footprint and
preservation of every other header/status/entity byte. It executes all153
reachable instructions while retaining the full156 denominator. Original
materials, generated code, RAM dumps and comparison artifacts remain ignored.

Later period setup can change direction, header flags and CPU starters through
65140/65070 before65B18 initializes players. Controller ownership, those period
effects, injury substitution, full player/entity bindings, court/camera/model
lifetimes and a gameplay loop remain unimplemented. The current native UI still
stops at an explicit partial match handoff. No emulator or wholesale generated
recompilation is embedded.

Checkpoint validation on2026-08-30:52/52 CTests pass in Debug and RelWithDebInfo.
The standalone core has1,836,991 checks; the snapshot integration has2,799
assertions. An independent comparison checks9,221 original-instruction cases
with74,775 native assertions, including complete destination preservation.
The receipt verifier rejects1,920 field/type/shape mutations and24 paired
whole-reader mutations. No mismatches or actionable review findings remain.

All98 Team Select/User Setup frames and264 arrow-flash frames repeat within
and across both builds. The98 images, four preexisting snapshot payloads and
flash schedules are unchanged from the previous checkpoint, apart from the
new stage and pending bit. Create Player retains27/27 deterministic scenarios,
753 projected vertices,251 primary packets and zero missing texture samples.
Real saves/configuration retain their bytes and timestamps. Existing global
instruction/recovery/progress totals are unchanged. The release executable
and desktop shortcut are refreshed.

Private evidence: Team Select Debug run20260830-213515-0fdee93e, release
run20260830-213538-de372fc8; Create Player run20260830-213501. Logs are under
.local/logs/team_header_*. Source and mutation reports remain under ignored
audit_a/team_header and audit_c/team_header_verifier_i1vc1w0p folders.

Work stops after this tested checkpoint at the user's request. Remaining
dependencies above are deliberately unfinished; no next milestone is started.

# Accepted-match presentation selection

2026-08-30. The ordinary exhibition snapshot now owns the presentation byte
written by FEONLY `80046D24`. This closes one frontend handoff field; it does
not launch gameplay or establish full Team Select runtime equivalence.

## Source boundary

The accepted state5 Start path calls cursor cue9 at `800375FC`, commits the
controller assignments, and returns6. Dispatcher `8003F7C8` then calls
`80061674(0)`, `80046D24`, and `8003E7A8` in that order. The cue can consume a
shared RNG draw before presentation selection. A muted cue consumes none;
successful voice allocation consumes one even if later submission fails.
Bank/voice allocation failure is a separate source branch, not a reason to
skip presentation selection.

`80046D24..80046D9C` contains30 instructions. It reads the unsigned halfword
at frontend context+78. Exactly mode1 first masks the selected schedule record's
byte+2 with `0x60`. A nonzero result consumes no RNG. Otherwise it repeatedly
calls the existing six-word `8007A538` stream until `draw & 0x60` is nonzero,
then stores that byte at `80021DF4`. The possible values are `0x20`, `0x40`,
and `0x60`; their later presentation meaning remains opaque here.

The schedule lookup uses context+14, the season pointer at+468, the unsigned
record index at+46E and a four-byte record stride. The C helper accepts the
resolved schedule byte, so it does not claim ownership of those pointers.
Only ordinary mode0 is connected to the current match snapshot. Source-mode1
tests do not enable season launch.

## Native ownership and failure policy

`nba97_match_presentation` in `recovered/match_setup.c` owns the mask, branch,
rejection loop and resulting byte. It calls the existing Team Select RNG
helper; it has no seed, attempt limit, alternate RNG or substituted value.
Its draw/rejection counts and schedule provenance are native diagnostics.

`buildMatchSnapshot` copies the caller's current RNG, finalizes controls,
selects the presentation value, and finalizes rules. It owns both the before
and after RNG words along with the result. `MatchSession::capture` completes
all validation and allocation before publishing the snapshot, live controls,
and advanced RNG. Failed native preparation preserves all three. The earlier
confirmation cue remains a separate event and is not rolled back.

This transactional refusal is a native safety policy, not an original
allocation-failure behavior claim. Special teams, season/playoff requests and
unresolved created-player roster membership still refuse explicitly. Modified
roster membership, rank recomputation, settings and generation receipts use the
existing owned snapshot path. The created-player catalogue remains retained.

Normal host confirmation passes the existing frontend RNG member. Reentry does
not initialize it, and selection draws do not increment the cursor-cue counter.
Successful publication clears only `MatchPresentationVariant`; extension
settings and any unresolved created membership remain pending.

## Evidence and verification

| Evidence | Result | Limit |
|---|---|---|
| B original owner and original RNG versus current C | 1,584 cases /23,760 assertions | Invented contexts, schedule bytes and seeds; no live accepted match |
| A independent original/C and caller audit | 78,192 cases /625,433 checks | All unsigned modes, schedule masks, carry/rejection cases and30 caller-fragment cases; controlled memory/helper boundaries |
| A accepted Start through cue and selection | 16 cases /128 checks | Four seeds and muted/unmuted source settings; voice allocation accepted, wait/text cleanup hooked |
| Public C test | 16 seeded branch vectors,512 flag cases,65,536 unsigned-mode cases and null refusals | No original data embedded |
| Snapshot test | Pure preparation, exact rejected draws, continuation, owned receipts and atomic refusal | Synthetic database and isolated paths |
| Host capture contract | Four actual accepted snapshots plus special-team refusal | Native handlers, not physical keyboard delivery or an original runtime trace |
| C independent publication/verifier review | 13 publication assertions;12/12 corrupted receipt pairs rejected | Extracted actual methods with preparation/allocation failures; no original runtime claim |

The host verifier records the stream before Start, independently checks cue9's
draw, then checks every rejection, accepted mask, final six-word state and
published value. It rejects extra draws or skipping an earlier nonzero result.
The initial test incorrectly expected the pre-Start stream to reach `46D24`
unchanged; the first mismatch was the confirmation cue's preceding draw.
Original `375FC` and the existing audio call chain confirm that ordering.
Only the test expectation changed; no compensating RNG draw was added.

Final Debug and RelWithDebInfo builds pass48/48 CTest tests each. All98 existing
Team Select/User Setup frames and264 flash frames repeat within and across
configurations. The four presentation receipts and shared RNG states agree;
the saved profile's native generated ID is checked against its actual store by
the host and normalized only for cross-process receipt comparison. Create
Player retains27/27 repeated scenarios,753 projected vertices,251 primary
packet/order records and zero missing texels. Real save/config bytes and
timestamps are unchanged. The historical rank comparison passes145/145 scores
and145/145 ranks. Existing global instruction/recovery totals are unchanged,
and the release desktop shortcut is refreshed.

Final private runs: Team Select Debug `run-20260830-205403-11e9c137`, release
`run-20260830-205436-0ee9c388`, Create Player `run-20260830-205352`. Logs are
`.local/logs/match_presentation_*`. C's independent receipt is
`team_select/audit_c/team_arrow_flash/match_verifier_pvdapo73/result.json`.

Private source receipts are under
`.local/verification/gameplay/audit_b/next_frontend_configuration/` and
`.local/verification/team_select/audit_a/handoff_variant/`. Original code,
source executors and runtime material remain ignored. The full30-instruction
owner is inventoried with zero new credit. The shared52-instruction RNG and
1173-instruction dispatcher retain their existing inventories.

For a later live comparison, stop at verified FEONLY `80046D24` entry and
`80046D8C` after the result store at `80046D88`. Record context+78, all24 RNG
bytes at `800C73E4`, the selected result register and `80021DF4`. Mode1
additionally needs the actual schedule pointer/index/record. This checkpoint
makes no GPU, SPU, wall-clock,
universal RNG-history or complete readiness-to-loader claim.

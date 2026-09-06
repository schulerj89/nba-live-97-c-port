# GAMEONLY match audio service recovery

`nba97_game_match_audio_service` owns the complete GAMEONLY routine at
`0x8002A264..0x8002A463`, 512 bytes and 128 instructions. The fresh Ghidra
listing is identified by instruction-byte SHA-256
`68e4fe53f12e6dbedafb6b16b9afa76973d43663a95d8badc90ad4bfe120f573`.
Its observed callers are `0x8002DE5C` in the recovered match-service
publication wrapper and `0x8002A1F8`.

The owner preserves the source frame, clock publication, mode branches, and
all delay slots. It calls the clock service first, computes the wrapping
32-bit delta against `0x800E430C`, and reads the signed mode from
`0x800FDA0C`. Mode 1 optionally clamps a signed timer of at least 480 to 120,
subtracts the live delta into the low halfword, and starts the timeout service
only when the resulting low half is negative. Mode 2 checks the live audio
result, clears through callback-live `s0` when the result is nonpositive, and
performs the two teardown calls. Mode 3 retains the nested stream eligibility
and readiness decisions, either pumps and copies `0x800FDA10` or decrements
the timer by callback-live `s1`, then cues phase `0x82` or clears the mode when
the signed timer is negative. Signed modes outside 1 through 3 return after
the common frame restore.

All eleven original callees have full-machine boundaries. The production
adapter composes the existing complete `0x800A5810` clock-read owner at the
exact `0x8002A270` event and the complete `0x8008472C` stream-status owner at
`0x8002A2DC`. The clock leaf shares AB's `GameMatchClocksMachine`, so its raw
counter knownness, all GPRs, and HI/LO pass directly into AB. The status leaf
has no calls or HI/LO instructions, so the adapter copies its full GPR result
and preserves AB's HI/LO pair. The existing
`0x80083EEC` stream-pump owner is intentionally left behind AB's full-machine
callback. Its public API carries all GPRs but its child callback contract has
no HI/LO channel, so direct composition could lose child mutations. A future
bridge may use that owner only if it explicitly returns unknown HI/LO or adds
a source-proven full-machine path; it must not assume ABI preservation.

The adapter also accepts the actual recovered AA
`Nba97GameMatchServicePublishEvent` at `0x8002DE5C`, validates its NOP delay,
no-argument entry and `ra=0x8002DE64`, and shares the complete retained-memory
and machine state through AB. The other nine child results remain explicit
typed services; no audio, streaming, or cue algorithm is substituted.

The focused synthetic test covers signed modes -32768, -1, 0, 1, 2, 3, 4 and
32767; clock wrap; timer 479/480, 0/1/0x7FFF/0x8000/0xFFFF and negative delta;
every mode-3 eligibility/readiness arm including `INT_MIN`, 1 and 2; phase
`0x82` cue versus reset; positive, zero and negative mode-2 results; exact
arguments and call order; callback-live `s0`, `s1`, saved frame, all GPRs and
HI/LO; every child refusal; every operation-budget prefix on the longest
path; partial knownness, unknown stores, aliases, alignment, mapping, wrapped
SP, callback-relocated live frames, every non-restored GPR, and unknown JR.
Runtime fixtures contain no retail assets.

The integration test executes AB with the actual AC and X owners, checks all
five counter extremes and all sixteen counter knownness masks, verifies both
nested leaf failure prefixes, exercises the direct `0x8002DE5C` adapter guard,
and executes the actual recovered AA owner through AB into AC and X. Strict
MSVC C11/C++17 `/W4 /WX` builds pass 170 focused checks and 102 integration
checks. A separate
manager-owned original-instruction differential passes 4,028 cases across all
128 source PCs, comparing full memory, all GPRs, HI/LO, callback entry state,
mode and timer branches, wrapping behavior, mutable live registers, and every
operation budget.

Visual classification: **no direct visual effect**. This routine changes CPU
audio orchestration state through typed service boundaries. It neither proves
audible playback nor renders or advances gameplay, so no screenshot is
claimed.

Manager acceptance also passes strict C99, all 249 asset-free CTests, and the
progress, recovery, instruction-semantics and roster metadata freshness checks.
Native input run `game-entry-20260905-213012-1390ed17` executes the actual
publication child in three explicitly initialized audio modes. Mode 1 clamps
480 to 120 then subtracts 22, producing 98; mode 2 clears its state after the
explicit nonpositive backend response; mode 3 uses the real stream-status leaf
and wraps 1-22 to 0xFFEB before clearing state. The real clock leaf reads 1022
in all three independent period fixtures. The main frontend diagnostic keeps
its separate speech counter at 1241. Child return registers and frame restores
are asserted without inventing a live match continuation.

Gameplay shown: NO - no direct visual effect. CPU scanout before/after SHA-256
matches `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The separate ignored frontend screenshot shows Boston/Chicago User Setup;
there are still zero advancing match steps or rendered gameplay frames.

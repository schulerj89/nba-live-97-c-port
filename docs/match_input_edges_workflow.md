# Native match input edges

`updateMatchRuntimeInputEdges` binds the complete original GAME700E4 and its
internal GAME7A498 direction helper to owned `MatchRuntimeState` records. The
caller supplies an explicit controller reference, the full mapped word from
original2D2DC, and the knownness of the three original camera inputs. The bridge
does not assign host keys or establish the initial camera state.

Only the selected controller is projected. Entity-table references remain
separate from physical entities, so selection can follow an alias into any of
the eleven owned records. The bridge imports each recorded source write into
candidate controller/entity records and publishes only successful completion.
All other records and per-byte knownness remain unchanged. A refusal retains
the source receipt without publishing its writes; this is native transaction
behavior, not a claim that the original rolled back. Invalid native metadata
is rejected even on the write-only entityE4 field when that store is reached.

The original signed previous-mask load affects the full32 returned edge mask,
independently of its low16 controller34 store. Contradictory directional bits
keep their literal priority, neutral direction8 can complete without camera
or controller3C values, and both controller2A stores remain in the receipt.
Held400 re-reads selected26 and the live table instead of treating the selected
slot as a physical player ID. These behaviors must survive later input-loop
integration; none is normalized to a host input convention.

The isolated Debug and Release builds use `/W4 /WX` and pass131094 assertions,
including all65536 low16 held masks against an independent rising-edge formula,
full32 return cases, table aliases, camera cases, and atomic native refusal.
The recovered C owner separately has original-instruction comparisons; see
`game_player_input_workflow.md`. This bridge does not yet run686B8,61760,
physical polling/mapping, jump, audio readiness, countdown, or a match frame.

An independent review additionally compared1024 bridge calls per build against
the original700E4/7A498 instructions: all3731456 controller/entity bytes,
8497 ordered events, full32 masks and table aliases matched. Thirteen directed
cases checked knownness, native refusal atomicity and unrelated-state
preservation. The review found no actionable defect. Its private receipts are
under `player_input/root_edge_review`; this does not extend the runtime scope.

# Team Select arrow flash and bounded text history

2026-08-30. This checkpoint implements directional arrow flashes and the
bounded text history needed to avoid inventing inherited colors. It reuses the
existing tint kernel. Full Team Select equivalence and gameplay remain pending;
new instruction credit is **0**. Addresses and offsets below are hexadecimal;
object, update and assertion counts are decimal.

## Implemented boundary

`recovered/team_select_text.c` owns 200 metadata slots, 255 group heads and three
layer lists alongside `Nba97ReorderTint`. Slots distinguish permanent, retiring
and free lifetimes. Allocation scans inclusively from the last chosen slot;
retirement is not immediate reuse. A presentation frees retiring nodes without
one last tint tick, then updates remaining nodes in layer order. Exhaustion or
invalid inputs refuse transactional allocation changes.

The model represents these source operations:

| Operation | Allocation and lifetime behavior |
|---|---|
| Entry | Cleanup, 22 descriptor label/value nodes, selected-group pulse, four arrows, two name replacements, then heading: 29 creations |
| Accepted Left/Right | Six active-side values from `4EF40`, then the selected value again from `3D930`: seven replacements |
| Random | Six value replacements per accepted candidate; no extra selected-value replacement |
| Focus/Cross | Old-group unpulse/new-group pulse; Cross preserves arrow identities and tint while placement moves them |
| Help | Six layer2 nodes created after terminal growth; retirement immediately detaches group heads, while slots stay unavailable until the first shrink text pass |
| Exit | Retire all after the input-change barrier, before the separate final cleanup presentation |

Mode2 copies retain color state and channel knowledge while excluding movement.
Labels and values remain distinct. The source copies both primitive pages'
first-glyph colors onto the new uniform text; this metadata boundary excludes
arbitrary glyph-range coloring, cropping, raw heap pointers and GPU geometry.
Placement continues to use `team_select_placement.c`.

## Unknown colors stay unknown

Normal native startup creates an **unanchored native epoch**: inherited start
colors are unknown and hint0 is bookkeeping, not an observed PS1 allocation
index. Internal placeholder bytes are never color evidence. Explicit test seeds
provide 600 stored RGB bytes and a hint for an all-free source-domain premise;
they are not installed as a universal runtime initializer.

Each tint has three-bit channel masks for start, alternate, target and visible
RGB. Fresh glyphs have known visible128 on both pages even when stored start
RGB is unknown. Setters update endpoint knowledge without drawing immediately.
Interpolation at its endpoint needs only the target; an interior update needs
both endpoints. The source's red-channel preservation quirk is retained.

For a first flash with entirely unknown inherited start and no retrigger:

| Arrow updates after the setter | Knowledge and display |
|---|---|
| 0 | Existing neutral128 glyph remains visible |
| 1..3 | Computed inherited-color channels are unknown; native renderer uses neutral fallback |
| 4 | Visible gold `(120,102,0)` is known regardless of inherited start |
| 5..15 | Visible gold remains known; stored start G/B are known, R may remain unknown |
| 16 onward | Actual hold-to-return transition establishes all start/target channels |

Alternate stays unknown until an operation actually writes it. Hold retriggers
can postpone the transition, so masks follow kernel branches, not a separate
16-frame timer. Unpulse cleanup alone does not establish stored start red.
Fallback never writes back into tint state. For partial knowledge, the renderer
uses exact known channels and neutral128 only for unknown channels.

The tracked route retains its metadata across ordinary Setup/Team Select round
trips. Entering other menus, including User Setup, invalidates the prediction
and begins another unknown epoch. Full frontend/heap history is not inferred.
The original captured route had zero starts for its four arrows, but its preceding
warm Setup pool contained 50 nonzero stored triples. Neither observation proves
a universal zero or128 seed. See `team_select_text_history_workflow.md`.

## Flash, rendering and presentation order

`3D534` selects index `2*page` for exact Left8, or `2*page+1` for exact Right4.
It follows the callback and its nonzero sound latch. Muted audio or device
submission failure does not suppress a flash. Chords, Up/Down, Cross, Random,
Help, Start and Select do not trigger this path. Recovered polling/repeat
dispatch remains authoritative; OS key repeats are not separate flash events.

The shared `roster_reorder.c` kernel preserves fade4, hold10 and return4, with
separate transition/cleanup updates. Fade retriggers do nothing; hold retriggers
reset elapsed only; return retriggers preserve the source red-channel quirk.
Without retrigger, gold is reached at update4, hold begins at5, return begins
at16, neutral is reached at20, and update21 copies the final color and clears
animation. `2AEAC` is inside `2AE5C`, not a separate arrow-update owner.

Four persistent states tick even when offscreen, during Help and Random waits.
The existing single visible RGB triple matches the completed current primitive
page in this constructor/flash/movement domain; the private oracle retains both
original pages. Arbitrary unequal-page restores are outside that representation.

`main_menu.cpp` uses original glyph RGB with per-channel multiplication by
modulation/128 and clamps the resulting pixel. It does not replace glyph colors
through a luminance shortcut. The host ticks text before composition, retains
that completed frame, then samples input and invokes callbacks. A new flash
cannot recolor the already completed poll frame. Repaint and transition preview
do not advance live history. Help's terminal full-box frame precedes its text
creation; exit's final cleanup frame contains no retired text.

## Evidence tiers and current verification

| Evidence | Result | What it establishes |
|---|---|---|
| B current shared C versus original MIPS | 207,215 assertions; 518 direct and 32 four-arrow layer sequences | Explicit starts, both page parities, retriggers, Cross and cleanup; controlled heap/glyph/GPU boundaries |
| B knownness oracle | 452,244 assertions /480 two-node sequences | Eight seed masks, varied unknown bytes, pulse/unpulse/flash/mode2 transfers; finite witnesses plus source transfer rules |
| B extracted current host/renderer | 3,147 assertions /10 cases | Actual scheduling/render-call fragments with real C modules; invented assets and drawing/audio/title hooks |
| A source/captured-seed oracle | 12,612 assertions | 40 flash cycles, 72 caller gates, 24 held-Left dispatches and Cross retention; callback/redraw/I/O/GPU fixture boundaries remain explicit |
| C current metadata versus original MIPS | 2,621,715 assertions /2,945 state comparisons /24 lifecycles | Seeded zero/wrap/pattern/warm pools, entry/focus/directions/Help/exit/reentry; all200 slots and lists within the bounded metadata projection |
| C metadata versus scheduled original RAM | All200 lifetimes/start colors, occupied active tint fields, all255 heads and hint35 | Independent replay reaches the captured first-Left state; no GPU or wall-clock equivalence |
| A independent retirement comparison | 6,189 checks /9 snapshots | Help detachment versus layer/value retirement, including same-group recreation before old slots are freed |
| Original first physical Left, post-setter | 109 checks | Runtime descriptor/callback fields, setter state/pages and seven new-slot observations |
| Original later post-Left state | 76 checks | Stable terminal fields/pages of all four arrows; **not an elapsed-update count or live-cycle timing proof** |

**Full original Right flash capture: PENDING.** Replace this status only with
validated stops/receipts; source predictions and later stable states do not
substitute for its complete update sequence.

The first register-based debugger condition was accepted but did not stop;
the subsequent memory-mask expression was rejected by the actual UI. Neither
is a working capture procedure. Continue with plain address breakpoints:
`8003D5D8` after Right scheduling, then `8002B1D8` after animated text updates.
At the latter, inspect T2 to distinguish the arrow from the selected text;
the captured right-arrow pointer is `801FA1D8`. Revalidate identities and FE
code before reuse. These stops establish text state before submission, not
GPU pixels or physical presentation timing.

Debug and RelWithDebInfo builds pass **47/47 tests each**. The existing
**98 capture checkpoints** and **264 flash-case frames** repeat within and
across both configurations, including original-glyph per-channel pixel checks.
Create Player retains **27/27 repeated scenarios**,753 projected vertices,
251 primary packet/order records and zero missing sampled texels. Real save
and config bytes and timestamps remain unchanged. Metadata/instruction totals
are unchanged; the release desktop shortcut is refreshed.
Use `scripts/verify_team_select.ps1` with fresh isolated paths as documented in
`team_select_workflow.md`; never use real saves as test destinations.

Final private runs: Team Select Debug `run-20260830-202847-7514182f`,
RelWithDebInfo `run-20260830-202842-a37424cb`, Create Player
`run-20260830-202839`. Logs use `.local/logs/team_arrow_flash_*`.

Private receipts remain under `.local/verification/`: B's
`gameplay/audit_b/team_arrow_flash/` holds kernel, knownness and extracted-host
results; A's `team_select/audit_a/arrow_flash_runtime/` holds source vectors and
the scheduled/post-Left comparisons. C's `team_select/audit_c/team_arrow_flash/`
holds `native_history_diff.json` and the source adapter. Original RAM, reference media and oracle
implementations remain private. Public fixtures use invented premises.

## Ownership and remaining limits

These supporting extents overlap existing inventories; they do not create a
new whole-game denominator. Every row retains zero new credit.

| Owner | End exclusive | Full instructions |
|---|---|---:|
|8002AC2C|8002AC90|25|
|8002ADEC|8002AE5C|28|
|8002AE5C|8002B1E4|226|
|8002C244|8002C4B0|155|
|8002C6B0|8002CEBC|515|
|8002D348|8002D5F4|171|
|8003D434|8003D534|64|
|8003D534|8003D5F0|47|
|8003D930|8003E620|828|
|8004FCD8|8004FDE8|68|

Still pending: complete heap/frontend producer history, arbitrary text/primitive
states, full original Left/Right/retrigger/Help/Cross scenarios, synchronized
GPU pixels and physical timing, broader audio equivalence and physical input
coverage. The metadata/renderer tests do not establish SPU output, original
wall-clock cadence, all Team Select behavior, or a playable gameplay handoff.

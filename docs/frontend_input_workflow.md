# Frontend input: bounded presentation and topology recovery

2026-08-30. The native Team Select route now samples complete held masks at
recovered presentation boundaries. User Setup retains shared input history,
debounces observed topology, and uses the dispatcher's Cancel barrier. This is
source-derived behavior with independent controlled-MIPS comparisons, not a
live original timing, GPU, audio or physical-controller comparison.

## Owners and boundaries

| Original function | Native scope | Accounted / full instructions | Shared dependencies and evidence | Remaining uncertainty |
|---|---|---|---|---|
| FE8003AE4C | team_select_poll.c physical-pad sampling, first nonzero controller, history and repeat delays |0 /210 |76198,39574; team_select_poll_test and private source oracle |Queued tokens, complete state0 polling, driver/cache refresh, original phase |
| FE8003D930 | State3 whole-mask dispatch, postwaits, both exits; exact state0 Start history fragment |0 /828 |3AE4C,3B194, callbacks; poll tests and88 host captures |Other descriptor routes, movement/text cleanup, arrow lifecycle, source GPU phase |
| FE8003B194 | Mandatory presentation, then remembered mask change; physical controller or aggregate8 |0 /21 |39574,76198; poll/session tests and original-MIPS barrier probe |Original driver/cache and shared presentation lifecycle |
| FE8003F7C8 | Exhibition transitions and state5 Cancel fragment3FCF4..3FD18 |0 /1173 |3D930,4FCD8,37010,3B194,61674; host scenarios |Full dispatcher, other modes and gameplay launch |
| FE8004F934 | Twelve random candidates; owner waits1..12 |0 /41 |Six-word RNG,39574; random/poll tests |Runtime seed history and absolute presentation phase |
| FE80037010 | Topology observation, retained row placement and shared entry/Cancel history, alongside existing state5 logic |0 /1716 |36CA0,36B80,36898, modal owners; session/placement tests and source oracle |Text lifecycle, physical rebuild cadence, original runtime |

Only3B194 and3F7C8 are newly listed in frontend_input.json:1194 full caller
instructions, all zero credit. The other functions retain their existing Team
Select/User Setup ledger denominators and are references only. The64-instruction
topology region373DC..374DB remains inside the full1716-instruction owner.
These overlapping bounded inventories must not be added as whole-game progress.

## Polling and exit behavior

The shared context is800214F0. Cold bytes+71B (controller),+724 (mask), and+752
(repeat counter) are zero. Reopening Team Select resets its loop, not this
history. A poll presents first and selects the first nonzero physical pad0..7;
it does not combine controllers. An empty poll sets only the prior controller
to255. An unchanged mask/controller increments the counter by2 while below48;
the source's odd47 ->49 is preserved. A changed mask or controller resets it.

Left/Right wait7,5,3,1 presentations at counter boundaries15,27,37. Up/Down wait4.
Other masks intersecting3E50 wait5, including ignored chords. Help, Start and
Select have their separate paths. Each subsequent ordinary poll presents once
again. Releasing a key during a postwait does not change sampled history.
The C movement gate accepts the source text-motion condition; the fixed native
composition currently has no queued movement and passes false. This does not
claim the original text list has been recovered.

Random chooses its first candidate immediately. Its12 owner waits sum to78;
the descriptor caller adds5 and the next poll adds1. A key held through the
random animation is therefore sampled after84 minimum presentations, excluding
pending text work. Help owns its modal presentations, grows before its first
display, and writes the actual acknowledgment mask back to shared history.

Both Team Select Start and Select update remembered regular teams before the
exit sound and barrier.3B194 presents at least once and waits until the initiating
pad differs;3D930 then owns a separate final presentation. The exact Game Setup
Start path updates history twice (a changed Start reaches counter2), plays sound9,
and uses the same barrier/cleanup. Other state0 polling/history remains pending.

State5 entry inherits mask/controller; it does not replace them with80/0.
Accepted Start preserves history. Select clears choices and stores controller8
and mask100; dispatcher3FD10 calls3B194. The host presents and waits until the
aggregate differs from100, without another controller pass or a second cleanup
presentation. Aggregate input includes every physically present driver slot,
even when excluded by topology. Disconnected stale masks are suppressed, matching
the driver's normalized output, not a topology filter. A changed nonzero mask
may satisfy either exit barrier; release to zero is not required.

The Win32 keybank records keydown/up before fade and autorepeat guards, then
updates complete normalized masks. Releasing one alias leaves another held alias
active; focus loss clears held keys. Team Select actions come from the polling
core, not WM_KEYDOWN repeat. Setup's pending Start also freezes mouse hover,
clicks and card-changing adapters, so mode cannot change after route acceptance.
The host uses a nominal1001/30ms cadence, advances
at most once per update, and waits for the prior paint. Physical delivery and
original VBlank/IRQ timing remain separate pending evidence.

## Topology observation

State5 enters with active99 and countdown-1. Each new outer iteration observes
the two driver words; exactly8000 means a multitap. Matching the active topology
sets countdown3. A different observation decrements3,2,1,0 to2,1,0,-1, then adopts
on the fifth observation and resets3. Even a changing alternative is adopted
when the old countdown is negative; it is not a new five-observation timer for
each candidate. The host primes the first observation for entry composition,
then consumes that same observation on the first step without polling twice.

Observation precedes global Start/Select and the signed elapsed>6 row gate.
Help/dialog continuations and terminal returns do not start another outer
iteration. Connectivity remains live inside a suspended pass, while its row
order retains the previously adopted topology. The portable placement owner now
hides all8 text groups and all15 marker objects on adoption. Each timed row tail
restores its targets; a blocking child suspends before that same row's tail.
Connectivity is checked when a row starts, not again when its child returns.
The renderer consumes these retained targets without another connectivity or
neutral-pulse filter. Recreated editor labels restart their cursor tint without
erasing draft/profile/cursor state. See user_setup_placement_workflow.md for the
bounded target contract; original text allocation and physical cadence remain pending.

## Evidence and first mismatches

Public tests exercise every normalized mask, repeat boundaries, first-controller
priority, postwait release, modal/reentry history, both exit paths, random caller
ownership, changed nonzero handoffs, and topology/modal continuation ordering.
The88 host scenarios include held input before/after polls, chords, focus/alias
handling, both Team Select exits, Setup Start, User Cancel, topology changes and
the existing isolated persistence/snapshot flows. Two independent processes
must agree; hashes are never adopted as retail fixtures.

Private compiled-C comparisons pass55,588 assertions for sampling, dispatch,
topology, exit tails and random timelines. A separate barrier probe passes7,985
assertions over1,632 original state0 Start-history cases and768 original
3B194 ->76198 ->78330 ->781DC aggregate cases. The source fixtures control driver
state; cache/timer refresh is outside the equivalence claim. Artifacts and source
hashes are under .local/verification/team_select/audit_a/input_followup/.
The extracted-host/real-Session review passes60 assertions. A separate private
verifier probe accepts the valid76-frame capture and rejects57 altered states or
ledger claims. Neither review substitutes for a physical or original runtime run.

The first host mismatches were (1) a key pressed during a fade disappearing
before the first poll, and (2) User Cancel changing page immediately rather than
executing3FD10's mandatory presentation/change barrier. The keybank now records
before fade guards, and Cancel stays on state5 until that barrier completes.
The earlier random helper also combined78 owner waits with6 caller/poll waits;
those owners are now separate so held input is sampled at the correct boundary.

## Original observations still required

At3AE4C entry/return capture80021C0B length1,80021C14 length2, and80021C42 length2
with the normalized physical masks. Compare a single held Left through successive
presentations, then one release. At3B194 entry/return record the same mask and
controller fields and count39574 calls separately from3D930 cleanup. Use a newly
validated emulated RAM backing, not an old host address.

Arrow construction is a further explicit gap. The source creates visible gray
glyphs but preserves stored tint-start bytes in reused text nodes. Prior flashes
can leave gold history; initializing all stored colors to gray would invent
behavior. Break at3D534 and its2ADEC call, dump the actual node including offsets
30..32, and follow2AE5C updates with the node's flags. Compare the first differing
field before changing rendering. Full text/arrow allocation and physical topology
rebuild timing remain pending, as do synchronized visuals, sound bank/tone/
pitch/sample timing, and a physical keyboard/controller walkthrough.

The first original pre-selector RAM observation found200 free text slots, each
with stored start RGB0/0/0. This narrows the cold heap observation only: Game Setup
and transitions can reuse slots before Team Select creates its arrows. It does
not justify initializing every native arrow to zero or declaring its tint history
recovered. Capture the actual selected arrow nodes before the first direction.

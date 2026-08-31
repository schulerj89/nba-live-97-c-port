# Gameplay substitution owner

`src/recovered/game_substitution.c` recovers the complete249-instruction GAME649D8 direct owner. It changes live typed state and invokes the original callee boundaries synchronously. It does not replace those callees with queued events or guessed success. `NBA97_SUBSTITUTION_OK` means the direct owner and every requested callback completed according to the caller's contract; it does not establish that any external callee is itself recovered or that a whole period runs.

The native state includes both teams' lineup halfwords and header fields14,34,77,A2,C0,C2; duration58/remaining60; globals6C,92,8E,90,86,A8,54; message8BC/player8C8;24 status+20 halfwords; and the ten-entry entity table and physical entity DE/DF bytes. These must come from actual owned state. Unknown source fields must be resolved or explicitly refused by the integration layer, not filled with fabricated defaults. Other callee-owned fields remain in callback context.

## Exact ordering and source quirks

The owner saves the entry values of globals92 and6C, sets92 to1, and sets headerC0 to1800. When duration differs from remaining it decrements headerC2 with16-bit wrap. It performs the original repeated clock comparison. Depending on the signed reason, signed marker8E, marker bit10, signed phase90, and31CB8's low return byte, it can call audio29258(12),64914,64964, and another64914. The repeated64914 call is preserved. Negative reasons also set message8BC to low16(reason+20), set player8C8 to low16(unsigned header14+active slot), then call62BFC and64964.

HeaderA2 clears before the lineup loads. Bench loads first, then active; the original swap follows. Active slots below5 resolve `entity_table[unsigned header14+active]`, clearing DF then DE. Slots5..11 do not touch an entity here. `side` selects an owned header; its actual header14 is independent and is never synthesized from that selector.

After the swap the source rereads both clock words. Midgame nonnegative reasons with marker bit10 clear call35378,29258(12),64964, then set phase90 to128, decrement header34 with8-bit wrap, set delayA8 to300, and OR bit10 into the current marker. Changes made by callbacks therefore affect those later reads. Header77 and the clocks decide whether announcement calls follow. Negative reasons enter that announcement path directly when the outer midgame test passed.

An announcement calls29258(14) only when flag86 is zero, then writes flag86=1 even if that callback changed it.353A0 receives signed header14 and the current signed active/bench lineup values.7F84C is chosen for any nonzero `first`; otherwise7F914 runs. Those calls reread the active lineup after353A0 and receive unsigned header14, unlike353A0/35378.64964 follows.

For a negative reason, the owner scans lineup positions11 down toward the bench position, excluding that bench position. Negative player halfwords and negative status halfwords are skipped. The first accepted row is swapped with the bench. Source header14 equal to zero selects the home status bank; any nonzero value selects the away bank. A positive home player value12..23 crosses into the owned away status records; this source quirk remains supported. A reached status reference beyond the24 owned records is an explicit native guard, not a repaired player.

Finally646A8 runs. The owner reads lock54 after that callback, calls63EDC only when it is zero, then always callsA584C. Only after A584C completes are the original entry92 and6C halfwords restored. These saved values override mutations to those globals during callbacks.

Signed phase values with bit15 set are less than128 in the original LH/SLTI branch. Query31CB8 is masked to its low byte, so0x100 behaves like zero. No signed branch, wrapping decrement, redundant call, or retained callback mutation is silently corrected.

## Callback contract

Each boundary receives the same live state and a call with an owner, argument count, and exact32-bit argument words. Unused argument-array cells are not representations of leftover MIPS registers. The established semantic inputs are:

| Owner | Inputs supplied by649D8 |
|---|---|
|31CB8|None; requires an explicitly known reply word, of which only low8 bits are consumed|
|29258|One unsigned selector:12 or14|
|64914,64964,62BFC|None; consume current owned state through the callback context|
|35378|Signed header14, sign-extended to32 bits|
|353A0|Signed header14, current signed active lineup, current signed bench lineup|
|7F84C,7F914|Unsigned header14 and current signed active lineup|
|646A8,63EDC,A584C|None; consume current owned state through the callback context|

Returning1 asserts that the requested callee and its transitive work completed and that all changed source fields are reflected in the live view before the callback returns.31CB8 additionally requires `reply.value_known=1`. The return payload of other callees is not consumed. Returning0 stops the owner with `CALLBACK_FAILED`; it is appropriate for a646A8 divide trap or another unfinished source boundary. Missing callbacks and unknown query replies also stop explicitly.

Native guard or callback failure retains every preceding direct effect and callback mutation. The saved entry globals are not restored on failure. Do not proceed to the enclosing caller or retry the owner from its beginning as though nothing happened. An integration layer can stage owned state and callback effects to provide atomic publication; the recovered owner itself follows source order.

## Integration with lineup recovery and player bindings

The65070 callback already supplies owned side, signed active/bench slots, reason0, and its raw first-call selector. Bridge its current header/global/status/entity state into this substitution view. Before and after each substitution boundary, synchronize the relevant live fields with the caller's actual canonical state; callbacks may change later branch decisions.

For the646A8 boundary, execute the native direct player bindings against the current lineups, apply their masked effects, then execute `game_team_roles` with the actual bound data and known full incomingt6=8001F984. Update the enclosing lineup-recovery state's inverse arrays before returning:65070 consumes them immediately on its next visit. Inverse/preferred fields are deliberately not duplicate members of this substitution view because649D8 never reads them; retain them in the canonical callback context.

When duration equals remaining, the direct owner skips midgame presentation but still performs headerC0/A2 changes, lineup swap, applicable DE/DF clears, negative-reason cleanup,646A8, conditional63EDC, andA584C. Skipping those tail calls would not complete the source boundary. Binding divide traps return failure without running subsequent substitution callbacks or restoring the saved globals.

Root integration needs the new C source added to the recovered library and `tests/game_substitution_tests.cpp` added as a linked test target. No shared CMake, host, Git, or UI files were changed by this owner implementation.

## Evidence

Public tests cover period-start behavior, exact callback sequences and argument words, live state changes, signed marker/phase conditions,512 query words, wrapping counters, negative-reason bench cleanup, cross-bank status references, aliased/same lineup slots, native guards, and callback failure prefixes. All2,144 checks pass in private MSVC Debug and RelWithDebInfo builds with warnings treated as errors.

Private `.local/verification/native_completion/substitution/compare_original.py` executes actual GAMEONLY bytes for all249 owner instructions, including branch delay slots. External callees are explicit mutating test boundaries. The independent comparison checks callback order, argument words, full live state at each boundary, final state, and all nonstack RAM. It includes25 interrupted callback prefixes; these are explicit stop comparisons, not claims of original success after a failed callee. Both Debug and RelWithDebInfo receipts record1,618 cases,16,881 callback events, and249/249 executed source instruction addresses. No owner denominator was reduced.

The preceding independent gameplay setup review is in `substitution/setup_review.json`: actual C++ getters match all30 signed formation values,512 duration words, and168 original640D8-normalized motion views, including unchanged raw mode2. All64 formation bytes and both1024-byte windows match the private source pack. Extraction guards and no-overwrite behavior were reviewed with no findings. No new emulator capture or full period runtime claim is made.

# Gameplay team role helpers

`src/recovered/game_team_roles.c` closes the four calls at the end of GAME646A8:6459C(0),6459C(5),644FC(0),644FC(5). It executes the recovered behavior of both6459C calls and their64388 helper, followed by both644FC calls. The original owners contain67,93,and40 instructions respectively. No helper behavior is stubbed. This owner is specific to the646A8 call sites; it does not manufacture the register or stack context for unrelated callers.

Inputs describe actual owned data: player bytes0E/0F/17, the separate ten active-player references, bound entity player/status references, status byte1E, physical entity word00 and halfwordD6, the entity pointer-table mapping, and the known full incomingt6 word. Outputs are typed team-header effects, masked entity D4/CB effects, and relevant carried register words. These are source fields, not inferred gameplay labels.

## Preserved original behavior and bugs

6459C independently maps five entity-table entries. It copies each entity's D6 to D4, ranks the opponent's maximum player0E/0F, and adds100 when that opponent entity's bound status byte1E equals2. Byte1E is distinct from the availability halfword at status+20. Own-player byte17 uses strict greater-than; ties retain the first winner. All-zero ratings leave t6 unchanged. The first call in646A8 demonstrably inherits fullword8001F984, so its fallback header61 byte is84.64388 does not clobber t6; the away call inherits the home winner or the same full fallback word.

64388's sort is intentionally incomplete: four passes each perform five comparisons, and a swap does not advance the comparison cursor. For ascending scores1..5, first-phase IDs5..9 become8,7,9,6,5. The fifth comparison can read the saved return address as a sixth score. The actual646A8 call sites save800648E8 and800648F0, both negative as signed words; valid scores0..355 cannot lose to either, so the adjacent uninitialized sixth ID is never consumed. The implementation retains those source sentinel values, not host addresses.

After writing header5C, the helper encodes side-local IDs and subtracts100 from every score at least100, even if the score came from a raw rating with no status bonus. The second sort has the same cursor behavior. It writes headerBB and opponent entityCB ranks5 through1, retaining duplicate destinations and the last write. This phase uses a3, leaving t1 equal to the last first-phase comparison's left score.

644FC resolves only the first entity-table pointer for a side, then walks five consecutive physical records of strideF4. It does not map five independent table slots. It finds two player0F ratings with strict comparisons. A best/second gap of10 or more, or second rating below70, forces the second ID toFFFF. It never initializes t1: all-zero home ratings inherit the away6459C sort's score, and all-zero away ratings inherit home644FC's final t1. The full words are carried before the final headerA6/A8 halfword stores. Incoming t0 is a source stack address, but either candidate updates replace it or the secondary threshold replaces it withFFFFFFFF before any output; no invented pointer value is required.

These quirks are preserved and commented, not normalized or silently corrected.

## Integration contract

1. Apply a successful `nba97_game_player_bindings` result's direct effects to the current owned state. Its divide-trap result has no tail calls and must not invoke this owner.
2. Copy the ten active references from binding effects into `active_player_reference`. Copy each physical entity's actual word00 and D6, and its current bound player/status references after applying the masked binding effects. Do not equate active-table slots with physical entity identity.
3. Resolve player bytes0E/0F/17 from the same actual player pool as the binding references. Resolve all24 status byte1E values from actual status records. Preserve `entity_table` mappings and aliases. Do not substitute availability+20 or fabricated zero records.
4. For the proven646A8 tail, set `incoming_t6=0x8001f984` and `incoming_t6_known=1`. The binding result's byte84 alone is not the full register provenance. An unknown caller register is explicitly refused.
5. Call `nba97_game_team_roles` once to execute all four ordered tails. Apply team outputs to header5C..60,61,BB..BF,A6,A8; apply only marked D4/CB entity effects. This discharges the four pending binding calls, not other scene or period owners.

Safety guards are native ownership bounds, not recovered retail branches. They refuse missing storage, unknown t6, invalid consumed player/entity/status references, or a644FC physical span beyond the ten-entity pool. Original out-of-bounds behavior is not replaced with fabricated data. A guard failure leaves the output unchanged; the host should stage this boundary if atomic publication of the preceding binding effects is required. Input/output/player storage can overlap because publication follows all consumed reads. No memory beyond the typed output is written.

Root integration needs the new C source in its recovered library and `tests/game_team_roles_tests.cpp` as a test target linked to that library. This work makes no shared CMake, host, UI, or Git changes.

## Evidence

Public tests cover zero-rating register leaks, strict ties, every byte-domain leading rating across secondary thresholds, physical traversal, independent bound/active references, duplicate opponent writes, incomplete sorting, raw-rating subtraction, guards, and overlapping storage. All9,096 checks pass in private MSVC Debug and RelWithDebInfo builds with warnings treated as errors.

Private `.local/verification/native_completion/team_roles/compare_original.py` executes the actual GAMEONLY bytes from the646A8 tail entry, including every instruction of all three helper owners and branch delay slots. It installs no semantic callee hooks. The925 comparison cases cover all120 permutations of five ordered scores, all256 rating bytes,32 status patterns,512 randomized bounded alias/reference cases, and five named regressions. It compares all nonstack RAM, exact write footprints, and relevant register words at all four real return sites. Both Debug and RelWithDebInfo comparisons pass. All200 helper instructions are exercised; no denominator is reduced. These are native-versus-original synthetic-state comparisons, not a new emulator capture or a claim that the full period runs.

The preceding independent review of `game_lineup_recovery` is recorded privately in `team_roles/lineup_review.json`:64 additional raw-source cases and150 live callbacks matched, including callbacks that change subsequent eligibility, preferences, inverse mappings, and status values. No review defects were found.

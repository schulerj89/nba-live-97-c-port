# Sign Free Agent: initial source audit

Historical baseline, audited 2026-08-28. For the implementation and current
verification, see [Sign progress and workflow](sign_free_agent_progress.md).
**No instruction credit was awarded by this initial audit.** At that point,
the native Sign card reached `MENU-BLOCK` in
`completeRecoveredBottomSelection`. Existing Trade/Re-order tests are not Sign
screen verification. Names below are descriptive audit labels, not recovered symbols.

## Bounded instruction baseline

Fresh read-only Ghidra exports, checked against private recomp callback pointers:

| Sign-specific owner | Original entry | Instructions | Pending Sign accounting |
|---|---|---:|---:|
| Entry / normalize destination / bind callbacks | `80056F9C` | 22 | 22 |
| First selection: reject empty free-agent slot | `80056D6C` | 53 | 53 |
| Second selection: validate destination, transfer, refresh | `80056E40` | 87 | 87 |
| Sign card availability | `80057B00` | 27 | 27 |
| **Bounded total** | **4 owners** | **189** | **189** |

This is **162 screen-wrapper/callback instructions + 27 availability instructions**.
It is not the whole feature's transitive instruction count, a rewrite estimate,
or a fidelity percentage. Start Sign-owned accounting at **0/189**; shared source
already implemented elsewhere must not be credited again. Dependency integration,
assets, original-reference acceptance and persistence remain separate gates.

Counts include original NOPs, prologue/epilogue and delay-slot instructions once.
Do not count generated C++ lines or sum overlapping Ghidra basic-block ranges.
The callback pointers are `80056D6C` and `80056E40`, NOT recomp fragment starts
`80056D74` and `80056E48`. Their two preceding input-load instructions matter.

## Original flow and rules

- Rosters menu `80057CE4`: Sign is card index1; selector return3 pushes frontend
  state14 (`0x0E`) in `8003F7C8`. The dispatch call returns at `80040160`.
- Entry `80056F9C` normalizes the destination through `80056A94`, then calls
  `80056494` with **left team29/kind0; right destination team/kind1** and the
  Sign callback pair. Selector result is returned as a signed halfword.
- Descriptor builder `80056128` creates **100 free-agent slots** at table offset435
  and **15 destination slots**. Right object IDs therefore start at100, not15.
  Both lists show six rows. Capacity100 is not the number of occupied free agents.
- First selection rejects an empty source using notice `800AED20`. A valid source
  delegates to `80056B44`, including its mode/injury check and selection transition.
- Second selection first handles View/Compare. For confirm, selector field+0x11
  equal to2 bypasses mutation and returns through `80055314`; preserve this branch
  and determine its exact runtime context before simplifying it.
- Otherwise `800556B0` returns -1 for a destination with exactly15 players:
  notice `800AEC72`. An occupied selected destination produces `800AED88`.
  **Signing is a transfer into an empty roster position, not a player swap.**
- Success calls `800558E0`, sound6, clears the selector sound latch, refreshes both
  lists through `80055AF8(2)`, then returns to first selection via `80055314`.
  Compaction removes the source hole; selection and viewport after compaction need
  original runtime checks, especially near the last occupied free-agent row.
- Shared team scan `80055EF0` forces the destination-side scan when the first team
  is29; do not change the free-agent list into another team when scanning.

### Why Sign can be disabled

`80057B00` disables it when mode+0x78 is2 and flag `80021D96` is nonzero, or when
the first free-agent slot is empty. Otherwise it returns `8004BA78(0)`:
the count of empty team slots. Normal context scans all29 teams; mode2 scans its
16 context-selected teams. Zero vacancies disables Sign. This is not merely the
current team's capacity. The higher-level meaning of flag `80021D96` still needs
confirmation; do not invent a season/playoff label for it.

## Shared dependencies: reuse does not mean Sign is verified

Counts below describe the individual original bodies; they are **outside189**.
This is a first-level working map, not an exhaustive engine call graph.

| Shared function | Instructions | Existing foundation / Sign-specific work |
|---|---:|---|
| `80056A94` team normalization | 22 | Portable C helper; reuse signed sentinel/mode behavior. |
| `80056128` list descriptors | 75 | Existing specialized screens; bind100/15 storage and absolute object IDs correctly. |
| `80056494` constructor | 276 | Trade presentation/controller contracts exist; current Trade arrays and bounds assume15/15. |
| `80056B44` first callback | 67 | Trade callback behavior exists; factor/reuse without dropping source injury guard. |
| `800552B4` / `80055314` phase changes | 24 / 34 | Selection/tint/cancel foundations; verify100-row source and timing. |
| `800556B0` validation | 140 | `nba97_roster_validate` already has team29/full15 result; Sign notices still need wiring. |
| `800558E0` mutation | 134 | `nba97_roster_mutate`: insertion search, transfer counts, change counter. |
| `800555F4` donor compaction | 47 | `nba97_roster_compact` supports100-slot free pool; signing does not need team-starter repair. |
| `80055AF8` refresh | 115 | `nba97_roster_refresh_lists` supports typed capacities; bind/render six visible rows. |
| `80055EF0` team scan | 115 | Existing Trade scan is not proof of Sign's forced destination-side branch. |
| `80054B94` child request | 74 | View/Compare requests and empty notices; verify free-agent child paths. |
| `800560BC` / `80056254` enter/exit | 27 / 86 | Resume phase, remembered cursor/top, snapshot/discard, rebuilt indices. |
| `8005A3FC` / `8005A6F0` child returns | 79 / 100 | Previously audited returns; team29 paths require dedicated Sign scenarios. |
| `8004BA78` vacancy count | 74 | Needed for live card availability; normal435-slot and mode2 scans. |

Additional dependencies include shared headers/row drawing, scroll callbacks,
`8003D930` selector, modal/Help engines, palette/title animation, audio,
`80057810` index rebuild, portrait lookup and outer child routing. These are not
silently included in189. Add a separately bounded owner if one needs deeper recovery.

## Asset findings (all originals stay private)

State14 layout pointer at `80093330 + 14*4` resolves to **`80096E84`**,22 records:

- Title **`ba30` at (156,10), depth3**, not Trade's `ba38`.
- `Bkga`..`Bkgd` background strips; `brte`..`brth`, `brle`, `brri`,
  `brbe`..`brbh` border pieces. The free-agent background palette route needs checking.
- `frml` at(30,15), `frmr` at(368,15); dynamic portraits at(54,22)/(386,22).
  Constructor explicitly loads **Z2PORT.IDX / Z2PORT.BIG**; don't substitute Z1PORT.
- Plates `110p` at(40,16), `111p` at(370,16); Help footer at(235,217).
- State14 Help table **`800B1264`**: first panel **`800B1270`** rectangle
  (121,70,270,140), second **`800B1350`** rectangle(121,80,270,125).
  These are bounded modal panels, not full-screen replacements.
- Sign-specific notices **`800AED20`**, **`800AEC72`**, **`800AED88`** were inspected
  in the overlay. Existing Trade UI/Help extractors do not include these records.

Reuse the private sprite/font/portrait providers and bounded descriptor extraction
pattern. Add a small Sign UI/Help pack rather than copied strings or a monolithic pack.
Still verify the Sign header formats, font choices and palette mapping before drawing.

## Manageable implementation and verification order

1. **Entry + screen construction contract (22 owned instructions):** state14 routing,
   100/15 descriptors, true callback bindings, original title/border/portraits and CLI
   evidence. Credit only the wrapper; do not call the whole constructor complete.
2. **First selection (53):** valid/empty source, injury context, View/Compare request,
   six-row scrolling, cancel stage1/stage2. Test first/last occupied and empty tail.
3. **Second selection (87):** full-team/occupied-slot refusals, vacant destination,
   counters/compaction, both-list refresh, cue order and return to source selection.
4. **Availability (27 + shared vacancy helper contract):** free pool empty, no team
   vacancies, full current team but space elsewhere, mode2 restriction/eligible set.
5. **Integration gates, separate from instruction credit:** Help/View/Compare round
   trips in both phases, original undo boundaries, accept/discard/reopen/Reset,
   save failure handling, held inputs, matched screenshots and timed audio captures.

Useful CLI events: `SIGN-ENTRY`, `SIGN-SOURCE`, `SIGN-DESTINATION`, `SIGN-REFUSE`,
`SIGN-TRANSFER`, `SIGN-REFRESH`, `SIGN-CHILD`, `SIGN-SAVE`, `SIGN-AVAILABILITY`.
Include original address, phase, source/destination IDs, cursor/top/base, counts,
notice/cue IDs and save generation; avoid logging unchanged frames continuously.

### Persistence boundary

The existing v1 roster document already represents all535 slots, including the
100-slot free pool; signing conserves player population, so no schema change appears
necessary. Reuse generic prepared-slot validation and atomic save publication, not
Re-order's adapter (which intentionally rejects changed free-agent membership).
Do not serialize cursor state, callback addresses, textures or duplicated player data.
Test ownership/index rebuild, no duplicate/lost IDs, count conservation and restart.
Trade's observed post-child undo quirk is a reference-test candidate here, not yet an
observed Sign behavior, even though the constructor/exit helpers are shared.

## Evidence and repeatability

Private evidence: `.local/recomp/recompiled_full.cpp`, fresh
`.local/ghidra/sign_audit_20260828.{c,json}` and
`.local/ghidra/sign_dependencies_20260828.{c,json}`, plus earlier Trade callback
inventories. FEONLY SHA256:
`14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c`.

For a fresh inventory, run the configured Ghidra `analyzeHeadless` against existing
project `.local/ghidra/nba97_feonly_evidence`, program `FEONLY.BIN`, with
`-process FEONLY.BIN -noanalysis -readOnly -scriptPath tools/ghidra`, then:

```text
-postScript PrepareSignCallbacksHeadless.py
-postScript ExportFunctionSemanticsHeadless.py .local/ghidra/sign_inventory.json 0x80056F9C 0x80056D6C 0x80056E40 0x80057B00
```

Check that each exported entry equals the requested address, each has
`size_bytes == instruction_count * 4`, and counts are22/53/87/27. A changed body or
entry fails the baseline review; do not silently change the denominator. Raw bytes,
decompiled source, extracted text, screenshots/audio and saves remain under `.local`.
This audit did not run a Sign screen, capture original screenshots, or prove fidelity.

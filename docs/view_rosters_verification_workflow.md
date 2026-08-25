# View Rosters semantic verification

This workflow distinguishes a translated native implementation from the original
MIPS instructions without treating different compiler output as a failure.

## What is compared

The schema-v2 public contract is
`config/decomp/view_rosters_scenarios.json`. It declares:

- a finite inventory of currently implemented interactions;
- small scenarios that cover every inventory item;
- an event count and canonical SHA-256 over every observable state field after
  each milestone;
- the exact collapsed native semantic-checkpoint sequence;
- for the subset captured in no$psx, the required ordered original
  function-entry subsequence.

The digest is compact, but it is not a fuzzy score: any changed event name,
change/no-change result, mode, team, player, list window, stat window, layer,
display field, or help-modal state fails that scenario. Native state and ordering
must match exactly. An original no$psx trace may contain
extra calls, rendering loops, and repeated PCs; the required semantic entrypoints
must occur in order. This is intentional because the native C representation is
not instruction-identical MIPS.

The current denominator is 12 scenarios covering 16 inventoried interactions.
Do not interpret 100% of that denominator as visual identity or total game
completion. Visual similarity, original-trace coverage, instruction accounting,
and the overall port roadmap remain separate measurements.

The asset-backed gate also captures distinct roster-help, player-help,
stat-layer, roster-scroll, and player-stat-scroll frames. These are pass/fail
render regressions with zero score weight: they can fail the workflow, but they
cannot inflate visual fidelity. A known original Cool Fact record is likewise
decoded to its exact PSX-ADPCM rate, sample count, and byte count without playing
audio during the automated test.

## Private original evidence

Keep all original traces under:

`.local/verification/view_rosters/original_traces/`

They are ignored by Git and must not be committed. The verifier accepts ordinary
no$psx MIPS trace text; it extracts eight-digit PCs and maps any address inside a
tracked Ghidra function back to that function's entrypoint.

The verified no$psx 2.3 Player 1 keyboard mapping is:

| PlayStation control | Keyboard |
| --- | --- |
| D-pad | Arrow keys |
| Triangle | `F` |
| Cross | `C` |
| Square | `D` |
| Circle | `V` |
| Select | Right Shift |
| Start | Enter |
| L1 / L2 | `A` / `Z` |
| R1 / R2 | `S` / `X` |

Live breakpoint evidence distinguishes the shoulder actions in View Player:
L1/R1 scan teams, while L2/R2 change the statistic layer. In particular,
R2 changing `95/96 season` to `95/96 playoffs` enters `0x80059610`; ordinary
team scanning does not. Keep this distinction in scenario contracts and native
semantic checkpoints.

Do not use keypad keys in the debugger: no$psx reserves some of them for run and
reset commands.

## Original capture procedure

Use the local NBA Live 97 disc image and start each recording immediately before
the first declared input:

1. Enable the no$psx MIPS trace log and pause at the scenario's starting screen.
2. Clear or rotate the previous trace.
3. Resume, perform only the inputs listed in the public scenario contract, and
   pause immediately after the final milestone.
4. Save the two logs as:
   - `view_rosters_navigation.txt`
   - `view_player_wrap.txt`
5. Keep screenshots beside the logs when diagnosing a mismatch, but never commit
   them or any original assets.

If full instruction logging is impractical, define no$psx code breakpoints with
Ctrl+B at the addresses in `original_required_subsequence`, record each hit in
order, and resume with F9. The resulting text file is accepted by the same parser.

## Run the comparison

From the repository root:

```powershell
pwsh -NoProfile -File scripts/verify_view_rosters_semantics.ps1 `
  -NavigationOriginalTrace .local/verification/view_rosters/original_traces/view_rosters_navigation.txt `
  -WrapOriginalTrace .local/verification/view_rosters/original_traces/view_player_wrap.txt `
  -RequireOriginal
```

The native executable regenerates
`.local/reports/view_rosters_scenario_trace.json`. The comparator writes
`.local/reports/view_rosters_scenario_comparison.json`.
When the two conventionally named trace files are present in the ignored local
trace directory, the comparator discovers them automatically. Pass
`--no-auto-original` to test native coverage without that local evidence.

A pass means the declared state transitions and required original semantic path
agree. It does not claim byte-identical code, full CFG equivalence, timing
equivalence, or visual identity; those remain separate verification tiers.

The local comparison report records SHA-256 hashes for supplied original traces.
After reviewing a full two-scenario pass, change each corresponding
`trace_scenarios[].status` in
`config/decomp/instruction_semantics/view_rosters_mapping.json` from
`native_verified_original_pending` to `original_trace_equivalent`, then run
`python tools/verify_instruction_semantics.py` and
`python tools/report_progress.py`. Do not promote a scenario from native-only
evidence.

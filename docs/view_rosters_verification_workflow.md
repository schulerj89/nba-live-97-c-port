# View Rosters semantic verification

This workflow distinguishes a translated native implementation from the original
MIPS instructions without treating different compiler output as a failure.

## What is compared

The public contract is
`config/decomp/view_rosters_scenarios.json`. It declares:

- the input sequence for each scenario;
- the exact observable native state after each milestone;
- the exact collapsed native semantic-checkpoint sequence;
- the required ordered original function-entry subsequence.

Native state and ordering must match exactly. An original no$psx trace may contain
extra calls, rendering loops, and repeated PCs; the required semantic entrypoints
must occur in order. This is intentional because the native C representation is
not instruction-identical MIPS.

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

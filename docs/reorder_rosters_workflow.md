# Re-order Rosters implementation gates

Work one slice at a time using the [generated ledger](reorder_rosters_progress.md).
Do not use its core-instruction percentage as a feature-completion estimate.

## First slice: native swap core

The original dispatch maps menu return 7 to state `0x0C`, which calls
`0x80056AEC`. That wrapper normalizes the team with `0x80056A94` and runs
`0x80056494` with list kinds 1 and 2. Recomp agrees with headless Ghidra.

The implemented C core covers:

- Team 29 normalization: mode 2 uses a signed context-team byte; other modes
  use team 3. Other signed team inputs pass through unchanged. The database
  adapter separately rejects IDs outside the real-team range.
- The Re-order branch of `0x800556B0`: both IDs must exist and differ.
- The occupied same-team path of `0x800558E0`: swap two slots without shifting
  intermediate players, then increment the 16-bit session change counter.
- An in-memory `RosterDatabase::reorderSlots` adapter that refreshes the
  existing resolved-slot/membership data. No assets or saves are overwritten.

This is **not yet a selectable menu screen**. There is no fake screen route.
The UI still reports the child flow as unavailable.

The two shared validation/mutation functions deliberately retain **zero whole-
block accounting credit** for now: their other branches handle different team
lists, empty-slot compaction, injury checks and roster-count limits. Implementing
one useful path is not implementing every instruction. Review those blocks in
the next accounting pass; an exclusion needs a proven Re-order precondition,
not merely an assumption that an instruction is an emulator detail.

## Reproducible check

```powershell
pwsh -File scripts/verify_reorder_rosters.ps1 -RequireDatabase
```

The command builds the application and standalone C/C++ test executable, checks
the ledger, then runs 19 asset-free scenarios plus three optional local-asset
scenarios. CLI output includes scenario names, original function addresses and
per-team test counts. It writes only ignored logs/evidence under `.local/`.
No interactive app or emulator is controlled, and no save is modified.

Public CI builds the same core tests without SDL or original assets:

```sh
cmake -S . -B build-core -DNBA97_RECOVERED_TESTS_ONLY=ON
cmake --build build-core
python tools/verify_reorder_rosters.py --check --native-test build-core/nba97_reorder_tests
```

The generated JSON lists every pending block start and its instruction count.
`--check` validates accounting and report freshness only; it does not fabricate
test results. `--native-test` must execute successfully and emit exactly the
declared scenario IDs. The local evidence records source and executable hashes;
the wrapper rebuilds by default. `-SkipBuild` is diagnostic only: hashes alone
do not prove that an old executable was built from current sources.

## Promoting a slice

1. Recover the next owner's behavior from private recomp, with Ghidra fallback.
   Follow called helpers and callback pointers; add newly scoped owners rather
   than hiding them in the initial denominator.
2. Implement in C where appropriate, with explicit native resource ownership
   in the C++ adapter. Add small tests for state changes and no-change cases.
3. Map complete reviewed basic blocks to code and test IDs. Partial fragments
   stay uncredited unless an entire block is genuinely represented. Do not
   mark a caller complete while its meaningful callbacks are placeholders.
4. Run fresh tests; record remaining screen, audio and original-reference gaps.
   Change the slice status only after its declared exit gate passes.
5. Regenerate with `python tools/verify_reorder_rosters.py` and check the diff.
   To expand/refresh original counts, use
   `pwsh -File scripts/update_reorder_inventory.ps1` and document why scope changed.

## Next slice and final acceptance

The first/second-selection state machine is now tested. The selected source is
frozen while choosing its replacement. Both lists retain their cursor/scroll
positions after cancelling the second selection. Empty and same-player choices
do not mutate the roster. Successful swaps return focus to the first list.

Private recomp and Ghidra show that generic input `0x8003D930` handles cancel
(`0x100`) during the second selection by changing object state 1 to 2 and
synthesizing confirm (`0x800`). `0x800569BC` skips validation/swap in that state;
`0x80055314` restores first focus and sets the input latch to 10. Start/continue
(`0x80`) exits only at first-selection state zero. The native C controller
represents those transitions directly, not by emulating controller memory.

Exit callback `0x80056254` asks before discarding when the session change counter
is nonzero. Confirming restores its 535-ID entry snapshot; declining keeps the
screen open. In this single-team slice, a 15-slot isolated working copy contains
every possible edit. `RosterDatabase::applyReorderSession` publishes an accepted
order only when the original team baseline still matches and membership is
unchanged. It refreshes derived tables before publication. Cancellation never
publishes. No files are written. The future player/team screen integration must
read session data rather than the pre-edit database while this session is open.

These generic dispatcher/exit helpers are evidence dependencies outside the
initial 875-instruction inventory; they receive no implicit coverage credit.
Only three newly reviewed blocks receive credit: `0x80056948` (9 instructions,
selected-ID/empty check), `0x80056A24` (4, state-2 bypass) and `0x80056A78` (1,
validation failure latch clear). Begin/finish helpers also call rendering and
animation routines, so their whole blocks remain pending despite working state
transitions. This is why native-tested interaction does not mean 166/166 credited.

Try the actual controller through the isolated CLI harness:

```powershell
build-windows/Debug/nba97_reorder_tests.exe --reorder-cli .local/assetpacks/database/roster.n97db 0
```

Commands: `up`, `down`, `select`, `back`, `continue`, `yes`, `no`, `view`, `compare`. Team IDs are
0..28. `continue` accepts only from the first-player stage; `back` cancels a pick
or requests exit. Only explicit `yes` discards when prompted. EOF aborts without
publishing. This is a debugging harness, not a replacement for the game's screen,
and accepted changes live only inside its process. It logs every action and phase.

## 23-instruction first-callback increment

Six additional blocks in `0x800568E4` are now represented:

| Block start | Instructions | Purpose |
|---|---:|---|
| `0x800568E4` | 9 | Context/input load, native frame replacement, View mask check |
| `0x80056908` | 3 | Compare mask check |
| `0x8005691C` | 5 | Confirm mask check |
| `0x80056930` | 2 | Preserve latch for View |
| `0x80056938` | 2 | Preserve latch for Compare |
| `0x80056940` | 2 | Clear latch for other callback inputs |

`nba97_reorder_first_callback` operates **after** generic navigation/cancel
handling. Inputs are exact equality checks, not bit flags: `0x810` must not
confirm. Tests cover every 16-bit mask plus invalid-phase and empty-slot cases.
The CLI routes first-stage `select`, `view`, and `compare` through this function.
View/Compare deliberately log pending-child requests; their availability helper
and child screens are not implemented or credited here. Recomp control flow and
the existing Ghidra block inventory agree on these six blocks.

The diagnostic renderer reads `fonts/ZFONT0.PSH` and `database/roster.n97db`
directly from the private asset packs. It uses the existing PSH decoder, including
transposed glyph correction; there is no system-font fallback. It draws only
surname labels in two six-row columns at the recovered list origins. This is a
transparent label layer, **not an original-screen screenshot or finished row
composition**. Portraits, backgrounds, borders and highlight animations are
not represented by substitute art. `Z2PORT` portrait integration remains pending.

The local asset test verifies 156 decoded glyphs, 47 transposed glyphs, rendered
pixels, unchanged labels on pick/cancel, refreshed labels on swap, and restored
pixels on discard. Missing packs fail. It writes before/after PPM captures under
`.local/verification/reorder/`; the CLI writes `cli_labels.ppm` while interacting.
Local run evidence records font/database hashes as well as source/executable
hashes. No original glyph pixels, player data or captures are committed.

Next: original two-list screen/assets and menu entry, consuming this same C
controller. Keep row refresh, animations, View/Compare dispatch, sound, modal
presentation and input-repeat timing pending until they are actually connected.

Remaining slices cover original assets/layout, sound/flash/help/player round trips,
versioned local persistence/Reset, then same-scenario original screenshots and
traces. A feature can be accepted with documented platform replacements, but not
with unresolved user-visible paths. All original data, portraits, sounds, screenshots,
traces and decompiler output stay under `.local/`.

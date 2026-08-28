# Re-order Rosters implementation and verification

Use the [generated ledger](reorder_rosters_progress.md). Its percentage is
reviewed source accounting, **not feature completion or binary equivalence**.

The next goal and proposed persistence architecture are in the
[Re-order completion plan](reorder_completion_plan.md). That plan is not a claim
that child routes, disk saves/Reset or exact reference fidelity already work.

## Current boundary

The four selection/cancellation owners are now **166/166 source-accounted**:
first input (54), replacement input (54), begin replacement (24), finish/cancel
replacement (34). This finishes the previous 99 pending owner instructions.

Construction/entry adds **298/298 source-accounted** (wrapper22 + constructor276).
The initial ten-function inventory is **875/875 source-accounted; 0 pending**.
Shared refresh115, validation140 and mutation134 now have individual block
evidence, reusable C implementations and targeted tests. No denominator was
removed or dismissed as emulator overhead. This closes the ten-owner review,
not the feature, its transitive callees, or original-execution equivalence.

**The actual Re-order menu card now opens the native two-list screen.** It
uses original local ZSET4 artwork, Z2PORT portraits and PSH fonts, with a C
controller inside the C++ window loop. Captures are native output, not a claim
of original-versus-port screenshot parity. The screen-area subtotal is413/413,
including the separately reviewed shared refresh owner.

## Construction and entry

`src/recovered/reorder_screen.c` owns the535-slot entry snapshot, staged table,
30 typed rows, saved cursor/top restoration, both selected IDs and original
entry/exit callback contracts. The native host supplies the frame pump and
local asset loading. Team scanning preserves both scroll/cursor positions;
it is disabled during replacement selection. Windows Accept durably saves then
publishes all edited teams; discard restores the entire snapshot. The100 free-agent
slots must remain unchanged. Original database assets are never overwritten.

Important distinction recovered from the outer dispatcher:

| Item | Original source | Native representation |
|---|---|---|
| Graphics | frontend state0x0C, layout80096BC4 | ZSET4 `ba22` at156,10; NOT Trade's `ba38` |
| Controller | `80056494 -> 8003D930`, layout0x0D | restored two-list input state |
| Portraits | `80030D14`, Z2PORT.IDX/BIG | 493 local87x51 images,88-byte padded rows; record0 fallback, playerN recordN+1 |
| Header | objects18/19 at54/386,22 | small portraits behind frml/frmr at30/368,15 |
| Rows | type0x33,30 objects,6 visible | x60/270; first baseline112, step16 |
| Modal | `80056254 -> 80040A1C`, descriptor800AF4F8 | local `discard.n97ui`, four body lines/two choices |

Keyboard: arrows navigate rows/teams; C or Space picks; X or Esc cancels;
Enter accepts from the first-selection phase. The confirmation uses Up/Down
and C/Enter. F/H/F1 opens the original Help for the active stage; D opens View
from the isolated draft. In that child, arrows browse players/stats, J/K scan
teams, Q/E change stat layer, C/Space plays a fact, D/S stops it, and Enter/X/Esc
returns to the unchanged parent selection. S opens Compare from the draft;
its C/Space changes the active side, J/K scan teams and Q/E change stat layer.
See [child verification](reorder_child_verification.md) for evidence and limits.
The existing desktop shortcut launches the rebuilt executable.

## Recovered behavior and native representation

| Original owner/helper | Native behavior and evidence |
|---|---|
| `800568E4`, `800569BC` | Exact input masks 0x10/0x40/0x800; other masks clear the latch. Normal and synthesized-cancel paths are tested exhaustively. |
| `80054B94` | Checks active View ID or both Compare IDs. Empty entries show a message. Success sets result 2/3 for the **outer dispatcher**; this helper does not execute a child screen. Same-ID Compare is allowed. Handoff uses the edited working IDs. |
| `800552B4` | Sets descriptor page, active page and object state to 1; pulses the selected second row; refreshes headers. |
| `80055314` | Restores those fields to 0; resets the second-row color; refreshes headers. State 2 bypasses validation/swap/rebind and sets a held-input barrier plus latch 10. |
| `8002AB88 -> 8002AB30 -> 8002AE5C` | Whole-row RGB modulation: neutral 128/128/128, gold 120/102/0, duration 20. Initial gold hold, then reversal at duration+1 with signed integer interpolation. |
| `8002AC90 -> 8002AC2C` | Eight-update reset to neutral. Both recomp and Ghidra confirm the odd duplicate blue write and preserved start-red channel; regression preserves it. |
| `8003B194` | Original pumps frames until the held input differs. Native frame API is nonblocking: continue rendering, but do not dispatch while it returns 0. |
| `800556B0` | Shared injury precedence (mode + injuries + state13 + second ID), team29/full15 result-1, exact minimum8, kind2 occupied/different and one-empty modal. Both-empty is silent. Typed notices retain original message address and affected player/team. |
| `800558E0` | Independent backwards empty-slot searches, two writes, symmetric counts before donor compaction, halfword counter. Does not validate: an identical occupied pair still increments if directly called. Re-order adapter validates first. |
| `800555F4 -> 8005539C` | Donor compaction15/free100; starter repair chooses ranked natural position then earliest bench match using external preference data. Preserves the original post-decrement count bound and no-match0 return. Tested callee contract, no additional instruction credit. |
| `80055AF8(0/1/2)` | Bind all requested rows, redraw visible only, sign-extend selected IDs; present-one-frame request, restore saved descriptor page, then refresh header. Cursors/tops untouched. |
| `80055068 -> 80054ECC / 80054DB4` | Redraw team object 0x73 (city + nickname at 256,70), natural-position line at x60/270,y95. Bench marker at 8002654C is an underscore, not an empty string. |
| `80040A1C` | Original private empty-slot/child-unavailable descriptors, message input ownership, acknowledgement and original-font feedback. Diagnostic draws warning rectangle growth/shrink; full shared modal/audio integration remains pending. |

The native C ABI represents original stack/register setup and return. Those
instructions count as reviewed platform replacements, not new visible gameplay.

### Shared-helper boundaries and discoveries

`src/recovered/roster_lists.c` owns all three helpers; the Re-order adapter uses
them instead of maintaining duplicate swap/refresh logic. The63 original blocks
have individual behavior/test mappings in `config/decomp/reorder_rosters.json`.
These mappings are review evidence, **not programmatic MIPS equivalence proof**.

Validation notices substitute typed player/team identities for original
pointer-based name formatting. Existing Re-order empty messages use the private
pack. Injury/minimum dialogs belong to future Trade/Sign/Release screens and are
not falsely presented as integrated here. Likewise, the native `PRESENT` callback
requests the retained host surface; it does not block for original vblank or
guarantee the same intermediate framebuffer. Exact timing remains pending.

Starter repair uses the private25-byte preference table at800265AC, not guessed
ratings. A local regression reads that table and roster positions, performs145
isolated starter transfers across29 teams, and checks exact slots/counts against
an independent ranking oracle. No original table bytes are published. Synthetic
cases also preserve the unusual shortened bench bound after count decrement and
the no-match return0. Those quirks need original runtime comparison before any
intentional behavioral change; future cross-mode UI must provide valid counts,
positions, injury flags and private preference data to the compaction service.

## Asset-backed feedback

`ReorderLabelPreview::renderFeedback` consumes local `ZFONT0.PSH`,
`ZFONT1.PSH`, the roster database, and the small `reorder/dialogs.n97ui` pack.
Missing data fails; there is no substitute font or portrait/background artwork.

The dialog pack is 155 bytes, extracted from FEONLY addresses 800AFFFA and
800AFC22. It retains the original geometry, line records and text. Extraction
is restricted to this repository's ignored `.local/` directory:

```powershell
python tools/extract_reorder_dialogs.py .local/extracted/FEONLY.BIN
```

The warning box grows from (246,110,20,10), by (-9,-4,+18,+8) per update,
clamped to its descriptor. Closing reverses these increments. Warning colors
come from 80030430: dark-red opposite corners and brighter-red opposite corners.
This is a native Gouraud approximation, **not verified PSX raster equivalence**.
Message open/close sound IDs5/8 now use the existing native cursor bank.
The game-window close is currently immediate; the diagnostic supports shrink
frames. Exact original audio/animation timing is still a separate feedback gate.

Rows now compose position, jersey and surname at recovered offsets.
Starters use slot positions in uppercase; bench rows use natural positions.
The first baseline is **112**, because 80055ED4 increments 96 before returning.
The old surname-only capture remains available as a deliberately separate test.

## Fresh verification

```powershell
pwsh -File scripts/verify_reorder_rosters.ps1 -RequireDatabase
```

The wrapper builds, runs ledger/extractor unit tests, checks report freshness,
then executes **57 asset-free + 7 local-asset scenarios**. If needed it extracts
the private message pack from the already-local FEONLY. It never edits original
asset files or persistent saves.

Public CI needs no original data or SDL:

```sh
cmake -S . -B build-core -DNBA97_RECOVERED_TESTS_ONLY=ON
cmake --build build-core
python tools/verify_reorder_rosters.py --check --native-test build-core/nba97_reorder_tests
```

Focused evidence includes:

- All 65,536 first-callback masks; 131,072 normal/cancel second-callback masks.
- All 256 object-state bytes; only state 2 bypasses validation/mutation.
- 180 source/replacement cases checking row rebind, refresh order and focus.
- 900 View/Compare availability cases, including same-ID comparison.
- Exact highlight keyframes and signed interpolation; held-cancel barrier.
- Modal input isolation, original descriptor bounds, expansion/shrink captures.
- Real glyph pixels returning to neutral after cancellation; both columns
  changing after a swap.
- 6,525 local database pairs across 29 teams, plus isolated publication,
  stale-baseline/membership rejection and discard checks.
- 768 shared-validation combinations; exact rule precedence, result and notice.
- Both transfer directions, interior/trailing empty searches, free-agent100
  compaction, starter repair/injury exclusions and counter overflow.
- Refresh selectors0/1/2, zero/negative display counts, six-row clipping,
  signed selected IDs, descriptor restoration and present/header event order.
- 145 private-data starter transfers; original preference data stays local.

Local captures: `.local/verification/reorder/feedback_*.ppm`,
`modal_*.ppm`, and legacy `labels_*.ppm`. They have been visually inspected as
diagnostic layers, not compared against an original Re-order screen.

Full-screen captures (entry, replacement scroll, swap, confirmation, both Help
pages, and View/Help/browse/return before and after a swap):

```powershell
python tools/verify_reorder_screen.py
```

This runs the actual compositor, checks dimensions/distinct states, hashes
private inputs/output and checks that database data is unchanged. It does
not assign a visual similarity percentage. Inspect the PNGs under
`.local/verification/reorder/screen/`; original-reference comparison is pending.

`.local/reports/reorder_rosters_run.json` records fresh pass IDs, executable
and source hashes, plus font/database/dialog pack hashes. A failed/aborted run
cannot leave old success evidence current. `--check` alone checks metadata;
it never pretends to run behavioral tests. `-SkipBuild` does not prove that an
old executable corresponds to current sources.

## CLI use

```powershell
build-windows/Debug/nba97_reorder_tests.exe --reorder-cli .local/assetpacks/database/roster.n97db 0
```

Commands: `up down select back continue yes no view compare tick ok`.
`ok` acknowledges a message. `tick` advances feedback. CLI commands are discrete
presses with release between them, not physical controller repeat simulation.

Logging includes phase, both pages, selected slots, row/header revisions,
modal state, latch, child-result codes and edited player IDs. The CLI inspects
child data and clears the result; it does **not** pretend to run a child screen.
Feedback captures update under `.local/verification/reorder/`.

Only `continue` from first-selection phase accepts the in-memory working order.
Cancelling a replacement preserves earlier swaps; cancelling a dirty first
stage asks before discard. The adapter checks baseline and membership before
publication and refreshes derived tables. EOF discards without publication.
No disk save is written.

## Accounting gates and next work

1. Recover behavior from private recomp, then use Ghidra to verify uncertain
   branches, data and callees. Keep outputs private.
2. Implement a native contract and tests before crediting a whole basic block.
   Original state restrictions must be explicit, never called emulator overhead.
3. Every fully accounted owner must have either a fully accounted callee
   or a tested, source-linked `selection_contracts`/`screen_contracts`/`shared_contracts`
   entry for every direct call. Shared owner credit also requires a behavior and
   owner-declared tests for every credited block.
   The verifier rejects missing contracts and missing integration-boundary notes.
4. A contract does **not** award full callee credit. These contracts are
   intentionally scoped; generic modal timing/audio, linked-object GPU behavior
   and outer frontend dispatch are not silently counted as done.
5. Expand the whole-function inventory only with a documented scope revision.
   Regenerate with `python tools/verify_reorder_rosters.py`; fresh native tests
   and local captures are required before promoting runtime integration.

Next: verify original held-key repeat and modal timing, and compare matching original
screenshots/traces. Current presentation still uses the existing title-jumble
approximation and a native180ms crossfade; the fixed plate vertices are recovered
but native triangle sampling is not asserted to match PSX rasterization exactly;
these are known fidelity limits, not source instruction matches. These gates
remain pending; 875/875 reviewed owner instructions does not close them.

## Windows save/Reset regression gate

`python tools/verify_reorder_save_host.py` exercises startup/Accept, save failure
and retry/discard, backup recovery, unsupported saves, and normal-roster Reset.
It also runs edit -> View/Help -> Compare/Help -> Accept -> fresh load -> Reset ->
fresh load on one isolated roster set. Captures and the JSON report remain under
`.local`. Save, profile, settings and private source hashes are checked unchanged
outside the unique fixture directory.

The private Reset confirmation comes from800AEDD2, not hand-authored text.
Up/Down choose, C/Space/Enter confirm; Cancel is initially selected. Reset is
enabled by differences from original defaults, not by save-file existence.
A successful Reset persists an empty override and disables the Reset card.
The red warning modal, original font and source-linked sound IDs are tested;
matched original frame/timing/audio evidence is still a separate open gate.

See [completion plan](reorder_completion_plan.md) and
[save format](roster_save_format.md) for transaction boundaries, compatibility,
created-player limits and the distinction between source accounting and fidelity.

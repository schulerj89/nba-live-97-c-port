# NBA Live 97 PS1 decompilation

An experimental, native C/C++ reconstruction of the US PlayStation release of
NBA Live 97 (`SLUS-00267`). The project uses static recompilation evidence,
headless Ghidra analysis, and runtime comparison with the original game.

[![NBA Live 97 decompilation progress](docs/progress.svg)](docs/progress.md)

The gold percentage measures original functions with explicit recovery
evidence, not completed functions. The green panel lists manually catalogued
native-port milestones by status, not an overall game-completion percentage.
Items vary greatly in size and the catalogue is incomplete; partial work earns
no arbitrary half-credit. Playable basketball remains unimplemented.

The tracked View Rosters scope currently has semantic ownership for **997/997
original instructions** across **83/83 basic blocks**, with **12/12 native
scenarios**, **18/18 interaction contracts**, and **91.82% weighted behavioral
and screenshot fidelity**. Structural MIPS control-flow matching is not targeted for the
native port and is not presented as a completion percentage.

## Current scope

The native Windows build currently covers:

- the loading, legal, intro-movie, title, and Game Setup sequence;
- Rules and Options, including persistent settings;
- local user-profile creation, editing, deletion, and versioned saves;
- the Rosters frontend, team browsing, player statistics, portraits, and Cool
  Fact playback;
- Re-order screen entry, two-list selection and in-memory accept/discard
  ([scoped instruction ledger](docs/reorder_rosters_progress.md));
- original frontend music and recovered menu sounds; and
- a CLI trace describing recovered states, assets, audio, and transitions.

This is not yet a complete game. Gameplay, roster transactions, and several
deeper frontend paths remain unfinished. See the generated
[progress report](docs/progress.md) for the measured breakdown.

Recovered game behavior is moving into portable C modules under
`src/recovered/`. The existing C++ code remains the native Win32 platform shell
for rendering, input, movies, audio devices, and resource ownership. A recovered
C function replaces its C++ approximation rather than creating a second copy.

## Assets and legal notice

**No game assets are included.** You must provide your own lawfully obtained
copy. The disc image, executables, overlays, decoded images, audio, fonts,
roster data, emulator dumps, and generated recompilation output must remain
under the ignored `.local/` directory.

The [MIT License](LICENSE) covers only project-authored source and
documentation. It grants no rights to NBA Live 97 or third-party intellectual
property. See [ASSET_NOTICE.md](ASSET_NOTICE.md) for the complete notice.

## Build and run on Windows

Requirements:

- Windows with Visual Studio 2022 and its C++/CMake tools;
- Python 3; and
- a matching `SLUS-00267` BIN image at
  `.local/input/nba-live-97-slus-00267.bin`.

From PowerShell:

```powershell
pwsh -File scripts/extract_assetpacks.ps1
pwsh -File scripts/prepare_intro_movie.ps1
pwsh -File scripts/build.ps1
pwsh -File scripts/run.ps1
```

Asset extraction and movie preparation write only beneath `.local/`. The
executable is created at `build-windows/Debug/nba97_boot_decomp.exe`, and its
runtime trace is mirrored to `.local/logs/boot_decomp_trace.log`.

`scripts/build_wsl.ps1` provides the SDL2 compatibility build used for WSL and
Linux testing; the main reconstruction targets native Win32.

## Keyboard controls

| Context | Controls |
|---|---|
| Intro/title | `Space` skips the movie; `Space` or `Enter` starts |
| Menus | Arrow keys move/change values; `Enter` or `Space` selects |
| Back | `Escape` or `Backspace` |
| View Rosters | `Left/Right` changes team; `Up/Down` changes player; `Q/E` changes category; `Z/C` changes field |
| View Player | `Left/Right` changes player; `J/K` changes team; `Q/E` changes stat layer; `Up/Down` scrolls |
| Re-order | Arrows browse rows/teams; `C`/`Space` picks; `X`/`Escape` cancels; `Enter` accepts from first selection |
| Cool Facts | `Enter` plays; `S` stops |
| Help | `H` or `F1` |

Mouse hover and selection are supported in the reconstructed frontend screens.

## Progress and reverse-engineering workflow

Recovery metadata is committed without original code or assets:

- `config/decomp/functions/` contains Ghidra-generated address and size
  inventories;
- `config/decomp/recovered_functions.json` records evidence-backed function
  research;
- `config/decomp/features.json` records manually assigned native milestone statuses; and
- `reports/progress.json` and `docs/progress.*` are generated views.

Refresh Ghidra inventories and progress reports with:

```powershell
pwsh -File scripts/analyze_headless.ps1
pwsh -File scripts/update_progress.ps1
python tools/report_progress.py --check
```

GitHub Actions rejects stale generated reports and any tracked `.local/` file.
Function coverage is intentionally conservative: partial behavioral evidence
does not count as a complete or instruction-matching decompilation.

View Rosters additionally tracks the original MIPS denominator—functions,
instructions, basic blocks, and call sites—from headless Ghidra. Source
ownership, explicit block accounting, native semantic checkpoints, and
original-trace equivalence are reported separately so a working native feature
is never mislabeled as byte-identical original code. The detailed report keeps
the original CFG inventory as research metadata, but structural CFG equivalence
is **not targeted** for this native C/C++ port; its raw edge count should not be
interpreted as a progress or quality score.

Refresh that structural inventory with
`pwsh -File scripts/update_instruction_semantics.ps1`. Once a same-scenario
original PC trace is available locally, compare its function-entry path with
the native checkpoint sequence using
`python tools/compare_semantic_traces.py --original <local-trace.txt>`.
For state-aware scenario comparison, follow the
[View Rosters semantic verification workflow](docs/view_rosters_verification_workflow.md).
That contract currently runs 12 small native scenarios over 18 inventoried
state interactions, while original no$psx trace equivalence remains a separate
2-scenario evidence tier. Help modals, scrolling, stat layers, and Cool Fact
decoding also have non-scoring pass/fail regressions so they cannot inflate the
visual-fidelity percentage.

View Rosters also has a separate end-to-end fidelity score generated from
recovered behavior checks and local screenshot comparisons. Run
`python tools/verify_view_rosters.py --behavior-pass --require-references` after
deterministic capture to refresh it. The score is useful regression evidence,
not a byte-match claim: emulator scaling, color presentation, and animation
phase can prevent identical pixels even when the recovered layout agrees.

Re-order Rosters now opens the original-asset two-list screen, with selection,
team scanning, shared validation/swap/refresh helpers and in-memory accept/discard.
Its [instruction ledger](docs/reorder_rosters_progress.md) accounts for **875/875**
instructions in the initial ten owners—not 100% of the feature. Help/child routes,
disk persistence and original-reference acceptance remain pending.
Run `pwsh -File scripts/verify_reorder_rosters.ps1` for
fresh CLI tests; see the [workflow](docs/reorder_rosters_workflow.md).

For the parent Rosters card screen:

```powershell
pwsh -File scripts/verify_rosters_menu.ps1 -RequireReference
```

It validates the recovered two-tier draw order, Reset/Injuries availability,
12-vblank selection flash, and all six local ZCURSOR waveform decodes including
the recovered BNKl program-table lookup and PATl/Tone pitch. Run
`python tools/compare_cursor_audio.py` to create a
local raw-versus-authored waveform/spectrum audit. The original screenshot,
exported WAVs, and plots remain ignored under `.local/`; only the asset-free
measurement report is committed.

Run the complete local verification pipeline with:

```powershell
pwsh -File scripts/verify.ps1
```

It checks progress metadata, validates C recovery ownership, builds the mixed
C/C++ application, and runs the asset-backed behavioral self-test. Optional raw
PS1 function candidates belong under `.local/build/ps1/functions/`; the verifier
compares eligible candidates against the owned original bytes. A normal
verification pass never implies byte matching—use `-RequireMatching` when exact
PS1 matches have been configured.

## Local-only layout

```text
.local/
  input/       Original disc image
  extracted/   Executables and disc files
  assetpacks/  Decoded runtime assets and databases
  recomp/      Generated static-recompilation material
  dumps/       Emulator and debugger captures
  saves/       Profiles and settings
  logs/        Runtime and tool traces
  tools/       Locally supplied third-party utilities
```

Do not commit or redistribute anything from this directory.

# NBA Live 97 PS1 decompilation

An experimental, native C++ reconstruction of the US PlayStation release of
NBA Live 97 (`SLUS-00267`). The project uses static recompilation evidence,
headless Ghidra analysis, and runtime comparison with the original game.

[![NBA Live 97 decompilation progress](docs/progress.svg)](docs/progress.md)

The gold percentage measures original functions with explicit recovery
evidence. The green percentage measures completion of the catalogued native-port
roadmap. They are deliberately separate; a working feature is not automatically
a fully decompiled or matching original function.

## Current scope

The native Windows build currently covers:

- the loading, legal, intro-movie, title, and Game Setup sequence;
- Rules and Options, including persistent settings;
- local user-profile creation, editing, deletion, and versioned saves;
- the Rosters frontend, team browsing, player statistics, portraits, and Cool
  Fact playback;
- original frontend music and recovered menu sounds; and
- a CLI trace describing recovered states, assets, audio, and transitions.

This is not yet a complete game. Gameplay, roster transactions, and several
deeper frontend paths remain unfinished. See the generated
[progress report](docs/progress.md) for the measured breakdown.

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
| Cool Facts | `Enter` plays; `S` stops |
| Help | `H` or `F1` |

Mouse hover and selection are supported in the reconstructed frontend screens.

## Progress and reverse-engineering workflow

Recovery metadata is committed without original code or assets:

- `config/decomp/functions/` contains Ghidra-generated address and size
  inventories;
- `config/decomp/recovered_functions.json` records evidence-backed function
  research;
- `config/decomp/features.json` tracks native feature milestones; and
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

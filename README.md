# NBA Live 97 PS1 decompilation

Experimental decompilation and recompilation research for the US PlayStation release (`SLUS-00267`).

[![NBA Live 97 decompilation progress](docs/progress.svg)](docs/progress.md)

The two percentages measure different things: original-function evidence is
conservative reverse-engineering coverage, while native-port roadmap progress
tracks playable reconstructed features. Click the status card for the full
methodology and subsystem breakdown.

## License and asset policy

Project-authored source code and documentation are available under the
[MIT License](LICENSE). That license applies only to this repository's original
work; it does not grant rights to NBA Live 97 or any third-party intellectual
property.

**No game assets are included.** This repository does not distribute the game,
disc images, executables, overlays, video, audio, fonts, textures, models,
logos, player likenesses, extracted data, emulator dumps, or generated
static-recompilation output. Users must provide their own lawfully obtained
copy, and all private or game-derived material must stay under ignored
`.local/` paths. See [ASSET_NOTICE.md](ASSET_NOTICE.md) for the complete scope
and trademark notice.

The repository does not contain the game, executable, disc contents, or extracted assets. Contributors must supply their own copy. All private game material and directly generated recompilation output belongs under `.local/`, which is excluded from Git.

## Local-only layout

```text
.local/
  input/       Original disc image or local path configuration
  extracted/   Extracted executable and disc filesystem
  assets/      Audio, video, textures, models, fonts, and other game data
  recomp/      Machine-generated static-recompilation source and configuration
  dumps/       Emulator traces, RAM/VRAM dumps, and debugger captures
  logs/        Tool output and diagnostics
```

Only project-authored source, scripts, configuration templates, documentation, and cryptographic hashes should be committed.

## Measured progress

Decompilation coverage and native-port coverage are tracked separately. The
committed Ghidra inventories currently scope the PS-X boot executable and the
`FEONLY` overlay. `config/decomp/recovered_functions.json` records only
functions with explicit recompilation, Ghidra, source, or runtime evidence;
partial recovery never counts as a completed function. The native feature
roadmap lives in `config/decomp/features.json` and includes unfinished roster
transactions and gameplay so a polished frontend cannot be mistaken for a
complete game.

The current generated baseline is in [docs/progress.md](docs/progress.md), with
machine-readable output in `reports/progress.json`, a standalone dashboard in
`docs/progress.html`, and the README status card in `docs/progress.svg`.
Regenerate and validate them with:

```powershell
pwsh -File scripts/update_progress.ps1
python tools/report_progress.py --check
```

CMake also exposes `progress` and `progress-check` targets when Python is
available. CI rejects stale reports and any attempt to track `.local` files.
Function inventories contain addresses, generated names, and sizes only—not
original code or assets.

## Boot decompilation slice

The repository root builds a native C++ reconstruction of the first recovered
boot path. Its main application, Win32 event loop, boot state machine, asset
ownership, and renderer are C++ code. It decodes the game's original
`SHPP/GIMX` PSH images at runtime and
shows `ZLOADSCR.PSH`, loads the separate `ZLOADING.PSH` strip, displays
`ZLEGAL.PSH`, reaches the `ZCPYRT97.PSH` title/press-start state, and enters an
interactive Game Setup frontend slice. It
decodes the title's lowercase `press start` prompt from the original
`ZFONT0.PSH` 4-bit glyph archive using recovered coordinates and spacing. It
does not use screen captures or host fonts. The Game Setup screen decodes its
original `ZSET1.PSP` sprite package into the ignored local asset pack and uses
the recovered background tiles, border, NBA Live 97/EA logos, heading, help
strip, Level plate, and bottom labels. It extracts the original 95-record
`ZCARD.BIN` pack locally, fixes its PAL8 scanline stride, and composites
authentic 69x63 player portraits into the four recovered `blk1` slots. These
assets are not copied from the supplied reference image. The slice uses the
recovered `quarter / mode / style / level` and
`rules / options / rosters / users / card` rows, animates the heading, and accepts
keyboard and mouse hover navigation. Rules and Options now open as recovered
frontend states 1 and 2. Both use the original `ZSET1.PSP` headings, borders,
logos and `ZFONT0.PSH` menu font; their 6/7-row scrolling lists support arrow
or mouse navigation and left/right changes. Rules preserves the game's
Arcade/Simulation/Custom preset behavior, including its hidden Custom snapshot.
Changes are saved immediately to ignored local file
`.local/config/frontend_settings.ini` and restored on the next launch. Other
bottom-menu roots are now recovered too: Rosters opens frontend state 9 with
all eight original `ZSET4.PSP` normal/highlight cards, and Card opens state 11
with the three original `ZSET1.PSP` memory-card choices. Arrow keys and mouse
hover change their highlighted cards; their deeper child flows remain outside
this slice except View Rosters. View Rosters follows the recovered card-4
return code into frontend state `0x10` (`FUN_800592C4`), preserves its team and
player selections, and uses the original `ZSET4.PSP` Team Rosters heading and
all 29 team-art sprites. Its recovered generic-list geometry shows exactly six
players at x=60 with a 12-pixel row pitch; the selected row uses the original
neutral-to-gold pulse and the list performs the recovered one-tick, seven-row
handoff only when focus crosses the six-row window. Left/right wraps through
teams without resetting the player slot, up/down browses the 15-slot-capable
roster, and mouse hover selects one of the six visible rows. Q/E reproduce the
original Select/L2 category controls, while Z/C reproduce R2/L1 field controls.
The six categories expose player attributes, ratings, 1995/96 season and
playoff totals, and extensible current-season/current-playoff slots. Enter and
Space, or clicking a visible row, pushes the recovered nested View Player state
`0x24`. That state streams the selected player's original 180x156 action photo
from the locally extracted `Z1PORT.IDX/BIG`, shows identity and season details,
checks the player's five `Z1COOL.IDX/BIG` fact slots, and returns with Escape or
Backspace without losing the team, selected player, or scroll position. The
View Player descriptor layers are recovered separately from the team-list
columns: Q/E cycles player attributes, player ratings, 1995/96 season,
1995/96 playoffs, current season, and current playoffs; Up/Down scrolls the
layer with the original 20-tick gold flash and `ZCURSOR.VH/VB` sound IDs 3/4;
Left/Right wraps players; and J/K (or the bracket keys) scans teams. Enter plays
a non-repeating available Cool Fact and S stops it. Each speech record is
streamed by player ID from the ignored 122 MB `Z1COOL.BIG`, decoded in-process
from its `PATl/TMxl` mono PlayStation ADPCM payload, and submitted through
WinMM. H/F1 opens the original green state-`0x24` controller-help popup over
the player screen with the exact `ZFONT1.PSH` button glyphs. Keyboard mappings
remain documented here instead of crowding the recovered in-game modal.
No cursor or Cool Fact audio is converted, bundled, or published. The roster's
team and scroll arrows are the original `ZFONT0.PSH` control glyphs
`0x8a` through `0x8d`; H, F1, or the original Help strip opens state-aware Help.
Escape from the roster cancels and restores the entry snapshot.

The View Rosters background is also reconstructed from the original private
indexed art instead of approximated. Recompilation `FUN_8002FDA4` loads the 33
`ZTMPAL.PSH` entries and `FUN_8002FE58` replaces colours 0 through 159 of each
`ZSET4.PSP` `Bkga`-`Bkgd` strip with the selected team's palette while retaining
the strip-local colours 160 through 255. Layout `0x1c` places the four full
`128x240` strips at x=0, 128, 256, and 384 with no crop or model transform.
`FUN_8002FF80` crossfades the team-controlled colours over 17 frontend ticks.
The extraction tool reproduces that mixed palette only under ignored `.local/`;
no decoded team background or source palette is tracked by Git.

The original first-boot Users rule is preserved. `0x8005CD88` scans twenty
0x6c-byte profile records; byte +0x5d is the first byte of the trailing user
name, so a non-empty name makes a record active.
`0x8003F43C` passes that count and layout object `0x2b` to `0x800399C4`, which
applies disabled flags `0x06` when the count is zero. The generic navigator
skips objects whose flags match `0x86`. The native menu therefore dims Users
and moves directly from Rosters to Card on a zero-profile first boot.

Accepting any top Game Setup card enters the recovered user-assignment/profile
stage from `FUN_80037010`. Its in-window editor uses the original font and
enforces the recovered limit of twenty unique profiles and thirteen visible
characters per name. Existing profiles can be renamed; Delete requires a
second press and then removes the full logical record, matching
`FUN_80036D48`'s zero-on-delete behavior. Profile changes immediately update
the Users enabled state and the CLI trace.

Profiles persist outside emulated memory cards in the ignored local file
`.local/saves/user_profiles.n97sav`. This is a compact, little-endian,
sectioned container with stable 64-bit profile IDs, explicit major/minor
versioning, length-aware records, a separate extensible statistics section,
generation counter, whole-file CRC32, atomic replacement, and `.bak` recovery.
Unknown future sections can be skipped, while a new incompatible major version
is rejected instead of being misread. No profile names or save data are
compiled into the executable or committed to the repository. Use `--profiles`
to select another local save path.

### Private roster database

Team and player data is not compiled into the executable. The asset extraction
script reads the owned copy of `FEONLY.BIN` and creates the ignored, versioned
`.local/assetpacks/database/roster.n97db` pack. The public C++ loader validates
its magic, version, byte order, section directory, string bounds, unique IDs,
team/player references, and one-team-per-player ownership before publishing its
database snapshot. There is intentionally no sample-name or built-in fallback
roster.

Static recompilation and headless Ghidra agree that `FUN_8005FE14` treats IDs
below `0x1ed` as the 493 original players. `FUN_80057864` copies the original
29-by-15 signed roster-slot table from `DAT_800C0CAC`, then `FUN_8005770C`
resolves every team. The recovered source table produces 362 assigned players
and 131 original free agents; team sizes range from 9 to 15 rather than being
invented as a uniform count. The pack retains stable IDs, names, biography strings, physical attributes, the 17
source ratings, team identity strings, roster membership, resolved school and
acquisition fields, and the original 1995/96 regular-season and playoff stat
lines. Current-season and current-playoff records remain explicit empty slots
that later gameplay can update without changing the viewer. The sectioned
container is versioned so future fields can be added without being mistaken for
older records.

### Native frontend music

The frontend decodes the original private `ZTMENU1.CNK` directly in-process.
Its fixed EA `SCHl/PATl/TMxl` header identifies 44.1 kHz stereo PlayStation ADPCM
(`codec 0x06`) with 7,421,609 samples. The C++ decoder validates every chunk,
decodes all 2,524 `SCDl` blocks to PCM, and plays/loops them through WinMM; no
external player, FFmpeg process, emulator, or published converted audio is used.
The Options music-volume value is applied live through the recovered
`value * 15`, clamped-to-127 rule from `FUN_8002F258`. Decoder format, block
count, playback state, and volume changes are written to the existing CLI trace.

The call chain is grounded in both the generated recompilation and independent
headless Ghidra analysis:

- boot entry `0x801E3508`
- boot loader `0x801E1A68` loads `ZLOADSCR.PSH` and `ZLOADING.PSH`
- front-end overlay entry `0x8007B79C`
- front-end caller `0x80035984` invokes legal routine `0x80036684` before title
- legal routine `0x80036684` loads and displays `ZLEGAL.PSH`
- title routine `0x8002EEF4` runs next
- recovered routine `0x8002D768` loads `ZCPYRT97.PSH` through `0x80028BAC`
- title input at `0x800804E8` releases the loop when `DAT_800FEE3C` is set
- `0x8002FDA4` loads the frontend palettes after Start
- `0x8003F7C8` selects Game Setup state zero and `0x8003F43C` constructs it
- `0x80030CDC` selects `ZSET1.PSP`; `0x80031A88(0)` loads its first screen
- `0x8003F7C8` dispatches Rules state 1 through control `0x80098194`
- `0x8003F7C8` dispatches Options state 2 through control `0x80098258`
- `0x8003F7C8` dispatches Rosters state 9 to `0x80057CE4`, Users state 19 to
  `0x8005CF78`, and Card state 11 to `0x80053F4C`
- `0x8005CD88`, `0x8003F43C`, and `0x800399C4` implement the active-profile
  count and first-boot Users disabled flag
- `0x80036D48` clears one 0x6c-byte user record; `0x80037010` implements
  Start New, duplicate-name checks, the twenty-user ceiling, editing, and delete
- `0x8003E698` applies Arcade, `0x8003E714` applies Simulation, and
  `0x8003E620` restores the saved Custom-rule snapshot
- `0x80035D88` supplies the recovered first-boot Options defaults
- `0x8002F258` selects `ZTMENU1.CNK` and converts the frontend music value to
  the engine's 0-127 volume
- `0x8005770C` resolves 15 roster IDs per team into `0x68`-byte team records
- `0x8005FE14` maps original player IDs below `0x1ed` through the player index
- `0x800592C4` restores the View Rosters team/list position and
  `0x800590B8` constructs its recovered state-`0x10` browser
- View input `0x10` returns 2 and pushes state `0x24`; `0x8005A538` loads
  `Z1PORT.IDX/BIG` and `Z1COOL.IDX/BIG` before returning to state `0x10`

Extract the private boot, frontend, font, and menu asset packs and build:

```powershell
pwsh -File scripts/extract_assetpacks.ps1
pwsh -File scripts/prepare_intro_movie.ps1
pwsh -File scripts/build.ps1
pwsh -File scripts/run.ps1
```

`extract_assetpacks.ps1` decodes `ZSET1.PSP`, `ZSET4.PSP`, and `ZSET7.PSP`
locally, extracts `Z1PORT`/`Z1COOL`, and decodes the View Player portraits.
Their generated PNGs and source packs remain under
`.local/assetpacks/menu/` and are excluded from Git.

On User Setup, Up/Down selects an existing profile or `Start New`, Enter begins
editing, typing uses the recovered-compatible uppercase character set,
Backspace edits, Enter saves, Escape cancels/returns, and Delete twice removes
the selected profile.

The default build is a native Win32 executable so the desktop shortcut does
not depend on WSLg window placement. `scripts/build_wsl.ps1` retains the SDL2
compatibility build for Linux/WSL testing.

`prepare_intro_movie.ps1` extracts the raw `Z0ZTITLE.XA` disc sectors and uses
jPSXdec locally to produce a synchronized playback copy. The native C++ app
builds an in-process DirectShow graph for video and renders it into the existing
Win32 window. Because the system DirectSound renderer can build successfully
while remaining silent, the app also parses the AVI's interleaved PCM chunks
itself and submits the original 37.8 kHz stereo stream through WinMM alongside
the video. No external movie-player or runtime conversion process is launched.
The CLI trace lists the selected graph filters and native PCM sample count.
The recovered sequence
is load screen, NBA legal screen, the 77.9-second EA/NBA intro movie, then the
title screen. Press Space during the intro to skip it. On the title screen,
press Space or Enter to open Game Setup. Use arrows to move within/between its
two rows, or hover with the mouse. Select a bottom menu with Enter, Space, or a
mouse click. Rosters and Card use the same navigation keys; Backspace or Escape
returns from them. In View Rosters, Enter/Space or clicking a player opens View
Player, Escape returns to the exact roster position, and H/F1 or the Help strip
opens Help. View Player uses Q/E for stat layers, Up/Down for stats,
Left/Right for players, J/K for teams, Enter for Cool Fact playback, and S to
stop the Cool Fact. In either settings screen, Up/Down changes the focused row,
Left/Right changes its value, and Escape or Backspace returns to Game Setup.
Escape from Game Setup exits.

Run `scripts/analyze_headless.ps1` to regenerate the local Ghidra evidence and
the committed address/size function inventories, then run
`scripts/update_progress.ps1` to refresh the reports.
The app mirrors its CLI trace to `.local/logs/boot_decomp_trace.log`. All disc
data, extracted PSH files, derived frames, Ghidra projects, and generated recomp
output remain under ignored `.local/` paths and must not be published.

# Boot sequence evidence

This sequence is derived independently from the generated static recompilation
and the headless Ghidra project built from the private `FEONLY.BIN` overlay.
No captured screen is used as runtime input.

## Recovered order

1. Boot routine `0x801E1A68` loads and draws `ZLOADSCR.PSH`, then loads the
   reusable `ZLOADING.PSH` strip and transfers control to FEONLY.
2. FEONLY caller `0x80035984` calls `0x80036684`.
3. Routine `0x80036684` passes `ZLEGAL.PSH` to loader `0x80028BAC`, decodes the
   result through `0x8008AE7C`, displays it for a bounded frame loop, accepts
   controller input late in that loop, fades it out, and releases the asset.
4. Returning to `0x80035984`, the next call is `0x8002EEF4`.
5. `0x8002EEF4` first calls `0x8002D768` to preload `ZCPYRT97.PSH`, then calls
   movie player `0x8002DFB4(0)`. Movie table entry zero at `0x80093540` points
   to `Z0ZTITLE.XA` at `0x80024914`.
6. The raw movie occupies 11,693 sectors from disc LBA `146888`, and its stream
   consumer is `0x8002EAA8`. It contains 1,168 320x240 MDEC frames plus stereo
   XA audio. PS1-specific decoding yields 77.944 seconds at 14.985 fps.
7. After movie playback, the title routine reveals the preloaded title asset,
   and the caller enters its loop that draws the `press start` text.

## Press-start text recovery

Headless Ghidra and `recompiled_full.cpp` agree on the title-loop call at
`0x80035B00`:

```c
FUN_8002c6b0(1, "press start", 0x100, 0x1e, 1);
```

The caller loads `ZFONT0.PSH`, `ZFONT1.PSH`, and `ZFONT2.PSH` through
`0x80029EC0`, which assigns successive `0x100` character pages. Before the
draw, it writes zero to the active-page field at font-state offset `0x26`, so
the prompt uses page zero (`ZFONT0.PSH`). The loader arguments recover a
10-pixel space and 1-pixel glyph kerning for that page.

Ghidra's decompilation of `0x8002C6B0` shows alignment value `1` subtracting
half the measured string width from X. After applying the stored-glyph UV
transpose described below, the original glyph metrics produce a 97-pixel
width, placing the first glyph at X `208` around center X `256`.
The nominal Y is `30`; individual glyph center-Y fields provide the final
vertical offsets. The C++ renderer decodes the original archive's PSX 4bpp
bitmap chunks and BGR555 palette chunks directly.

The font loader at `0x80029EC0` also treats the bitmap Position-X halfword as
signed. When bit 15 is set, the recovered four-corner UV assignments transpose
the stored bitmap and exchange its rendered width and height. Several `ZFONT0`
characters—including lowercase `r` and `t` used by `press start`—carry this
flag. The C++ archive decoder applies the same diagonal transpose.

## Game Setup transition and assets

The title loop polls controller state through `0x800804E8` and exits when
`DAT_800FEE3C` becomes non-zero. After the loop, the recovered `0x80035984`
caller continues at `0x80035BF0`. The recompilation and headless Ghidra output
agree that it calls `0x8002FDA4`, which loads `ZTMPAL.PSH` and copies 33 palette
entries, then runs the frontend setup path. The state machine at `0x8003F7C8`
dispatches state zero to the Game Setup constructor at `0x8003F43C`.
Independently recovered asset references show:

- `0x8002F258` selects `ZTMENU1.CNK` as the frontend audio stream.
- `0x8002FDA4` loads `ZTMPAL.PSH`.
- `0x80030308` loads `ZBPAL.PSH`.
- `0x80035260` loads and relocates `ZFEMOCAP.BIN`.
- `0x80030CDC` selects `ZSET1.PSP` and `0x80031A88(0)` loads screen zero.
- `0x80031A88` reads the 95-record `ZCARD.BIN` pack and allocates four
  contiguous card records for screen zero.
- `0x80031F48` assigns those four card images uniquely to the four flags-`0x20`
  `blk1` layout records.
- the original frontend model, art, logo, and animation packs are
  `ZFEMODEL.BIN`, `ZFEPLAYR.ART`, `ZLOGOS.PSH`, and `ZFEMOCAP.BIN`.

`ZTMENU1.CNK` begins with EA `SCHl / SCCl / SCDl` stream chunks; it is audio,
not a flat menu screenshot. The actual first-screen sprite archive is the
342,448-byte `ZSET1.PSP` file at disc LBA 251190. The local decoder restores
PS1 palette-index-zero transparency and derives padded 4-bit scanline strides
from RefPack output before cropping to each directory entry's logical width.

The recovered screen-zero layout contains 49 records. It places four 128-pixel
background strips at X 0/128/256/384, `ba09` at (148,10), the NBA Live 97 and EA
logos at (30,16) and (404,16), game cards around X 50/150/260/350, and the five
bottom labels at X 50/130/220/320/410. The native renderer uses the original
background, border, logos, title, help strip, complete Level plate, and bottom
label pixels. `ba09` is registered by `0x80031F48` as a deformable object; the
C++ loop applies a small horizontal scanline wobble to reproduce its visible
jiggle.

The `c00a..c17a` option plates are ordinary axis-aligned sprites. The four
flags-`0x20` `blk1` records are not 3D model slots on screen zero: Ghidra shows
that `0x80031F48` replaces them with unique SHPP images from `ZCARD.BIN` at
(61,99), (156,93), (272,94), and (363,102). Each portrait is logically 69x63
PAL8 but stored at a 70-pixel scanline stride; decoding at 69 causes the false
diagonal shear. The private decoder uses 70x63 and crops to 69x63. The native
renderer composites these original portraits beneath the original option
plates, matching the PS1 ordering-table result. `ZFEMODEL.BIN`, `ZFEPLAYR.ART`,
and `ZFEMOCAP.BIN` belong to later frontend model paths, not these four slots.
The supplied reference image is used only for comparison and is never loaded by
the executable.

The current interaction slice has two explicit focus rows. Left/Right moves
among the four game options or enabled frontend buttons; Up/Down transfers
focus between the rows. Disabled Users is skipped by keyboard and rejected by
mouse hit-testing, matching the recovered flag test. Rosters uses an eight-card
grid and Card a three-card row, both with original normal/highlight sprites.
Activation inside those roots is intentionally intercepted until each child
flow is decompiled. Every transition, hover, disabled gate, and blocked child
activation is mirrored to the CLI trace.

The remaining bottom dispatches agree between the static recompilation and a
fresh headless Ghidra pass: `FUN_80057CE4` constructs Rosters state 9 with eight
objects, `FUN_8005CF78` owns Users state 19, and `FUN_80053F4C` constructs Card
state 11 with three objects. `FUN_8005CD88` scans twenty user records at a
0x6c-byte stride and counts the active byte at +0x5d. Game Setup passes that
count and layout object `0x2b` (tag `o06a`, Users) to `FUN_800399C4`. A zero
count applies disabled mask `0x06`; the generic menu navigator excludes objects
matching mask `0x86`.

## Local proof artifacts

The ignored Ghidra reports are regenerated by `scripts/analyze_headless.ps1`.
In the current workspace, the focused reports are:

- `.local/ghidra/feonly_zlegal_refs.txt`
- `.local/ghidra/feonly_intro_callers.txt`
- `.local/ghidra/feonly_title_xa_refs.txt`
- `.local/ghidra/feonly_menu_asset_callers.txt`
- `.local/ghidra/feonly_game_setup_screen.txt`
- `.local/ghidra/feonly_game_setup_controller.txt`
- `.local/ghidra/feonly_game_setup_callbacks.txt`
- `.local/ghidra/feonly_menu_layout.txt`
- `.local/ghidra/feonly_jiggle.txt`
- `.local/ghidra/feonly_remaining_bottom_menus.txt`
- `.local/ghidra/feonly_remaining_menu_flags.txt`
- `.local/ghidra/feonly_bottom_disabled_logic.txt`

Ghidra identifies `ZLEGAL.PSH` at runtime address `0x80024B38` and reports the
reference from the function beginning at `0x80036684`. The generated recomp
constructs the same address as `0x80020000 + 19256` and calls the same loader.

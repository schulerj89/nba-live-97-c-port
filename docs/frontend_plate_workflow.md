# Frontend logo and portrait plate sampling

2026-08-30. Fixed frontend plates now preserve the source image coordinates
instead of stretching a rectangular image into the polygon. User Setup also
uses its own descriptor depths, restoring its missing border and logo frames.
These are bounded native composition fixes, not full original GPU equivalence.

## First differing variables

The previous `blitReorderPlate` assigned rectangle UV corners to each shaped
quad. For the home plate, its upper-left UV was `(0,0)` while the original
relative UV is `(20,10)`. For the away plate, the upper-right was `(103,0)`
instead of `(86,10)`. `80031F48` applies the shape offsets to destination XY;
`80034A5C` applies exactly the same offsets to the texture origin. The result
is an unwarped image clipped by the shape. Both original retained primitive
pages independently confirm these offsets for Chicago and Seattle.

A coordinate-color probe of the previous helper disagreed at 4,853 of 4,952
known home samples and 4,984 of 5,115 known away samples. Those are sampling
comparisons under native coverage, not original framebuffer pixel counts.

Integration then exposed a separate earlier error: `renderUserSetup` traversed
only depths 4 through 1 and placed logos at 3. Its source layout at `80094444`
uses borders at 13, logos at 9 and frames at 8. `80031F48` copies the depth
without normalization. The corrected renderer traverses 17 through 1 and
reverses object indices within each bucket, matching the submission list's
prepend order. Marker positions still come from the existing retained placement
state; markers and title use depth 3, followed by Help at 1. Team Select's
separate depth-3 logo and title order is preserved in the same way.
An independent original-construction execution checks all 35 state5 depth
transfers; file/GPU hooks isolate the descriptor-to-object boundary.

## Texture extent and foreground proof

A logical 103-by-60 source uploads 104 texels per row: 52 VRAM words for 8bpp,
26 for 4bpp. Column 103 is authored padding and can be opaque. Columns 104/105
belong to the next retained VRAM word; neither the allocation bitmap nor the
free helper establishes their colors. They must not be clamped to column 102,
read from the next CPU row, or invented as transparent.

The actual opaque foreground frames cover every unavailable sample. Independent
ZSET1 and ZSET4 palette/alpha checks find 11 away and 174 home covered samples
under the retained native pixel-center rule. A conservative closed integer hull
also covers all 15 away and 233 home edge points. This larger check does not
claim the PS1 rasterizer uses those exact sample positions.

Fresh paused state3 RAM confirms both foreground frame textures and their Pal0
palette against the archive. Their XY and UV offsets agree; primitive command
`2C` is non-semitransparent. Both retained ordering-table chains put logos
19/18 before frames 17/16. Each chain contains 4,135 links. These facts establish
the bounded foreground composition without guessing adjacent VRAM contents.

`frontend_plate.cpp` owns only native pixels. It retains the prior two-triangle
coverage, samples local `(x,y)` directly, and preflights every unavailable
visible source sample against the actual later frame's alpha. Anything short
of full opacity refuses before writing the destination. Extent, alias and
coordinate checks prevent malformed inputs from turning this optimization
into an out-of-bounds read. The caller must draw that frame later, unscaled.

Team Select, User Setup, Compare, Reorder, Trade, Sign and Release use this
shared helper with their actual frames. No image decoder, private pack format,
save format or Create Player model implementation changes.

## Source inventory and limits

All counts below are full contiguous extents, not new instruction credit.
Existing owners retain their existing ledgers; these references are not additive.

| Owner | Accounted / total | Bounded use and remaining uncertainty |
|---|---:|---|
| `80031F48..8003282C` graphics construction | 0 / 569 | Shape XY and descriptor depths; full construction/lifecycle not ported here |
| `80033DD4..80034420` texture resolution | 0 / 403 | Texture origins and expanded upload boundary; general cache/lifetime pending |
| `80034A5C..80034AE4` shape UV | 0 / 34 | Fixed shape offsets agree with XY; other graphics remain outside this repair |
| `80039574..800399C4` presentation | 0 / 276 | Existing shared pump; retained packets/order inspected, complete timing pending |
| `8006763C..80067650` bucket insertion | 0 / 5 | Prepend order explains reverse object traversal; complete graphics dependency graph not inventoried |
| `8007B4B4..8007B618` image upload | 0 / 89 | Rounded word width and relevant height; native code does not emulate uploads |
| `8008AA88..8008AAF0` format helper | 0 / 26 | Contiguous extent includes one instruction outside Ghidra's 100-byte body count |
| `8007B618..8007B62C` free helper | 0 / 5 | Clears a RAM flag; no inferred VRAM clear |

Recovered behavior remains in C; this change concerns C++ resource composition
and sampling only. Global instruction/recovery totals remain unchanged.

## Evidence

- Independent upload execution: 108 original-MIPS cases / 910 assertions.
  The two actual expanded logo payloads match all 12,480 resident source bytes.
  SDK upload dispatch is hooked in the numerical test, not a live GPU transfer.
- Independent palette, frame, packet and ordering audit: 8,341 checks, including
  8,270 list-link bounds checks. The fresh RAM snapshot validates eight FE
  anchors totaling 14,764 bytes; audit-specific anchors are recorded separately.
- Public synthetic helper tests pass 47 assertions covering both shapes, identity samples, transparent
  and opaque-black texels, real versus missing edge samples, refusal atomicity,
  malformed extents, aliasing and signed clipping. No original bytes are public.
- An independent compiled-helper comparison passes 734,687 checks across 64
  real-asset cases and 12 refusal cases. Its separate exact-rational geometry
  excludes four exact boundary pixels per shape from the pixel comparison;
  this does not promote native floating-point coverage to PS1 raster equivalence.
- Native capture checks compare authored logo/frame/border pixels directly.
  Repeated captures remain regression evidence, separate from a synchronized
  original framebuffer comparison.

`scripts/verify_team_select.ps1` runs the focused helper test and checks 18,800
authored pixels per capture run with `tools/verify_frontend_plate.py`: 3,199
visible Chicago samples and 2,047 pixels per frame in both entry screens, plus
4,214 User Setup outer-border pixels. The checker validates the private layout
coordinates/depths and excludes text, title and unavailable edge samples. Its
expected colors come from authored assets, not a previously rendered golden.
All four Debug/release capture sets pass, totaling 75,200 comparisons. The
independent verifier audit rejects five actual prepatch regions (both warped
Chicago logos, both missing User frames and its missing borders) and seven
single-pixel corruptions. All five shared-helper call sites were reviewed for
side, origin, later unscaled frame composition and applicable descriptor order.

Final Debug and RelWithDebInfo builds each pass 49/49 CTest tests. All 98
Team Select/User Setup frames and 264 flash frames repeat within and across
configurations. Reorder/Compare passes 129 captures; Trade 48, Sign 26 and
Release 30. Create Player retains 27/27 repeated scenarios, 753 projected
vertices, 251 primary packet/order records and zero missing sampled texels.
Real save/configuration bytes and timestamps are unchanged, the historical
145-score/145-rank check passes, and the release desktop shortcut is refreshed.

The Reorder verifier's older audio marker names and Help/child marker counts
were stale after previous checkpoints. They now require the current audio
checks, the added atomic invalid-Help-open check and Release card check;
existing assertions remain in place. Its source receipt also includes the
new shared plate helper and test.

Final private Team Select runs: Debug `run-20260830-210819-a30a5e2f`, release
`run-20260830-210832-9589562a`. Create Player: `run-20260830-211016`.
Logs use `.local/logs/frontend_plate_*`. Roster captures are isolated fresh
directories; no previous captures or reports were overwritten.
The final Reorder/Compare receipt is under
`team_select/audit_c/team_arrow_flash/plate-reorder-8p9qrezj/` and includes
the current helper/test source hashes and a separate unchanged-save receipt.

Private source and frame receipts are under
`.local/verification/team_select/audit_a/graphics_boundary/`,
`team_select/audit_c/team_arrow_flash/logo_mapping/`, and
`.local/verification/gameplay/audit_b/logo_sample_edge/`. The fresh reference
capture is `team_select/runtime-graphics-20260830/paused-ram.bin`, at recorded
PC `800395E8`. No game input was needed for this checkpoint.

Exact PS1 edge rules, fixed-point interpolation, full VRAM/display-page history,
whole-screen pixels, live physical input and timing/audio remain pending.
This checkpoint neither completes Team Select nor launches gameplay.

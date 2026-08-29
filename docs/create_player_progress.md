# Create Player progress

The first bounded native editor slice is working: the original three-card manager is assembled from the private `ZSET5.PSP` asset pack, opens on **New** for an empty catalogue, disables **Edit/Delete** in red, and applies the recovered 40-slot availability rules. New Player now provides the complete audited 32-field path from First Name through Dribbling, including the original required-name gate and final-field boundary.

## Verified now

| Area | State | Evidence |
|---|---|---|
| Created-player catalogue | Native tested | 40 records, 68 bytes each, empty marker `0xFFFF`, IDs 493-532 |
| Parent/New availability | Native tested | Recovered owners `0x80057BDC` and `0x8004DA74` |
| Manager construction | Partial/native tested | Edit/New/Delete ordering, original ZSET5 art, disabled red variants, empty/one/full states |
| Edit/New transaction isolation | Native tested | Cancel leaves the catalogue unchanged; accept commits to the selected/free slot |
| New-player navigation | Native tested | Required names, 32-field ordering, left/right adjustment, and Down-at-Dribbling no-op |
| New-player acceptance | Native tested | START/accept saves immediately without confirmation and consumes one of 40 slots |
| Rendering determinism | Native tested | Eight 512x240 scenarios reproduce byte-identically across two runs |
| Original visual acceptance | Pending | Reference captures exist locally; exact layout/timing comparison is not yet a pass gate |
| Editor and animated model | Pending | Assets and call chain are identified, but the PS1 model/mocap renderer is not yet translated |
| Persistence/roster insertion | Pending | Must be versioned, atomic, and remain separate from source assets |

Live no$psx reference captures additionally confirm that changing Team rebuilds the animated uniform, Jersey # is composed onto the live model through the `NUM0` art path, and Height visibly rescales the model. Height is stored at created-player record byte `+9`; the recovered conversion applies the `0x3F` storage bias and preview setup routes the value through `0x80067F50`. These observations define acceptance requirements but do not mark the native editor/model complete.

The audited ratings path contains four banks: shooting (FG, 3PT, FT, Dunking), defense (Stealing, Blocking, Def Awareness, Agility), rebounding/physical (Off Reb, Def Reb, Jumping, Strength), and handling/offense (Ball Handling, Off Awareness, Speed, Dribbling). The selector flashes in place, and Dribbling is the terminal field. Exact per-field maxima and the original text-entry viewport remain open reference checks.

## Recovered model path

`0x8004DCF4` rebuilds the preview and reaches `0x80068320`, `0x8006785C`, `0x80067A14`, `0x8006781C`, and `0x80067F50`. That chain selects team/free-agent uniform assets and prepares the head, skin, materials, pose, and draw dimensions. Private `ZFEMODEL.BIN`, `ZFEMOCAP.BIN`, `ZFEPLAYR.ART`, and team-specific assets have been identified; none are published.

The primary manager/editor inventory contains 1,020 instructions across 14 functions. Three owners are behavior-complete in the current bounded slice, four are partially represented and native-exercised, and seven are research-only. Eight shared model helpers (1,018 instructions) are separately inventoried and remain research-only. These are audit counts, not a whole-feature percentage.

## Fresh verification

```powershell
pwsh -File scripts/verify_create_player.ps1
```

The verifier builds the app, runs the recovered C transaction/editor behavior tests, captures manager and editor states twice, validates all eight 512x240 outputs, and requires identical SHA-256 hashes between runs. Evidence is written only beneath `.local/verification/create_player/`.

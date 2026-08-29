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
| Rendering determinism | Native tested | Eleven 512x240 scenarios reproduce byte-identically across two runs |
| Original visual acceptance | Pending | Reference captures exist locally; exact layout/timing comparison is not yet a pass gate |
| Editor and animated model | Pending | Assets and call chain are identified, but the PS1 model/mocap renderer is not yet translated |
| Persistence | Native tested | `.n97cpl` version/generation/CRC, atomic replacement, backup recovery, no-op protection |
| Edit/Delete picker | Native tested slice | `FUN_8004E184`, states `0x20/0x21`, sparse scan, seven visible, cursor/top and hard boundaries |
| Delete confirmation | Native tested | Exact FEONLY descriptors `0x800AF352/3D6/460`, free/bench/starter branches, recovered `FUN_80040A1C` input/animation, atomic deletion and picker loop |
| Roster insertion | Pending | Recovered `FUN_8004D514` team/free-agent mutation must be integrated with the roster save transaction |

Live no$psx reference captures additionally confirm that changing Team rebuilds the animated uniform, Jersey # is composed onto the live model through the `NUM0` art path, and Height visibly rescales the model. Height is stored at created-player record byte `+9`; the recovered conversion applies the `0x3F` storage bias and preview setup routes the value through `0x80067F50`. These observations define acceptance requirements but do not mark the native editor/model complete.

The audited ratings path contains four banks: shooting (FG, 3PT, FT, Dunking), defense (Stealing, Blocking, Def Awareness, Agility), rebounding/physical (Off Reb, Def Reb, Jumping, Strength), and handling/offense (Ball Handling, Off Awareness, Speed, Dribbling). The selector flashes in place, and Dribbling is the terminal field. Exact per-field maxima and the original text-entry viewport remain open reference checks.

## Recovered model path

`0x8004DCF4` rebuilds the preview and reaches `0x80068320`, `0x8006785C`, `0x80067A14`, `0x8006781C`, and `0x80067F50`. That chain selects team/free-agent uniform assets and prepares the head, skin, materials, pose, and draw dimensions. Private `ZFEMODEL.BIN`, `ZFEMOCAP.BIN`, `ZFEPLAYR.ART`, and team-specific assets have been identified; none are published.

The recomp also establishes the shared created-player picker: `0x8004E46C` enters state `0x20` for Edit and calls `0x8004E184` once; `0x8004E768` enters state `0x21` for Delete and loops the same picker while records remain. The native controller now scans sparse occupied slots, exposes seven rows when eight or more exist, and maintains independent cursor/top offsets. Edit opens the decoded transactional editor. Delete uses the recovered `0x80040A1C` choice controller and the three original FEONLY descriptors: free-agent pool (`0x800AF352`), team non-starter (`0x800AF3D6`), and team starter (`0x800AF460`). The selected choice pulses, Cross confirms, Circle does not silently dismiss, deletion waits for the shrink/return barrier, and the durable store is committed before the live catalogue is published.

Created players persist locally in `.local/saves/created_players.n97cpl`. Each slot retains the original 68-byte record plus a small explicitly port-owned metadata section for decoded names, team, and roster membership context. Version 1.1 still reads 1.0 saves and marks their precise membership unknown. This avoids inventing retail record offsets before they are proven and leaves the format upgradeable. The file is fixed-size, bounded, versioned, generation-counted, CRC-protected, atomically replaced, and backed up; private game assets are never embedded.

The primary manager/editor inventory contains 1,020 instructions across 14 functions. Three owners are behavior-complete in the current bounded slice, four are partially represented and native-exercised, and seven are research-only. Eight shared model helpers (1,018 instructions) are separately inventoried and remain research-only. These are audit counts, not a whole-feature percentage.

## Fresh verification

```powershell
pwsh -File scripts/verify_create_player.ps1
```

The verifier builds the app, runs the recovered C transaction/editor behavior tests, captures manager/editor states and all three Delete contexts twice, validates all eleven 512x240 outputs, and requires identical SHA-256 hashes between runs. Evidence is written only beneath `.local/verification/create_player/`.

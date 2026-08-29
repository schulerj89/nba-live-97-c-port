# Create Player progress

The first bounded native editor slice is working: the original three-card manager is assembled from the private `ZSET5.PSP` asset pack, opens on **New** for an empty catalogue, disables **Edit/Delete** in red, and applies the recovered 40-slot availability rules. New Player now provides the complete audited 32-field path from First Name through Dribbling, including the original required-name gate and final-field boundary.

## Verified now

| Area | State | Evidence |
|---|---|---|
| Created-player catalogue | Native tested | 40 records, 68 bytes each, empty marker `0xFFFF`, IDs 493-532 |
| Parent/New availability | Native tested | Recovered owners `0x80057BDC` and `0x8004DA74` |
| Manager construction | Partial/native tested | Edit/New/Delete ordering, original ZSET5 art, disabled red variants, empty/one/full states |
| Edit/New transaction isolation | Native tested | Cancel leaves the catalogue unchanged; accept commits to the selected/free slot |
| New-player navigation | Native tested | Required 12-character names (retail 0x0D-byte buffers), 32-field ordering, wrapped College list, six-vblank bank scroll, left/right adjustment, and Down-at-Dribbling no-op |
| New-player acceptance | Native tested | START/accept saves immediately without confirmation and consumes one of 40 slots |
| Rendering determinism | Native tested | Sixteen 512x240 scenarios reproduce byte-identically across two runs; selector tint, mocap phase, and three scroll stages must also differ |
| Original visual acceptance | Pending | Reference captures exist locally; exact layout/timing comparison is not yet a pass gate |
| Editor and animated model | Partial/native tested | Local extractor provides 135 private D/E/F/S/shared/transform files. The real 20-part, 251-triangle body topology is decoded from retail descriptors and signed XYZ records. `FUN_80067100`/`FUN_80067378` apply the base transforms; `FUN_80069098` supplies the recovered three-root, depth-five hierarchy; both ZFEMOCAP directories now provide all 20 joint rotations, and `FUN_80062C00`/`FUN_800696C4` provide height scale and root placement. The filled smoke proves the pipeline can rasterize a 3D surface, but exact `FUN_80066090` dual-matrix composition, head/hair assembly, and GPU-packet-equivalent texturing remain pending |
| Persistence | Native tested | `.n97cpl` version/generation/CRC, atomic replacement, backup recovery, no-op protection |
| Edit/Delete picker | Native tested slice | `FUN_8004E184`, states `0x20/0x21`, sparse scan, seven visible, cursor/top and hard boundaries |
| Delete confirmation | Native tested | Exact FEONLY descriptors `0x800AF352/3D6/460`, free/bench/starter branches, recovered `FUN_80040A1C` input/animation, atomic deletion and picker loop |
| Roster insertion | Pending | Recovered `FUN_8004D514` team/free-agent mutation must be integrated with the roster save transaction |

Live no$psx reference captures additionally confirm that changing Team rebuilds the animated uniform, Jersey # is composed onto the live model through the `NUM0` art path, and Height visibly rescales the model. Height is stored at created-player record byte `+9`; the recovered conversion applies the `0x3F` storage bias and preview setup routes the value through `0x80067F50`. These observations define acceptance requirements but do not mark the native editor/model complete.

The audited ratings path contains four banks: shooting (FG, 3PT, FT, Dunking), defense (Stealing, Blocking, Def Awareness, Agility), rebounding/physical (Off Reb, Def Reb, Jumping, Strength), and handling/offense (Ball Handling, Off Awareness, Speed, Dribbling). The selector flashes in place, and Dribbling is the terminal field. Exact per-field maxima and the original text-entry viewport remain open reference checks.

## Recovered model path

`0x8004DCF4` rebuilds the preview and reaches `0x80068320`, `0x8006785C`, `0x80067A14`, `0x8006781C`, and `0x80067F50`. That chain selects team/free-agent uniform assets and prepares the head, skin, materials, pose, and draw dimensions. Private `ZFEMODEL.BIN`, `ZFEMOCAP.BIN`, `ZFEPLAYR.ART`, and team-specific assets have been identified; none are published.

The recomp also establishes the shared created-player picker: `0x8004E46C` enters state `0x20` for Edit and calls `0x8004E184` once; `0x8004E768` enters state `0x21` for Delete and loops the same picker while records remain. The native controller now scans sparse occupied slots, exposes seven rows when eight or more exist, and maintains independent cursor/top offsets. Edit opens the decoded transactional editor. Delete uses the recovered `0x80040A1C` choice controller and the three original FEONLY descriptors: free-agent pool (`0x800AF352`), team non-starter (`0x800AF3D6`), and team starter (`0x800AF460`). The selected choice pulses, Cross confirms, Circle does not silently dismiss, deletion waits for the shrink/return barrier, and the durable store is committed before the live catalogue is published.

Created players persist locally in `.local/saves/created_players.n97cpl`. Each slot retains the original 68-byte record plus a small explicitly port-owned metadata section for decoded names, team, and roster membership context. Version 1.2 still reads 1.0 and 1.1 saves, safely migrates their shorter name fields, and marks legacy roster membership unknown where needed. This avoids inventing retail record offsets before they are proven and leaves the format upgradeable. The file is fixed-size, bounded, versioned, generation-counted, CRC-protected, atomically replaced, and backed up; private game assets are never embedded.

The primary manager/editor inventory contains 1,020 instructions across 14 functions. The input slice now additionally accounts for the recovered 13-byte name buffers and 20/6-vblank selector/list transitions. Nine shared model helpers (1,029 instructions) are separately inventoried: their complete D/E/F/S asset family, relinked anchor graph, 81 source-triangle banks, 251 surface descriptors, exact private trig table, base fixed-point transform path, recovered `FUN_80069098` hierarchy, and the 11-instruction `FUN_8006734C` RTPS wrapper are native-exercised. Seven are now exercised by native code; two remain research-only. The newly translated mocap/root runtime is behaviorally tested but is not converted into additional instruction-completion credit until its remaining `FUN_80066090` boundary is partitioned and audited.

The camera audit recovered rotation `{1500,0,0}`, translation `{0,-384,3328}`, geometry offset `{256,120}`, and projection distance `H=160` (FEONLY static `0x800C6508`). The port's viewport composition remains explicit and is not counted as a recovered GTE register. `FUN_80069098` pointer assignments resolve to three roots and the exact static parent table `[-1,0,1,2,-1,4,5,6,-1,8,9,10,9,12,13,14,9,16,17,18]`. ZFEMOCAP has two six-entry directories: 12-joint `0x60` records and root-plus-8-joint `0x44` records. Create Player uses clip 1, whose 18 physical keys become 36 logical ticks through flag-8 half-key interpolation. The runtime applies all 20 local rotations, `height * 0x270` 16.16 scale, signed root lift, and root yaw. Tracing `FUN_80066090` also corrected an upstream error: a part's vertices follow its transformed joint endpoint, not its parent's origin. Exact dual/mirrored matrices and special GTE group buffers inside that function remain isolated and must be matched before the port claims original transform equivalence. Secondary head construction and textured GPU-packet equivalence also remain research/translation work. These are audit counts, not a whole-feature percentage.

## Fresh verification

```powershell
pwsh -File scripts/verify_create_player.ps1
```

The verifier builds the app, runs the recovered C transaction/editor behavior tests plus the model decoder, mocap decoder/blender, fixed-point transform, hierarchy, root/scale, and RTPS/camera numeric suites. Its private-asset runtime smoke fills all 251 decoded body faces in an auto-fit diagnostic view while retaining the exact RTPS points at the right in `.local/verification/create_player/hierarchy/smoke.ppm`. That image proves a deterministic 3D render reaches the rasterizer; it is not yet an original-reference visual match. The verifier also captures manager/editor states and all three Delete contexts twice, validates all sixteen 512x240 outputs, requires identical SHA-256 hashes between runs, and separately proves the recovered selector, mocap, and scroll phases change pixels. Evidence remains local only.

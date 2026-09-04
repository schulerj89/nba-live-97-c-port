# GAMEONLY PsyQ MoveImage recovery

`src/recovered/game_move_image.c` owns the complete 49-instruction GAMEONLY
routine `0x800997E4..0x800998A7`. The game-entry path calls it twice, at
`0x80029A94` and `0x80029AA4`, immediately after the double-buffer environment
initializer. The caller constructs this live stack rectangle:

| Field | Value |
| --- | ---: |
| Source x | 512 |
| Source y | 0 |
| Width | 512 |
| Height | 256 |

The first call supplies destination `(0,0)` and the second supplies `(0,256)`.
In PS1 VRAM terms, the routine therefore copies the 512x256 right-hand page to
both vertically stacked left-hand framebuffer pages. It seeds both sides of
the double buffer from one staged image before the game orchestration call at
`0x8002D8D4`.

## Source packet and dispatch

Every call first crosses diagnostic boundary `0x80099560` with string
`"MoveImage"` at `0x8002831C` and the source rectangle pointer. A valid extent
then updates only three words in a shared 20-byte packet:

| Address | Natural startup value after call 2 | Meaning |
| --- | --- | --- |
| `0x800C5668` | `0x04FFFFFF` | Existing packet header; untouched |
| `0x800C566C` | `0x80000000` | Existing packet header; untouched |
| `0x800C5670` | `0x00000200` | Source `(512,0)` |
| `0x800C5674` | `0x01000000` | Destination `(0,256)` |
| `0x800C5678` | `0x01000200` | Extent `(512,256)` |

The owner loads the table pointer from `0x800C55B8`, then independently loads
the dispatch context from table offset `+0x18` and the indirect target from
offset `+0x08`. The retained startup fixture uses the retail table
`0x800C5578`, context `0x8009B1F8`, and target `0x8009B298`. It dispatches
arguments `(context, 0x800C5668, 20, 0)` and returns that child's raw `v0`.
These remain explicit native boundaries: the recovered C owner does not
pretend that writing packet metadata alone moved pixels.

## Preserved retail behavior

No compatibility cleanup was added:

- The `MoveImage` diagnostic runs before either extent check, including calls
  that later return `-1`.
- Width and height are rejected only when their signed halfword is exactly
  zero. Negative values still reach the GPU dispatch.
- Destination x and y are packed from their low 16 bits.
- The first eight packet bytes are never initialized or sanitized here.
- The indirect dispatch target has no source null guard. A misaligned target
  traps; an aligned zero target remains an attempted external boundary.
- Source coordinates and extent are reread after the diagnostic callback, so
  synchronous child mutations remain visible.
- The real 32-byte stack frame, noncanonical `s0/s2/s1` save order, live
  `ra/s2/s1/s0` epilogue reloads, and partial writes on later failure remain.

## Native visual verification

`scripts/verify_game_entry_visual.ps1` continues to drive Game Setup, Team
Select, and User Setup exclusively through recovered native input handlers. It
captures the existing 98 frontend PPM frames and, after match acceptance,
executes both `MoveImage` calls against an isolated 1024x512 retained-VRAM test
fixture. The fixture contains a generated diagnostic grid rather than inferred
retail art and produces four additional proof images:

- `move-image-before-buffer0.ppm`
- `move-image-source.ppm`
- `move-image-buffer0.ppm`
- `move-image-buffer1.ppm`

The verifier requires the old buffer to differ from the source and both final
buffers to exactly equal the source. This visualizes the recovered copy while
keeping the scope honest: the diagnostic does not replace or modify the native
frontend renderer, and it does not claim a synthesized court or possession.
The receipt and semantic log record both source call PCs, packet words, live
driver target, two diagnostic calls, two GPU dispatches, and all 262,144 copied
16-bit words.

The standalone test covers both startup destinations, all 20 successful-path
operation-budget stops, zero and negative extents, coordinate truncation,
untouched packet headers, diagnostic-time rectangle mutations, live epilogue
rewrites, null/misaligned dispatch targets, unknown values, callback refusals,
prefix commits, alignment faults, mapped-memory faults, and context validation.
The startup composition test proves both natural calls use the caller's live
stack rectangle and leave the final packet targeting the lower buffer.

Fresh read-only Ghidra evidence records eight original callers and raw
instruction SHA-256
`4add213cbc7f37bd2f8e378036e62931e90896255b81495a65ffbf964dff7fca` under
`.local/verification/native_completion/game_move_image/source_evidence.txt`.

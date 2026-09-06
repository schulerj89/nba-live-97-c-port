# GAMEONLY texture-window command recovery

## Boundary and evidence

This recovery owns only `0x8009A824..0x8009A8A7`, 132 bytes and 33
instructions. The fresh Ghidra listing is
`.local/evidence/tipoff-recovery/game_8009a824.txt`; its instruction text has
SHA-256
`f2d40cd7880eaef38f2f91670a1d1221ee18c294f310c40d2a06ae425bcf7526`.
The listing has callers at `0x8009A3D4`, `0x8009A1D8`, and `0x8009A320`, and
contains no calls. Repository search found typed references from the recovered
draw-packet owner but no complete earlier owner of this routine.

The implementation follows the source access order: x byte read, stack word
store, width halfword read, stack word store, y byte read, stack word store,
height halfword read, and final stack word store. This order matters when the
input rectangle overlaps the live scratch frame. The entry branch decrements
`sp` in its delay slot on null and unknown pointers. The epilogue restores
`sp` before validating `ra`.

## Native boundary

`nba97_game_texture_window_command` accepts the full CPU machine, mapped guest
memory, per-byte knownness, an operation budget, and an optional access
journal. Guest addresses remain `uint32_t` values. A store with unknown bytes
requires a mapped knownness array; otherwise it returns `NBA97_TEXT_ARGUMENT`
without changing the destination bytes.

`nba97_game_texture_window_command_from_packet` owns only the draw-packet event
at `0x8009A3D4`, delay slot `0x8009A3D8`, entry `0x8009A824`, return address
`0x8009A3DC`, and one argument. An event that identifies this boundary by kind
or entry but has malformed metadata is rejected without mutation. Other child
events are routed to the configured typed fallback.

## Verification

The focused test uses always-active checks for null and unknown branches, all
eight access budgets, the exact journal, all coordinate bytes, signed 16-bit
extent extrema, byte-knownness combinations, stack/input aliases, mapped stack
wrap, alignment and mapping failures, atomic malformed-knownness handling,
invalid contexts, unknown `ra`, full register preservation, and deterministic
journal truncation.

The natural integration test runs the recovered draw-packet owner with the
actual draw-area-start, draw-area-end, draw-offset, draw-mode, and texture-window
owners. It verifies the five child boundaries, six packet words, and the zero
texture-budget prefix after the prior four words and the draw-mode JAL delay
store have completed.

The manager's independent original-instruction differential passed 9,216 cases
covering all 33 PCs, all 34 CPU words and masks, the full 2 MiB RAM image, null
and nonnull paths, eight input/stack alias layouts, random signed dimensions,
raw coordinates, and budgets zero through eight. The receipt is
`.local/evidence/tipoff-recovery/texture_window_command_differential.json`.

Gameplay shown: NO - no direct visual effect.

The native script composes this helper through the actual scene, draw,
packet, graphics-submission and packet-DMA owners. All five packet helpers now
execute their recovered C implementations. Two synthetic rectangles yield
E2020E18 at packet+20; each call retains four reads and four stack writes.
The mapped GPU/DMA ports record submission without GPU consumption.

Manager verification passed 1,527 focused checks, 28 natural-caller checks,
and all 325 asset-free CTests. The native run is under ignored
.local/verification/team_select/game-entry-20260906-044743-54c9c819.
Its CPU scanout before/after hashes both equal
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d.
The separate frontend screenshot shows User Setup only. Tests compare machine
fields explicitly so compiler padding is never mistaken for CPU state.

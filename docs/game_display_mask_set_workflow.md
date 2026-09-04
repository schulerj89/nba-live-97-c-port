# Display-mask service

`src/recovered/game_display_mask_set.c` is the canonical recovered owner for
GAMEONLY `0x80099458..0x800994F3`, the 39-instruction PsyQ `SetDispMask`
wrapper. GAMEONLY main calls it at `0x80029AB4` with argument one immediately
after `DrawSync(0)` has completed the two startup `MoveImage` submissions.

## Recovered source order

The owner retains the original 32-byte o32 frame and executes its observable
operations in instruction order:

1. Save incoming `s1`, `ra`, and `s0` at their original mapped stack words.
2. Read the unsigned debug byte at `0x800C55C2`.
3. At levels two and above, load and invoke the live `0x800C55BC` pointer with
   format string `0x800282AC` and the full caller argument.
4. For exact argument zero only, call `0x8009BD78` to fill the 20-byte cached
   environment record at `0x800C562C` with `0xFF`.
5. Load the driver table through `0x800C55B8`, then load slot `+0x10`.
6. Invoke that live target with GP1 command `0x03000001` for zero or
   `0x03000000` for any nonzero argument.
7. Retain the final child's raw `v0` and reload `ra`, `s1`, and `s0` from live
   mapped stack bytes.

The retail table at `0x800C5578` contains `0x8009B16C` in slot `+0x10`.
Fresh read-only Ghidra evidence identifies that ten-instruction leaf as the
GPU-control writer. It writes the command through the pointer at `0x800C5694`,
caches its low byte by command id, and leaves command id three in `v0`.
The wrapper keeps this leaf as an explicit typed service boundary so this
single-function recovery does not silently claim a physical GPU backend.

## Preserved original behavior

GP1 command `03h` uses an active-low enable bit. Consequently
`SetDispMask(1)` emits `0x03000000` (display on), while `SetDispMask(0)` emits
`0x03000001` (display off). The native translation deliberately does not
"correct" this polarity.

Other retained source-era quirks are:

- the argument is tested as a full 32-bit zero/nonzero value, not truncated;
- the 20-byte `0xFF` clear happens only before the disable command;
- diagnostic and clear callbacks can replace the table before it is loaded;
- zero and otherwise invalid aligned indirect targets are not guarded;
- diagnostic and clear return values are discarded, while final child `v0`
  remains the wrapper return;
- callback changes to saved stack words affect the epilogue; and
- failures preserve every earlier source-ordered effect rather than rolling
  back the prefix.

These include awkward or unsafe original behaviors and are covered directly
by `tests/game_display_mask_set_tests.cpp`.

## Native composition and visual proof

`game_main.c` keeps the address-bearing call at `0x80029AB4`. The native
game-entry composition recognizes that exact boundary and invokes the typed
owner with the retained RAM and stack mappings. Its concrete diagnostic
display service resolves retail target `0x8009B16C`, applies GP1(03h)'s
active-low visibility bit to an isolated retained scanout, and returns the
leaf's raw `v0 = 3`.

The existing self-driving visual test still supplies all frontend input
through recovered input handlers; it does not use computer control. After the
test reaches User Setup and accepts the controller assignment, it runs the
bounded GAMEONLY entry diagnostic and writes:

- `set-disp-mask-before.ppm`, which is entirely black while scanout is masked;
- `set-disp-mask-after.ppm`, which equals the active `0x80022070` display
  buffer after the command; and
- the JSON receipt and native trace that bind both frames to the recovered
  call, table slot, target, command word, and source-operation counts.

The after frame uses the same generated retained-VRAM grid completed by the
preceding `DrawSync`; it is diagnostic data, not retail art. The 98 native
frontend click-through frames stay unchanged.

## Evidence and claim boundary

Read-only Ghidra export records wrapper instruction hash
`28520655216df722415935c464f41c3d5921787e341e163afe573dcc44bd54f0` and
retail dispatch-leaf hash
`43224c6b6612d2c3440ea6e3a16f7e74f9b0d9711b10aba909f1d09cd97a73f2`.
The extracted GAMEONLY image independently supplies table word
`0x8009B16C` at `0x800C5588`.

This proves the wrapper, its natural startup connection, and the isolated
scanout transition. It does not claim a physical PS1 GPU, production renderer
connection, court frame, possession, or live gameplay.

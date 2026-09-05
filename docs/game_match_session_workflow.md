# GAMEONLY match-session owner recovery

`src/recovered/game_match_session.c` owns the complete 165-instruction
GAMEONLY routine `0x8002D8D4..0x8002DB67`. Main reaches it at call PC
`0x80029ADC`, immediately after the standalone frame-rate reset and before
continuing at `0x80029E58`. A fresh Ghidra instruction extraction has SHA-256
`8b903bb9beff9912b32380c6def33d0d05dae91c37bef14f99228587c1a9851e`.

This is the match-session orchestrator, not the game simulation itself. Its
ordinary source-order path is:

```text
clear (512,0,1024,512)
0x80021498 = 0
reset frame-rate tracker through 0x800A7738
define DRAWENV  0x80021EEC at (0,0,512,240)
define DISPENV  0x8002205C at (0,256,512,240)
define DRAWENV  0x80021F48 at (0,256,512,240)
define DISPENV  0x80022070 at (0,0,512,240)
write 13 draw/display/session control bytes
optionally substitute the selected team's location pointer
initialize 0x8002DB90
load scene 0x8002DB68
run game loop wrapper 0x8002DC38
teardown 0x8002DC58
optionally restore the saved team fields
0x80015021 = 0
clear (0,0,512,512)
presentation wait, DrawSync(0), then ten more presentation waits
restore the live o32 frame and return zero
```

The initializer `0x8002DB90` now composes its complete recovered owner and the
existing zero-fill owner through a narrow adapter; see
[match initializer](game_match_initialize_workflow.md). Its eleven downstream
services still require providers. The remaining `0x8002DB68`, `0x8002DC38` and
`0x8002DC58` stages remain explicit mandatory boundaries. These recoveries do
not establish a native court, possession, gameplay loop or teardown.
The parent is useful because it now preserves and exposes their exact order,
arguments, surrounding state, presentation tail and continuation point.

Compatibility deliberately retains the retail location/index bugs. The code
tests custom-location word `0x8001EC94` independently before and after the four
stages. If it becomes nonzero late, zero-initialized saved fields can be
written into a team record; if it becomes zero late, restoration is skipped.
Selected-team index `0x80021D74` is reloaded before the initial save, after
the location lookup, and separately before each restore store. A changing
index can therefore clear, patch and restore different `0x68`-byte records,
including splitting the two restore fields. There is no bounds check. The
location helper receives the sign-extended low 16 bits, not a sanitized enum.
Callback-visible prefix writes and live stack reloads are also preserved.

The standalone unit covers the 23-call ordinary path, the 24-call custom
location path, exact environment arguments and control bytes, signed location
conversion, partial-known saved fields, callback refusal/malformed values,
unknown branch inputs, live return-address mutation, every operation-budget
prefix, and the late-enable, late-disable and split-record behaviors. The
`game_main` composition proves the natural `0x80029ADC` call and continuation
at `0x80029AE4 -> 0x80029E58`.

`scripts/verify_game_entry_visual.ps1` remains fully self-driving. Its native
input test traverses Game Setup, Team Select and User Setup without computer
control, then runs this parent on the ordinary no-custom-location path. The
receipt records all 23 child entries, the 14 control-byte stores and eleven
deterministic source VBlanks. It emits `match-session-before.ppm` and
`match-session-after.ppm`; they remain pixel-identical because the initializer
changes CPU state while the remaining stages are boundaries; the test does not
fabricate their rendering. The frames are generated retained-scanout evidence,
not retail art or proof of a playable possession.

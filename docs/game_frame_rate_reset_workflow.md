# GAMEONLY frame-rate tracker reset recovery

`src/recovered/game_frame_rate_reset.c` owns the complete 14-instruction
GAMEONLY routine `0x800A7738..0x800A776F`. Main reaches it at call PC
`0x80029AD4`, after enabling scanout and registering the CRCF completion hook,
and immediately before match orchestrator `0x8002D8D4`. Its other two known
callers are `0x8002D908` and `0x80048E9C`.

Fresh read-only Ghidra extraction gives this exact source-order transition for
retail `gp=0x800D79C8`:

```text
save ra on the 0x18-byte o32 frame
0x800D7B44 = 0       frame counter
0x800D7B48 = 0       retained auxiliary word (role not proven)
0x800D7B50 = 0       instantaneous fixed-point rate
0x800D7B54 = 0       eight-sample average fixed-point rate
0x800D7B58 = 0       last report clock
clock = clock_read_800A5810()
0x800D7B4C = clock   measurement baseline
restore live ra and return with clock still in v0
```

The names are grounded in the only recovered consumer, `0x800A7460`: it
increments `0x800D7B44`, subtracts baseline `0x800D7B4C` from clock leaf
`0x800A5810`, maintains the instantaneous and eight-sample average values, and
contains the original `cmn_frate.c`, `framerate: TIMERHZ NOT SET` and rate
format strings. No consumer reference to `0x800D7B48` was found, so the port
does not invent a meaning for that word. The extracted routine bytes have
SHA-256 `e122698bfd70117a0be374f1318dd0c5a6f910e835dec0fb793cfebfda339608`.

Compatibility keeps the odd edges instead of cleaning them up. The five
clears are separate writes in original order and all occur before the child
clock read, so aliases and callback observations stay visible. The clock
sample is stored without a success/availability guard and is incidentally
left in `v0`, even though callers treat this as an initializer. Addresses stay
GP-relative, and the return address is reloaded from the live stack word after
the callback. The auxiliary word remains cleared despite its presently
unproven role. This routine does not create host timing, smooth frame rate,
clamp a value, render, or fix any retail behavior.

The standalone unit covers exact state and callback metadata, unknown samples,
the pre-callback write order, callback mutations, stack/global aliasing, the
live epilogue, callback refusal/malformed results, mapped-memory errors, and
all nine observable operation-budget prefixes. The `game_main` composition
test proves the natural `0x80029AD4` call and the following `0x8002D8D4`
boundary.

`scripts/verify_game_entry_visual.ps1` remains fully self-driving: native
recovered input handlers traverse Game Setup, Team Select and User Setup and
accept the controller assignment without computer-control clicks. The GAMEONLY
receipt seeds distinct values in all six tracker words, records the exact
before/after state, calls the retained zero source clock, and writes
`frame-rate-reset-before.ppm` plus `frame-rate-reset-after.ppm`. The function
has no GPU operation, so those two native captures are required to be
pixel-identical to the already-enabled scanout; a difference is a regression,
not an expected gameplay effect. These are generated retained-scanout proof
frames, not retail court art or evidence that a live match is running.

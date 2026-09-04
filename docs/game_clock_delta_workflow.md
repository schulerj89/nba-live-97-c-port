# GAMEONLY gameplay clock-delta recovery

`src/recovered/game_clock_delta.c` owns the complete 13-instruction GAMEONLY
routine `0x800A584C..0x800A587F`. Startup reaches it at call PC `0x80029A5C`,
immediately after the source clock and GTE initializers, and 25 additional
GAMEONLY call sites reuse it for match simulation, presentation, and animation
timing. Fresh Ghidra evidence identifies its single child as `0x800A5810`, the
four-instruction leaf that returns retained clock word `0x800D7A70`.

The routine performs this source-ordered state transition:

```text
old = *(gp + 0x164)
now = clock_read_800A5810()
*(gp + 0x164) = now
return now - old
```

With retail `gp=0x800D79C8`, the baseline is `0x800D7B2C`. The preceding
`0x800914D8` initializer resets both the live clock and that baseline to zero,
so the natural startup observation is a zero sample and zero delta. Later
callers consume nonzero deltas to pace gameplay work; this owner does not
advance the interrupt clock itself.

Compatibility retains all source edge behavior. The address remains
GP-relative and wraps as 32-bit arithmetic. The old value is captured before
the child call, even if that child later changes the mapped baseline. The new
sample is committed before the result is returned. MIPS `SUBU` supplies raw
modulo-2^32 subtraction: a wrapping counter produces its natural small delta,
while a genuinely backward sample underflows to a large unsigned word. The
port does not clamp, sign-convert, smooth, or replace it with host wall-clock
time. Live stack words are also restored after the child, so deliberate memory
aliases and mutations retain their original effects. No repairable branch bug
was found; these potentially surprising behaviors are preserved rather than
silently corrected.

Public tests cover forward time, counter wrap, backward underflow, repeated
sampling, pre-child capture, write-before-return order, stack/global aliases,
unknown samples and knownness propagation, child refusal, malformed metadata,
live epilogue mutation, alignment/resource failures, and all seven observable
operation-budget stops. Private bounded R3000 comparison covers all 13 original
instruction PCs across randomized registers, mapped memory, child samples, and
wrap cases. The raw function bytes have SHA-256
`9e68dfd7bdbb041458cf44d8eb0b48cd44f9118d4c5db44ba271c3d2a5438f39`;
evidence stays under `.local/verification/native_completion/game_timing_sample`.

`scripts/verify_game_entry_visual.ps1` uses native recovered-input handlers to
drive Game Setup, Team Select, User Setup, and the accepted controller
assignment. It captures 98 PPM frames, a structured receipt, and execution
logs. The receipt proves the zero startup baseline was sampled through the
translated owner. Because the routine only refreshes timing state and emits no
render command, those frontend frames should remain byte-identical to the
preceding capture; a visual difference would be evidence of an integration
regression, not an expected effect of this function.

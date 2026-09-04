# GAMEONLY presentation-wait wrapper recovery

`src/recovered/game_presentation_wait.c` owns the complete eight-instruction
GAMEONLY routine `0x80029BDC..0x80029BFB`. Startup first reaches it at call PC
`0x80029A64`, then calls the same routine twenty times at `0x80029B20` and
another twenty times at `0x80029B50`. Those loops bracket FELOAD setup and use
the routine as a source presentation delay.

The wrapper itself has one exact job:

```text
sp -= 0x18
*(sp + 0x10) = ra
v0 = synchronization_service_800A9CC0()
ra = *(sp + 0x10)
sp += 0x18
return v0
```

The child is deliberately still an explicit boundary. Fresh Ghidra evidence
shows that `0x800A9CC0` coordinates `DrawSync(1)` with VBlank ISR
`0x800A450C`. On the ordinary cold path it clears ready word `0x800D7A80` and
spins until the ISR sets that word. A scheduled-buffer path instead waits for
display completion word `0x800D7A84`, moves pending display-environment pointer
`0x800D7B3C` to ISR slot `0x800D7B40` inside a critical section, and lets the
ISR submit it. In both cases it clears gate `gp+0x1B4` (`0x800D7B7C` with the
retail global pointer) before returning.

Compatibility keeps the source's sharp edges. The wrapper reloads `ra` from
live mapped memory after its child, so an alias or child write changes the
actual return target. It does not sanitize the incidental child `v0`; known or
unknown register state flows through even though known callers treat it as
void. Most importantly, neither wrapper nor child has a timeout. If VBlank,
GPU readiness, or the pending display handoff never arrives, the original can
wait forever. The native owner does not turn that into a successful no-op,
wall-clock sleep, timeout, or renderer tick.

The unit test covers exact call metadata, live stack reload, arbitrary and
unknown child `v0`, callback refusal, malformed knownness, missing/unaligned
memory, overlapping regions, and all three observable operation-budget stops.
It passes strict GCC `-Wall -Wextra -Wpedantic -Werror` and an independent
ASan/UBSan build as well as MSVC Debug and RelWithDebInfo.
The raw wrapper bytes have SHA-256
`63097de0e5bda84b9bd09eef6ad2843262ba1d93f501897e0ece68bddfde6b1b`.
The referenced child has 80 instructions and raw SHA-256
`d52bcbe8ef5df04e44136001f886bbbf5140c57a2274aab68d9cf23387b30f60`;
its body is evidence for the service contract, not newly claimed ownership.
Private listings and reference audits stay under
`.local/verification/native_completion/game_call_80029bdc`.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers, accepts the controller
assignment, and captures 98 PPM frames plus logs and a structured receipt. The
GAMEONLY diagnostic then executes all 41 wrapper invocations. Its concrete
cold-path fixture acknowledges one source VBlank event per call, ending source
frame counter `0x800D7A88` at 41. This does not use computer control and does
not claim host/PS1 timing equivalence. The wrapper submits no render command,
so the captured frontend pixels remain unchanged; that absence is the expected
visual result, while the receipt and trace expose the synchronization work.

# GAME68BF8 outer match-tick recovery

`nba97_game_match_tick` owns the complete original GAME interval
`0x80068BF8..0x800691F8` and the reached frame-pump leaf
`0x8002DD84..0x8002DDCC`. It preserves the original outer restart, period,
simulation, timing, controller-input, and cleanup ordering. Source loops remain
loops and are bounded only by the explicit operation budget.

This owner is a CPU/control-flow recovery. A successful synthetic call is not
evidence that the native application naturally reached gameplay, rendered a
court, began a possession, or completed a match.

## Typed child boundaries

Four already recovered production owners are distinct required callbacks:

- player update at call PC `0x80068D84` (`0x8006801C`);
- ball simulation at call PC `0x80068D9C` (`0x8006EF60`), with the exact live
  pointer read from `0x800FDC48` and published to `0x800FDC3C` first;
- net transform at call PC `0x8002DDA4` (`0x8002DC88`);
- match frame at call PC `0x8002DDB4` (`0x80049018`).

They cannot fall through the generic service callback. A missing typed owner
refuses at its exact call boundary. Generic platform/game services likewise
refuse when unbound, and a successful callback contract means the synchronous
source effects are already visible to subsequent live reads. Successful no-op
stubs are not valid production bindings.

## Original ordering and quirks

- The `0x8002DD84` pump calls `0x8007E26C`, reads signed `0x800FDB6C` for
  `0x800798B4`, invokes the net transform, calls `0x80032B10`, and then invokes
  the match-frame owner in that exact order.
- The nonzero `0x800FDB8A` arm reads `0x800FDB6C` at `0x80068D3C`, calls
  `0x80067A60`, then deliberately rereads the live halfword at `0x80068D48`
  before `0x80067D38`. A service mutation between those reads is observable.
- The compiler executes `sll s0,s6,16` in the `0x80068D38` branch delay slot
  before this function necessarily assigns `s6`. The same incoming caller
  register can be published at `0x80068F98` when the random-timing block is
  skipped. Unknown incoming `s6` therefore refuses at its first real
  consumption; it is never silently replaced with zero.
- The timing block establishes known `s6` after its random accumulation path.
  The `fp` carry used by the odd-tick adjustment is reset for every original
  period at `0x80068C5C`, including after an outer restart.
- Delay-slot writes retain source order. In particular, the writes at
  `0x80068CF0`, `0x80068E04`, `0x80068E90`, and `0x80068EA8` precede the
  synchronous effects of their associated calls.

## Verification

Private verification lives under
`.local/verification/native_completion/game_match_tick/`.

`compare_original.py` executes 16 directed state families against the original
R3000 instructions. It compares the complete ordered read/write/call trace and
all non-stack memory, covers all 402 owned instruction PCs, and exhausts every
operation-budget refusal prefix for one seed of every directed family. Its raw
source interval hashes pin both owned ranges.

Strict standalone packages are produced with:

```text
python .local/verification/native_completion/game_match_tick/build.py
python .local/verification/native_completion/game_match_tick/compare_original.py
python .local/verification/native_completion/game_match_tick/build_linux.py
```

The first command builds and runs MSVC Debug and Release with `/W4 /WX`. The
second compares both native DLLs with the original CPU oracle. The third builds
and runs GCC/G++ Debug and Release with warnings-as-errors and UBSan.

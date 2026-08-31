# Court-startup interactive interval

`game_court_interactive.c` owns the complete 512-instruction
`47CB8..484B8` interval inside original `479B8`. It preserves both the ordinary
reset route and the controller-driven `zcheat.psh` resource/render loop. The
implementation is native C99 and directly calls the existing native SHPP count
and entry owners in `game_font_loader.c`; it does not ship original resource
bytes or an instruction interpreter.

This interval follows the separately recovered initial roster prefix. It ends
before the still-required 163-instruction `484B8..48744` synchronization and
player-packet patch interval, so completion is not the completion of `479B8`
and does not establish natural match entry.

## Inputs and real service boundaries

Call `nba97_game_court_interactive` at `80047CB8` with the actual retained
scratchpad, globals, player roots and records, digit jump tables, display state,
and loaded SHPP resource mappings. The callback must provide real synchronous
implementations of every reached source service:

- `8F224`: actual input/clock result. The port never chooses controller values.
- `29BFC`: actual loader/retry/heap owner and its known nonzero resource result.
- `99CA4`, `99ACC`, `29BDC`, and `AA0BC`: actual display/draw setup and clear
  effects.
- `994F4`: actual synchronization.
- `946B8`: the existing complete image-upload owner and actual transfer backend.
- `90698`: actual resource release.

The callback must return success only after the source-visible effects are
complete. Missing callbacks, refused calls, unknown required results and a
zero loader result fail at the original call site while retaining every earlier
event and effect. The upload owner may mutate image metadata; image width is
reread from retained memory after upload as in the source.

The SHPP helpers at `A3FE0/A3FEC` are actual native owners, not callback
approximations. They use the existing text-memory contract and consume the same
access budget. Invalid or incomplete SHPP resources refuse through those owners
rather than being filled with plausible headers or image entries.

## Preserved control flow and source behavior

The interactive path is entered only when scratchpad `+18` is exactly `20` and
the first real player-zero input result is exactly `E75`. All other states take
the ordinary reset route. Once entered, each frame and each player's rendering
and control handling runs in source order. Only an actual outer input result of
`820` exits and reaches the resource release. Native budgets can bound the loop,
but they do not inject an exit value or skip frames.

After loading, the source visits the live SHPP count and every entry, reads the
entry's first byte, and writes that byte as a **whole word** at the entry base.
It then performs the exact display-buffer swaps and environment calls. The
frame loop preserves `DCF10`'s conditional image, all eight player-input calls,
portrait uploads, live player-root traversals, height rendering, selection-bit
changes, byte increment/decrement wrapping, later clamps to `18..144`, and the
special player-zero and player-four selector writes.

Both live digit jump tables at `800260E4` and `8002610C` are read at each
dispatch. Their ten original targets are supported separately. Any other target
returns `NBA97_COURT_INTERACTIVE_CONTROL_TARGET` at the original `JR`; this is a
native ownership boundary and is not a claim that the original game would
refuse that target.

The original height-rendering bug remains: every raw byte from `200` through
`255` still draws the fixed hundreds glyph for `1`, then renders the remainder
after subtracting only 100. Thus `200` is displayed as `100` and `255` as
`155` before the byte is subsequently clamped. Signed image widths are added
with wrapped 32-bit arithmetic after the upload callback. Player roots,
records, scratch words, flags and display globals are reread wherever the
source rereads them; backing-buffer aliases can therefore affect later work.

The ordinary path writes `FFFFFDFF` to scratchpad `+4`, rereads and ORs `800`,
then clears `+10`, `+14`, and `+18`. It is not replaced by a single normalized
constant or used to bypass the interactive condition.

## Memory, journals and refusal

Source regions are fixed and disjoint in the original 32-bit address space;
their native backing buffers may alias. Context, progress and journal storage
must remain disjoint from retained data and knownness buffers and from one
another. Original-code and active-stack aliases are excluded. Only reached
bytes are checked, and each completed store establishes knowledge only for its
own bytes.

Every store and required service call is journaled in source order. A service
event is incomplete until its callback succeeds. A journal or access limit,
alignment/resource/knowledge failure, unsupported indirect target, or service
refusal preserves the exact completed prefix. These bounds are native safety
rules, not original rejection behavior. The operation is not transactional or
resumable; an integrator requiring atomic host publication must stage retained
memory, input/device state, heap/resource ownership and transfer state together.

## Verification and next boundary

Strict MSVC Debug and Release and strict GCC C99/C++17 builds each pass 926
public checks. The new owner and test use all warnings as errors; the separately
owned frozen font source disables only GCC's two existing
`misleading-indentation` diagnostics while retaining all other warnings as
errors. The checks cover ordinary and interactive entry, multiple frames,
every button route, digit mappings, display calls, mutable rereads, the raw
`200..255` bug, journal prefixes, every required-service refusal, unknown
results, access limits, invalid knowledge, alignment, aliases and unsupported
jump targets.

Each MSVC configuration also passes 284 independent original-byte cases:
33,000 ordered events, 951,053 executed original instructions and all 512 owned
PCs. The CPU oracle executes the exact GAMEONLY words and the original SHPP
helpers while stopping only at the declared external services. It compares all
retained bytes and knowledge, event arguments/results, progress, refusal
PC/address and service-prefix behavior. It covers four-frame interactive input,
all journal prefixes, randomized two-frame state, every digit and the explicit
native indirect-target refusal. No null or fabricated resource is treated as a
successful image-upload case.

Private evidence is in
`.local/verification/native_completion/court_interactive/`. The original
GAMEONLY SHA256 is
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
`source-audit.json` hashes the exact interval and disassembles each of its 512
words independently so unsupported COP2 decoding cannot truncate the view.

The exact next owner is `484B8..48744` (163 instructions): its initial real
`994F4` synchronization, ten-player context/mask traversal through the actual
`800F0ED8` ownership graph, and both packet-bank UV/page patches through the
complete `9BF98` helper. Only after that owner completes may the already frozen
`48744..487B8` texture/resource-selection bridge run. The surrounding `479B8`,
its caller, actual resources and later court/frame stages remain required for a
natural gameplay handoff.

# Native ball simulation: GAME6EF60

`game_ball_simulation.c` owns the complete `8006EF60` ball tick and its bounded
rim, backboard, rule-selection and boundary helpers. It advances actual integer
ball positions and velocities, performs owned hand attachment, and publishes
speed, court region and predicted horizontal position. It is simulation, not a
render pass. This does not yet close a whole match loop or establish possession.

## Where the original simulation runs

Fresh inspection of the original 18 instructions at `2DD84` corrects the idea
that it is the ball/player simulation entry. Its calls are `7E26C(0)`,
`798B4(signed FDB6C)`, `2DC88`, `32B10`, and `49018`: camera/script timing,
publication, text services and rendering. None is a substitute for the tick.

The real `2DC38 -> 68BF8` loop reaches `2DE34` at `68D7C`, the existing player
update `6801C` at `68D84`, then loads live `FDC48` at `68D90`, publishes that
same pointer to `FDC3C` at `68D98`, and calls `6EF60` at `68D9C`. The native
entry takes that captured original `a0` ball address. The outer loop must own
its gates and the preceding pointer publication; the ball owner does not
invent them or reread a replacement `a0`. Later collision, AI/input, pose and
render work remains in its actual source order.

## Owned closure and remaining services

The newly owned CPU closure contains 1,427 original instructions:

| Original owner | Native responsibility |
| --- | --- |
| `6EF60` (561 instructions) | Ball substeps, held/loose motion, gravity, ground bounce/friction, contact dispatch, court bounds, speed and predictions |
| `6D588`, `6D4B0` | Rim/plane tests and their ordered reflections/effects |
| `6D880` | Backboard crossing/interpolation, height snap, velocity changes and RNG |
| `5DCEC`, `5DBCC` | Private velocity-vector normalization and reflection arithmetic |
| `6EDAC`, `6ECD8`, `6F824` | Held/loose boundary handling and its required service order |
| `62D84`, `6229C`, `62300`, `62358` | Live rule gate, RNG and score-selection fields |
| `2AB70` | The original shared halfword RNG state transition |

The implementation reuses the frozen `57F5C`/`58120` attachment owner and
`7066C` integer distance helper. Hand endpoints remain actual `FED20`/`FAA04`
outputs; no frontend motion preview, guessed hand position or float physics is
introduced. The two public contact entries also expose full `6D588` and `6D880`
with their source `v0` and refusal prefixes.

The following reached calls require a real synchronous service against the
same retained memory. Missing service returns
`NBA97_BALL_SIMULATION_SERVICE_REQUIRED`; no source successor is fabricated.
Unconsumed source return bits may remain unknown.

| Callee | Captured arguments / boundary |
| --- | --- |
| `29258` | One argument: bounce/contact sound selector `0`, `1` or `2` |
| `29590`, `295C8` | One argument each: source-selected side call and its duration |
| `626A0` | No arguments; actual boundary reset effects |
| `623B0` | One argument, `1`, on the reached loose-ball reset path |
| `628FC` | Entity word0 and captured actor pointer from the actual rule gate |
| `56FFC`, `5703C` | Actor/motion `1`, then actor, only on the reached animation-reset path; existing native animation owners may be composed here with their real inputs |
| `6DC18` | Captured ball pointer; the still-unowned 711-instruction rim/scoring interaction, invoked only after both owned initial contact checks return zero |

`6DC18` has actual `6DB48`, `6DB08`, `5DBCC`, `2D358`, `29258`, `7066C`,
`2AB70`, `5DDD0`, `5847C`, `6E7AC`, `29590`, `31FE4`, and `35318` calls.
It is a concrete next gameplay frontier, not a successful no-op. The ordinary
released ball near court center can already execute this new owner without
any external service. A reached rim/scoring or sound dependency still refuses
unless its actual implementation is supplied.

## Retained memory and refusal contract

`Nba97BallSimulationContext` uses the existing checked `Nba97PlayerFrameAccess`
and a named service callback. Each read/store carries the exact source PC,
original address, width and byte-knowledge mask. The adapter must retain actual
address aliases, resolve only reached spans, validate every reached metadata
byte as canonical, and validate a store before changing its bytes. Unknown
unread fields and object padding are neither required nor initialized. Reads
are observational. Required callbacks may mutate live memory; captured locals
remain captured and subsequent source reads see those changes.

The initial three position loads precede all three old-position stores.
Opaque copies preserve per-byte knowledge; later arithmetic refuses when its
actual operand is unknown. Numeric fields require representable original
address/word bits. Private ABI stack/code must not alias the visible retained
memory; this owner does not invent a native stack address or expose private
velocity temporaries as live objects. `5DBCC`/`5DCEC` are implemented only in
that source caller's private-vector domain, not as new unrestricted public
pointer entries.

Every completed CPU store and callback mutation survives a later refusal.
The progress record reports the failing PC/address, access/service counts and
substeps. Its operation budget is a host safety bound, not game time; a partial
receipt is not resumable and must not be automatically retried. Clone/rebind
outside this owner if an application needs an atomic attempt.

Required inputs include the real ball/entity references, live phase/owner/rule
fields, signed scheduler halfword `FDB6C`, actual RNG state, selected roster
rows, and original signed court-region tables `B8A54`/`B8A5C`. Held paths also
consume actual actor/motion/header and hand data through the attachment owner.
There are no default globals, allocated identities, tables or successful SDK
services in production.

## Preserved original behavior

* `6F03C` runs once even when signed tick `FDB6C` is zero or negative. A strong
  bounce or held attachment discards remaining admitted substeps. No tick or
  blend clamp was added.
* Gravity subtracts 24 from the sign-extended vertical halfword. Full wrapped
  height addition precedes narrowing the new velocity. Friction preserves the
  original arithmetic shifts and special negative-one quotient handling.
* `6F554` compares saved full-coordinate words against small integer rim
  thresholds, whereas the earlier current-coordinate tests shift by eight.
  This confirmed source scale mismatch is preserved and commented.
* The earlier signed phase gate in `6EDAC` excludes `0x82`, making its later
  `0x82` branch unreachable. The gate is retained; the superficially similar
  reachable branch in `6EF60` is not substituted for it.
* `6D588` compares an already-subtracted old X against `15400`. Its horizontal
  `5DCEC` path discards the modified private velocity words but still takes the
  collision effects path. Both source quirks remain; no inferred correction is
  applied.
* `6D9D0` snaps height to `5400` before an upward ball returns zero. A zero
  return therefore does not mean no mutation. `6D4D8` likewise publishes a
  plane position before its velocity-direction rejection.
* `6D8CC`/`6D918` interpolate with low-32-bit products and signed division.
  With old height `800053FF`, current height `80005400` and an odd horizontal
  difference, the original reaches `BREAK 6`. Native code preserves this trap
  rather than widening the calculation. Division-by-zero guards are retained.
* RNG uses original `1EDEE`, including its source zero-seed replacement. This
  is an executed original store, not permission for the caller to assume a
  seed or to share a separate host RNG.

## Verification and integration

Private receipts under `game_simulation` check all 1,427 newly owned instruction
words against the extracted original GAME bytes, plus the 18-instruction
`2DD84` entry. Debug and Release each compare 2,314 original/native cases,
18,358 visible stores and 407,761 executed instructions. They compare ordered
reads, writes, service packets, complete visible RAM, returned contact values
and exact refusal PCs. Cases include wrapped coordinates/division traps,
signed ticks, held paths, rule/RNG branches, actual address aliases, every
store prefix and reached unknown-field refusal in selected complete traces.

External-service continuation cases explicitly use fixture effects, including
mutations between caller accesses; they verify this owner around the boundary,
not implementation of those services. Cases without a service stop at the
actual original call. The standalone tests separately check partial knowledge,
noncanonical metadata priority, callback-introduced invalid bytes and every
reached access refusal of a complete three-tick trace.

A further composed proof starts at actual `602CC`/`5D140` contact instructions,
then compares native `608A4 -> 58610` and 144 new ball-simulation calls per
configuration at explicit ticks 1, 2 and 3. The simulation calls execute with no
external-service hooks; the earlier contact sound is an explicitly declared
fixture boundary. This uses actual original trajectory tables and validates
ball movement after release. It does not prove natural controller input,
retained possession, an original-device run or a rendered live match.

Strict MSVC Debug/Release and GCC 11.4 Debug/Release each pass 309 public checks;
the GCC tests additionally run with undefined-behavior sanitization. Coverage
receipts disclose unvisited instructions rather than claiming all branches
executed: 1,389 of the 1,427 new PCs were visited, including all 190 rim-owner
instructions. Unvisited code includes guarded division exceptions, the dead
held-phase branch and other disclosed paths. The written owner retains these
source conditions; coverage alone is not evidence that an uncovered path is
unreachable.

Root integration adds `game_ball_simulation.c` and its test to the existing
recovered core. Link the existing `game_ball_attachment.c` and
`game_controller_selection.c`; no shared header or C++ adapter changes are
required by this owner. Supply the live frame-access context and actual
service dispatcher, preserve the `68D98` pointer publication, and call in the
original update order. Shared build/host integration remains the parent task's
responsibility.

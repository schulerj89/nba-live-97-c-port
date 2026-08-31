# GAME ball scoring and rim-grid workflow

## Scope

`src/recovered/game_ball_scoring.c` is the native bounded owner for GAME
`0x8006DC18..0x8006E734`. It also owns the complete CPU-only leaves needed by
that caller:

- `0x8006DB08..0x8006DB48` — signed rim-grid cell lookup;
- `0x8006DB48..0x8006DC18` — current/saved radial grid positions;
- `0x8002AB70..0x8002ABB4` — original 16-bit RNG step;
- `0x8002D358..0x8002D37C` — signed net-deformation wrapper;
- `0x80031FE4..0x80032020` — close-clock predicate;
- `0x80035318..0x80035378` — score-message setup wrapper; and
- `0x8005847C..0x800584CC` — score UI query wrapper.

Those extents total 864 original instructions. The native owner also reuses
the already verified arithmetic behavior of `5DBCC`, `5DDD0`, and `7066C`
(163 instructions) without claiming new ownership of those shared leaves.
Every listed extent is bound byte-for-byte to `GAMEONLY.BIN` SHA-256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.

This is not the whole ball simulation. The caller must pass the live ball
address captured by the outer simulation owner. The function does not reread
`FDC48` as a substitute for that argument.

## Retained-memory and service contract

The owner uses the same raw retained-memory access contract as the recovered
frame owners. Reads and writes preserve original widths, source PCs, ordering,
alignment traps, unknown-byte refusals, operation bounds, and successful
prefixes. No default ball, court, actor, clock, audio, AI, or UI state is
fabricated.

Unowned services remain synchronous calls against the same live memory:

| Entry | Role at this boundary |
| --- | --- |
| `8006E7AC` | real scorer/AI actor selection and its larger state mutation |
| `8004C374` | basket/net state and 200-vertex net deformation below the owned `2D358` sign-extension wrapper |
| `80029258`, `80029590` | gameplay audio/event services |
| `800583FC` | platform/UI query below the owned `5847C` wrapper |
| `80031C8C`, `800345E0` | score-message setup and presentation services |

Callbacks receive the exact source call PC, entry, argument count and values.
Required return widths are explicit: `583FC` returns one byte and `6E7AC`
returns the four-byte actor address. Refusal retains all earlier original
mutations and identifies the precise stopped call.

## Preserved original quirks

- The `height >= 81` arm enters at `DD80` and skips the small-radius shortcut
  at `DD18`, even though both arms invoke `DB48`.
- The steep grid walker adds its minor-axis delta on every iteration. Reducing
  it to a one-time Bresenham initialization changes visited cells.
- After the position quotients are published at `E140/E14C`, later response
  math intentionally reuses stale pre-division `a1/a2` values.
- Stack slot `sp+1C` first carries a grid coordinate, then is overwritten by
  the first reflected horizontal component. The `E324` sign test observes the
  overwritten value, not the coordinate.
- Source reads after the `E26C` and `E3FC` halfword stores are retained even
  when a conventional C rewrite could reuse the just-stored local.
- The original RNG initializes a zero seed to `A5A5`; its conditional XOR and
  16-bit truncation are unchanged.
- Original divide-by-zero and signed `INT_MIN / -1` paths remain arithmetic
  traps at their source guard PCs.

## Verification

The strict public executable passes 84 checks in each of:

- MSVC Debug `/Od /W4 /WX`;
- MSVC Release `/O2 /W4 /WX`;
- GCC Debug `-O0 -Wall -Wextra -Werror` with UBSan; and
- GCC Release `-O2 -Wall -Wextra -Werror` with UBSan.

The private original-CPU comparison passes 1,924 cases per MSVC configuration.
Each configuration compares 12,395 ordered stores, 41,938 ordered reads,
fixture-labeled service calls, refusal prefixes, retained RAM, and 541,573
original instructions. It visits 973 distinct source PCs (810/864 newly owned
words and all 163 shared dependency words). External-service
fixtures are comparison controls only; they are not implementations or device
evidence.

This proof does not establish natural match entry, a visible gameplay frame,
real audio playback, complete AI behavior, a possession, or a full game.

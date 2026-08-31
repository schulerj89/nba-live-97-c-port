# GAME period coordinator

`src/recovered/game_period.c` implements the direct state transitions and synchronous call ordering of GAMEONLY `80065DB0..800663A0` (381 original instructions). It does not replace its seven dependency owners with successful no-ops. `nba97_game_period_initialize` returns complete only after every reached callback actually completes on the candidate state and its associated context.

This is an owned native coordinator, not an emulator or an original runtime capture. Public files contain no original instruction stream, duration tables, formation data or motion assets.

## Candidate and dependency contract

The state uses explicit raw-width values with known bits, owned entity/controller references and the caller's incoming `s6` provenance. Address suffixes identify recovered fields; they are not native pointers. Width helpers expose the direct scalar/entity widths. Unknown values have zero metadata and `known=0`; zero metadata is never an invented original value.

The caller clones both the state and any mutable callback context, runs the coordinator, and publishes only a complete supported transaction. Direct writes mutate this candidate immediately. Callback effects must be visible before the coordinator's next read. A stop retains the exact executed prefix and a diagnostic receipt; it is not resumable and makes no promise to roll back external callback effects. Restart only on a fresh clone.

A null callback or a callback returning zero stops at the reached call with `CALLBACK_PENDING`; the call is recorded but not marked completed. Negative or unexpected positive callback results return `CALLBACK_FAILED`. Return one only when the requested dependency really completed. A completed callback yielding an invalid representation stops with `ARGUMENT` and retains its completion receipt. Invalid initial representations leave both candidate and receipt unchanged. Needed unknown reads stop with `UNRESOLVED`; a reached reference outside native owned storage stops with `REFERENCE`. Unknown or unowned references that are never dereferenced do not acquire invented meanings.

The receipt records every direct write and dependency call in source order. It does not duplicate internal dependency writes: those belong to the dependency's own effect/receipt contract. It holds 160 events; the verified maximum is 105. Candidate, receipt and duration objects must not overlap; callback context may own or reference candidate but must not overlap receipt or immutable durations.

| Original callsite | Required dependency | Arguments and timing |
| --- | --- | --- |
| `65DC4` | `646A8` bindings and actual role tails | Completes before signed quarter `FDB68` is captured |
| `660A8` | `65140` | Amount 120, after initial clock/controller writes |
| `66184` | `65140` | Amount 600, only captured quarter 2; current directions are read afterward |
| `661D8`, `661E8` | `65B18` | Home side 0 then away side 5; actual selected formation; special-center -1 for quarters 0/4, otherwise 0 |
| `66274` | `653E8` | Ball setup already applied, including delay-slot X=0; explicit original incoming `s6` passed unchanged |
| `6627C` | `646A8` bindings and actual role tails | After actual controller selection |
| `66284` | `60EF8` | Current render table/entity state |
| `6628C` | `5828C` and its `58260` reset | Current state, before final phase selection |
| `662E0`, `662F0` | `56B78` | Quarters 0/4 only; motion 39; resolve current entity-table slot 0, then resolve slot 5 after the first call |

The already recovered `game_player_initialization`, `game_controller_selection`, `game_player_bindings` and `game_team_roles` contracts can supply their corresponding boundaries. The lineup/substitution owners have separate mutable contracts; copy actual changed shared fields back before returning. Requesting a tail is not completing it.

## Source behavior retained

- The captured quarter is signed 16-bit and remains local after the first bindings call, even if later dependencies modify global `FDB68`. Quarter 4 sets `1EDF2=1`; other quarters leave the marker unchanged. No quarter increment is added.
- Duration uses the unsigned option byte at `21D73`. Supply both 256-word windows from actual `B895C` and `B8970`. The windows overlap by 1,004 bytes: the second starts only 20 bytes after the first. Raw options outside the ordinary five entries read adjacent original bytes; there is no fabricated five-option bound. Nonzero unsigned `1EDEC` overrides the loaded duration with 5,400.
- Signed 32-bit duration division truncates toward zero. The divisor is `max(duration/1800,10)` and the stored quotient is `805/divisor`. Each controller gets the low 16 bits of signed current duration divided by 3,600. Controller-table aliases remain aliases.
- Quarters 0/4 use formation `B891C`; every other raw signed quarter uses `B893C`. Quarter 4 writes away then home team byte `34=3`. Quarter 3 clamps only signed byte values at least five to four, preserving negative raw bytes. Quarter 2 negates current directions with 32-bit wrap, including unchanged `INT_MIN`.
- The saved ball is physical entity record 10. Ball pointers and table entries are written explicitly. Later dependencies may change global ball references, but final direct ball writes still target saved record 10. No player-selection or animation state is fabricated.
- For non-tipoff branches, signed `FDB72` is XORed with five unless the captured quarter is three. Every nonzero result selects table slot five, yet the unnormalized low 16-bit result is stored into all three side fields. This original invalid-side quirk is preserved. Target X/Y come from the actual resolved entity and can alias the ball.
- The tipoff branch sets phase `81`, timer 120 and ball `BA=360`, then calls both actual motion setters. The other branch sets phase `82`, copies the selected entity's current X/Y and writes ball height `400` and field `18=0`. No fallback position, guessed register or generic basketball behavior is substituted.

## Verification and limits

`tests/game_period_tests.cpp` covers all 65,536 raw quarter words, all 256 raw option bytes in both duration rows, source quirks, unknown/reference failures, every reached callback stop, timing of callback mutations and retained local state. It passes separately compiled MSVC `/Od` and `/O2`, with `/W4 /WX`.

Private evidence is under `.local/verification/native_completion/period/`:

- `original_instructions.txt` contains the original 381-instruction span; `verify_period.py` executes those original instructions with branch delay slots and original multiply/divide arithmetic.
- `verification.json` records 2,312 cases per native build, including 512 cases using the unchanged original duration windows. All 461,044 ordered direct-write/call events and 11,596,932 state-word comparisons at boundaries and completion match.
- 377 unique owner instructions executed. The denominator remains 381. The four unexecuted instructions are division-error paths at `66020`, `66030`, `66034`, `66038`, unreachable after the source clamps the divisor to at least ten.
- Dependency boundaries in this coordinator differential are controlled synthetic effects, explicitly identified in the receipt. No original dependency execution, natural gameplay capture or complete playable-period credit is claimed by this comparison. Each dependency needs its own implementation evidence and a composed integration run.
- Original GAMEONLY SHA-256: `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`. Builds, the private interpreter extension, original resources and logs remain private. The extension adds only instructions needed to execute this audited original owner; it is not a public runtime.

## Exact next dependency closure

`60EF8` is 49 instructions: stable signed-X insertion sorting of the eleven current `FDCC0` references. It updates entity halfword `06` only during an actual move. Already sorted entries retain any stale index; rebuilding all eleven indices would fix original behavior and is forbidden. Preserve aliases and stop on unowned references rather than replacing the table.

`5828C` is 16 instructions and calls the 11-instruction `58260`. It clears `FDB90` only if its signed value is below 128, then writes `FDBD4=0`, `FDBD6=0`, `FDBD0=FFFF`, `FDBD2=FFFF`, in that order. It does not restore the previous phase. Those four reset fields belong to the dependency context because the coordinator itself does not read or write them.

`56B78` is 42 instructions. It returns unchanged if signed animation lock `48` or `4C` is nonnegative; if both current clips `46`/`4A` already equal the full requested motion; or if flags `60`/`64` contain bit one. Otherwise it calls secondary `56AA4` then primary `5699C`. Close this actual motion-39 path with the real normalized headers and existing animation-state provenance. The forced reset path already recovered under `56FFC(entity,1)` does not establish this distinct conditional setter path.

Host integration needs to add the new source/test to its own build, map actual owned state and immutable duration resources into this contract, provide all reached synchronous dependencies, and retain a pending/failure boundary until each has executed. No shared build, host, UI or Git files are changed by this module.

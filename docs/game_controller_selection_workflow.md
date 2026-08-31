# Gameplay controller player selection

`recovered/game_controller_selection.c` implements GAMEONLY `800653E8`
(114 instructions) and integer distance owner `8007066C` (30 instructions).
It derives controller selections from existing entity claims and positions.
It does not initialize those positions, bind players to roster records, operate
native input, implement the external `7A36C` callback, or run a possession.

## Owned input and effects

The input captures eight live controller records, eleven contiguous entity
records including the ball, both pointer tables, the live ball reference,
signed tail fields `FE8CA`/`FE8CC`, and the incoming s6 register as a raw32-bit
word with explicit known/unknown provenance. The controller-selected halfword
uses the existing known/unknown type from `game_controllers.h`.

| Original access | Native representation |
|---|---|
| Eight FDC50 pointer entries | `controller_table`, references to owned controller records |
| Controller +24 / +26 | Signed `team_base` / raw selected word and provenance |
| Eleven 20BEC pointer entries | `entity_table`, references to owned entity records |
| Contiguous records beginning FDCEC, stride F4 | `entity[11]`, preserving physical record order |
| Entity +4 / +8 / +C | Signed claim halfword / signed32 x / signed32 y |
| FDC48 pointer | `ball`, an independent reference to an owned entity |
| FE8CA / FE8CC | Signed16 `tail_entity` table index / `tail_state` |
| Saved incoming s6 | `incoming_s6`, explicit raw32-bit token |

The output owns selected-word and claim effects, per-record write flags, an
ordered list of writes, the final tail-state effect and an optional `7A36C`
call request. It does not clear any other bytes or alter input data. Each
write event records the logical controller index, physical controller record,
physical entity record, raw s6 and low16 selected word. This preserves aliasing
and repeated writes that cannot be reconstructed from final values alone.

The native references can contain255 for UNKNOWN. Unknown references remain
representable and are rejected only if a source access actually needs them.
Other out-of-range reference values are invalid representations. Needed source
indices outside the eleven/eight owned records return OUTSIDE_STORAGE. Needed
unknown references or incoming s6 return UNRESOLVED. All failures leave the
output unchanged, including failures after earlier effects were computed.
These safety boundaries are explicit native refusals, never source fixes or
invented replacement values. Input/output byte ranges may overlap.

## Original selection behavior and preserved bugs

Controllers are processed in logical table order0 through7. A negative team
base skips the controller; zero selects the table0 side and any positive value
selects table5. The search first reads five physically contiguous entity records
starting at that table entry. Finding an existing claim equal to the logical
controller immediately ends its work, preserving even an invalid or UNKNOWN
controller-selected word. The native core intentionally does not repair this
source inconsistency.

Otherwise, it searches those five contiguous records whose signed claim is
negative. It compares their distance to the live ball, starting with limit800.
Candidates at exactly800 qualify. Equal distances replace the previous choice,
so the later candidate wins. It then writes the low16 selected word and claims
the entity reached through `entity_table[s6]`. The physically scanned record
and indexed write destination need not be identical when tables alias.

The source sets s6 only when a candidate qualifies. A failed search reuses the
last candidate from an earlier controller, or incoming s6 if none qualified.
This can overwrite a claim or select an entity from the other side. The C code
comments and tests preserve that bug. It does not choose a fallback or skip
the source write. If native provenance cannot resolve the stale register,
the transaction explicitly refuses instead of manufacturing a value.

Pointer-table addressing uses source `SLL s6,2`, discarding the high two bits.
For example incoming `C0000007` resolves table7 and stores selected7 while
preserving the full raw value in the event. Indices that cannot refer to an
owned table entry remain an explicit native storage boundary. `scratch_s6`
reports search scratch for verification only: the source restores incoming s6
before returning. Never save this diagnostic as the next caller's register.

The distance helper preserves all32-bit signed comparisons, wrapping shifts,
addition and negation. On ordinary bounded magnitudes its result is max plus
floor(min/4), or max plus floor((min+floor(min/2))/4) when twice min >= max.
Arbitrary raw operands also preserve source overflow quirks; for example
distance(INT_MIN,0) returns268435456. Before this helper, the selection owner
subtracts coordinates with32-bit wrapping and arithmetic-shifts each difference
right8. Floating point, Euclidean distance and truncation toward zero would
change source behavior.

## Tail callback ordering

For nonzero signed `FE8CC`, the source first resolves signed `FE8CA` through the
entity table and reads its claim, even when the later state comparison will
fail. If that claim is negative and `FE8CC<9`, it writes1 to `FE8CC`. Values2
through8 first call `7A36C`; negative values and1 write1 without that call.
State0 does not dereference `FE8CA` at all. The C core retains this ordering,
including signed negative-state behavior.

Apply each ordered controller/claim event first, then invoke `7A36C` if
requested, then publish the tail-state write. The callback must observe the
old tail state and completed selection effects. Do not publish the final tail
state early or replay final selected/claim arrays over callback changes.

Static original instructions establish that `7A36C` checks byte `BC1F0`, calls
`7A114(0)` when nonzero, then clears that byte. `7A114` conditionally pops a
20-byte record using signed count `DCE00`, publishes several state fields and
calls `799CC`. Those transitive effects are not implemented here, and no audio
or camera interpretation is asserted without their owner audit. The native
host must resolve this callback dependency before treating a requested call as
executed. The first `65DB0` selection call occurs after `FE8CC` was cleared,
so its tail branch is naturally skipped; this does not eliminate later uses.

## Evidence and next integration

Private fresh original-instruction comparison passed4,387 selection cases,
covering all114 owner instructions, and10,196 standalone distance cases,
covering all30 helper instructions. Cases include both ordinary sides,
all signed team-base branches, boundary distances, existing claims, exhausted
candidates, raw incoming-s6 high bits, aliased pointer tables, random claims and
full32-bit positions. Ordered original writes match native events; unowned
original bytes remain unchanged. All172 external callback requests match at
the correct point. The oracle records the callback and returns; it does not
claim its transitive state effects are verified.

Public C++ tests passed standalone MSVC `/Od` and `/O2`, each with `/W4 /WX`.
They cover all65,536 signed tail states with free and claimed variants, the
documented source bugs, distance boundaries, reference aliases, unknown-state
lifetime, atomic failure and overlapping input/output. There is no original
live-runtime or native host integration claim yet.

Receipts and reproducible verification stay ignored under
`.local/verification/native_completion/controller_selection/`:
`verify_selection.py`, `build_logs.json`, `verification.json`. Source hashes,
original overlay hash, full instruction lists and explicit evidence exclusions
are recorded there. Public files contain no original binary/asset fixtures.

Root integration requires the C source in native/snapshot targets and a CTest
target for `tests/game_controller_selection_tests.cpp` with C99/C++17 and the
`src` include directory. Compose it at its real caller boundary: `65DB0`
calls `65B18` for both teams, initializes ball references and positions, then
calls `653E8` at `66274`, before `646A8` at `6627C`. Provide the actual tables,
entity claims/positions and selections retained after `65328`; do not invent
positions or a first incoming-s6 value to enable this call. An UNKNOWN incoming
s6 is safe only when the executed source path establishes a candidate before
needing it. Existing claims preserve their prior selected-word provenance.

Next close `646A8` and its entity/player/status producers plus `65140`/`65070`
lineup updates, then `65B18` positioning and `56FFC` animation initialization.
Only those original producers supply a valid period selection input. The source
period ordering and remaining helpers are detailed in `game_controllers_workflow.md`.

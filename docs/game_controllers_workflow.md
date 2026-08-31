# Gameplay controller ownership

`recovered/game_controllers.c` owns the complete 48-instruction GAMEONLY
`80065328..800653E4` controller initializer. This is a semantic C effect object,
not an emulator, a controller input loop or a playable-game claim. GAMEONLY
executes at `80015000`; the same address in FEONLY is a different owner.

## State and lifetime contract

The source caller `659F0` invokes both `655B0` team initializers, then `65328`
at `65A9C`, then period owner `65DB0` at `65AA4`. It applies `65820` strategy
after that period call. Controller ownership must not move past those boundaries.

| Source field | Native effect | Behavior |
|---|---|---|
| Ten player entities, stride F4, halfword +4 | `player_claim[10]` | Set FFFF; excludes entity10, the ball |
| FDBD0 halfword | `marker` | Set FFFF; no guessed semantic meaning |
| FDC50, eight pointer words | `controller_binding[8]` | Slot i refers to owned live controller i |
| Eight controller records, stride78, halfword +24 | `team_base[8]` | Assignment1 gives0; assignment2 gives5; all others give-1 |
| Controller halfword +26 | `selected`, `selected_written` | Neutral writes FFFF; joined slots retain the prior word |
| Home/away team header +42 | `human_count[2]` | Count assignment1 / assignment2 |

Source traversal clears players9 through0 and visits controllers7 through0.
Only these fields are owned. Statistics, control maps, other controller/entity
bytes and the ball are not cleared. Bindings are stable native indices; callers
resolve them against owned records rather than manufacturing PS1 pointers.

The existing `Nba97MatchControls` owns the36 statistic bytes and59 map bytes,
not the selected field in the gap. `Nba97GameControllerSelection` therefore
records both a raw16-bit word and its provenance. KNOWN does not mean a valid
entity index: all65536 bit patterns remain representable. UNKNOWN is a native
knowledge state with zero payload, never an assertion that source RAM was zero.

A joined controller with UNKNOWN prior selection remains UNKNOWN. A neutral
controller becomes KNOWN FFFF because the source actually writes that word.
Joining again retains that FFFF until a subsequent source owner changes it.
Do not derive initial selections from controller number, assignment, saved
starter order, or C++ value initialization. Imported or retained selections need
their own producer evidence. The resident controller records survive overlay
transfer; GAME BSS clearing does not prove their selected fields start at zero.

The API validates all provenance tokens before publishing any output and
supports overlapping input/output bytes. Invalid tokens or null pointers fail
without output mutation. These are native representation guards, not claimed
retail branches. All256 assignment values remain accepted by this owner.

Source quirks are intentionally retained: joining preserves stale or invalid
selection words; nonstandard assignment bytes act neutral; this owner permits
more than five controllers on a side. The frontend's separate five-human limit
must not be silently inserted here.

## Verification and integration boundary

The public test covers72,353 assignment/raw-word cases per build, plus unknown
and retained-selection lifetimes, every invalid provenance byte in every slot,
null/payload guards, immutable inputs and overlapping buffers. Standalone MSVC
builds passed with `/Od` and `/O2`, both `/W4 /WX`.

The fresh private differential executes the actual GAMEONLY instructions on
synthetic patterned memory, then compares native effects. All6,561 ternary
assignment combinations plus253 raw-neutral diagnostics agree:6,814 cases,
68,140 comparison groups, all48 original instructions. The source oracle also
checks the exact write set, unchanged assignments, unowned record bytes and the
untouched ball. This is original-instruction evidence, not live emulator or
native host integration evidence.

Private reproducible receipts are under
`.local/verification/native_completion/controller_period/`:
`verify_game_controllers.py`, `build_logs.json`, `verification.json` and
`selection_dependency_probes.json`. Original code and fixtures remain ignored.

The native executable and snapshot tests now link this C owner. A CTest target
builds `tests/game_controllers_tests.cpp` with C99/C++17. The session retains
selected provenance independently of maps; the immutable snapshot copies the
prior tokens and effect object. Publication of the separate
after65328/before65DB0 receipt is atomic. The four accepted host receipts carry
provenance forward; the earlier team655B0 receipt remains unchanged.
Do not describe UNKNOWN joined selections as fully initialized controllers or
enable gameplay merely because this owner passed. Root owns these integration
changes; the portable module itself still owns only65328 effects.

## Exact next dependency work

1. Recover `653E8` (114 instructions) together with distance helper `7066C`
   (30 instructions) using the controller bindings, ten entity claims/positions,
   actual ball reference and explicit selection state. `653E8` visits controllers
   in ascending order, skips negative team bases and leaves a controller entirely
   alone if any of its five contiguous team entities already claims it. Otherwise
   it selects among negative claims with distance <=800; equal distances choose
   the later entity. It writes the selected controller word and entity claim.
   Its final `FE8CC`/`FE8CA` branch and `7A36C` call are separate required effects.
2. Preserve the source selection bug: register s6 is set only by an accepted
   candidate, but is used unconditionally after the search. If no candidate is
   accepted, the first affected controller uses incoming s6; later controllers
   can reuse the last accepted candidate. Do not replace this with nearest-player,
   zero, FFFF, or a no-op. A faithful native contract must represent the incoming
   value or explicitly reject an unrepresentable state. Private original-only
   probes without semantic hooks prove incoming s6=7 yields entity7 even for a
   home controller when all claims are occupied or all distances exceed800.
   Another probe proves a pre-existing entity claim leaves a stale DEAD selected
   word unchanged. These are controlled synthetic source quirks, not claims that
   a natural game reaches these states.
3. `7066C` uses integer approximate distance, not square root. On this caller's
   bounded shifted-coordinate domain it takes absolute differences, then adds
   either floor(min/4) or floor((min+floor(min/2))/4) to max; the latter branch
   applies when twice min >= max. Preserve source32-bit subtraction followed by
   arithmetic shift8 before the call, inclusive threshold800 and tie direction.
   Raw instructions are privately captured in `distance_instructions.txt`.
4. Prepare actual entity/player/status references before `65DB0`: its first call
   is `646A8` at `65DC4`. `646A8` binds active roster/status tables, reverse lineup
   indices and entity fields, then calls `6459C(0/5)` and `644FC(0/5)`. It reads
   real player-record byte+9 and traps when zero; preserve that source failure.
   Never substitute zero player records. Decode its exact first-call entity
   identity/side lifecycle from the preceding clear/registration owners before
   assuming it is identical to later calls after `65B18` initialized entities.
5. Implement period effects in original call order, not as a clock-only helper:
   `646A8`; scalar resets and clock data; `65140(120)` at `660A8`; optional
   `65140(600)` at `66184` for period2; `65B18(home)` at `661D8` and away at
   `661E8`; ball bindings/fields; `653E8` at `66274`; `646A8` at `6627C`;
   `60EF8`; `5828C`; phase/possession fields and optional `56B78` animations.
   `65140` can call `65070` to reorder CPU lineups, so the originally selected
   five saved starters are not proof of final entity/player bindings.
6. For the period tables, extract private typed data at B895C/B8970 for duration
   and B891C/B893C for positioning; do not publish raw original tables. `65B18`
   reads five signed coordinate/orientation triples, real player byte+D, and
   calls animation owner `56FFC(entity,1)` before publishing positions. Period0
   and4 use tip state81; periods1/2/3 use state82 with possession-side fields.
   Period2 negates both team directions and sets byte+35; period3 uses signed
   byte+34 comparisons to clamp values above4, so negative byte values remain
   unchanged; period4 sets byte+34 to3. Repeated `65DB0` calls during entry must
   stay distinct. Source-style arithmetic and unusual signed branches are not
   opportunities to fix the original.

Those owners and their assets/callees remain unimplemented by this milestone.
The dependency note provides exact next work; it does not claim first possession,
scene rendering, AI, movement, period completion or full-game parity.

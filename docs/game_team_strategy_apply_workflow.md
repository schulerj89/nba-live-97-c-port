# GAMEONLY team-strategy application recovery

## Boundary and evidence

This recovery owns only `0x80065820..0x800659EF`, 464 bytes and 116
instructions. The fresh Ghidra listing is
`.local/evidence/tipoff-recovery/game_80065820.txt`; its instruction SHA-256 is
`a8eb544429cee266dd170710e0a2684889dee2404446c9ba6ff6462c727739ad`.
The two known callers are the match-state reset calls at `0x80065ABC` and
`0x80065AC4`. Direct children `0x800646A8` and `0x80064DBC` remain typed
full-machine services.

Repository review found an older `nba97_match_strategy_apply` projection. Its
header identifies it as a seven-field, no-injury projection and its metadata
claims no instruction credit. It remains unchanged. The new owner retains the
complete mapped routine, including injury selection, lineup scans and swaps,
child calls, count decrement, live frame behavior, and every failure prefix.

## Source behavior retained

The launch mode writes only team byte `0x76`. The ordinary CPU path writes one
to bytes `0x76` and `0x77`. The human path reads and stores all seven paired
strategy settings in source order. These operations are kept separate so a
team/strategy alias lets an earlier store affect a later load exactly as on the
original machine.

Home and away select different injury bytes and apply the original side offset.
Values below five call `0x80064DBC` with the live team, injury, status pointer,
and cleared `a3`. Values five through eleven scan from slot eleven downward,
swap the first eligible lineup halfword pair when found, and call `0x800646A8`.
Values at least twelve skip the child and count decrement. A processed injury
decrements the callback-live team's halfword at `+0x66`, including zero wrapping
to `0xFFFF`.

## Native boundary and verification

`nba97_game_team_strategy_apply` consumes the full CPU machine, mapped guest
memory with per-byte knownness, an operation budget, and typed child service.
The reset adapter claims the exact two `0x80065820` events, including their
call PC, delay PC, per-kind invocation, return address, and one-argument
metadata. Malformed assigned events are rejected without mutation; unrelated
reset children go to the configured fallback.

The asset-free focused test covers launch, CPU, human, home/away pairs, injury
boundaries, equal/negative/eligible scans, lineup swaps, exact child arguments,
source aliases, callback-live `s0`/`sp`/HI/LO, count underflow, operation budgets
across distinct paths, partial predicates and stores, malformed knownness,
alignment, mapping, and invalid contexts. The natural test composes both actual
match-reset call sites while retaining other reset children as typed services,
and verifies nested failure promotion and both live frames.

The manager's independent original-instruction differential passed 8,320 cases
covering all 116 PCs, all 34 CPU words and masks, the full 2 MiB RAM image,
callback entry machines, all launch/CPU/human/injury paths, callback-live state,
count underflow, and strategy/lineup/stack aliases. The receipt is
`.local/evidence/tipoff-recovery/team_strategy_apply_differential.json`.

Gameplay shown: NO - no direct visual effect.

Manager verification passed 281 focused checks, 27 natural reset integration
checks, all 331 asset-free CTests, strict C99, and a private 8,320-case
raw-instruction differential spanning all 116 instructions with full machine,
knownness, callback entries and retained-memory comparison.

Native run `game-entry-20260906-051950-a9420634` drove 98 frames through the
port's input API and displayed User Setup. Both strategy calls compose on the
actual initializer/reset/roster/team-header memory. Explicit runtime-generated
strategy/injury inputs exercise home human settings and direct substitution,
and away CPU defaults and a reserve lineup swap. The original scan call keeps
lineup values 5/11 as a0/a1; counts decrease 3 to 2 and 12 to 11. Remaining
binding/substitution services are typed diagnostic fixtures. Operations are
26 (15 reads, 10 stores, one call) and 19 (11 reads, seven stores, one call).
The ignored receipt is `frames/team_strategy_apply_verified.json` in that run.
CPU before/after hashes both equal
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
No advancing native match or gameplay is claimed.

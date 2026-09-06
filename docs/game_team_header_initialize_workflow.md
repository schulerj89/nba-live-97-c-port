# GAMEONLY team-header initialization recovery

`nba97_game_team_header_initialize` owns GAMEONLY `0x800655B0..0x8006581F`
inclusive: 624 bytes and 156 instructions. The fresh Ghidra listing is
`game_800655b0.txt`, with instruction SHA-256
`c9bee46f4653d4e1c816cf14503651e4feb900aae914bcb804c9723f34d6f046`.
The only known callers are `0x80065A88` and `0x80065A94` in the recovered
match-state reset. The routine has no callees.

The C99 owner receives the complete live GPR/HI/LO machine, retained mapped
memory, per-byte knownness, an operation budget, and an optional access
journal. It preserves every source-visible load and store in order. This
includes the duplicate team-ID reads, the count store and reload, the live
count reload after every status write, the descending actor-pointer writes,
and each branch or jump delay slot. Unknown predicates, unknown return targets,
mapping failures, alignment traps, malformed knownness, and budget exhaustion
return the typed status with the completed machine and memory prefix intact.

Side word zero selects status base `0x8001F7EC`, injury byte `0x80021ED5`,
alias `0x80020B8C`, and table indices 0 and 12. Every nonzero side word selects
`0x8001F984`, `0x80021ED6`, `0x80020BBC`, and indices 12 and 24. The raw side
word still participates in five actor-table indices and is never normalized to
0 or 5. The metadata count at `0x80023AEC + 104 * team_id` clamps to 12, while
the status loop rereads header halfword `+0x66` after every write. Aliasing that
halfword with a status slot can therefore lengthen the loop; the mapped-access
budget bounds such a run without changing its prefix.

Five lineup halfwords are copied to header `+0x98..+0xA0` while actor pointers
are registered from local index 4 down to 0. Each actor receives the opposing
raw side plus the same local index at halfword `+0xD6`. The owner then stores
the opponent link, table words, direction, defaults, and rank/difficulty
thresholds. All halfword results retain low-16-bit wrap. The LBU zero
extensions prove both `BGEZ` clamps always taken, so source instructions
`0x800657B4`, `0x800657B8`, and `0x80065808` are documented as unreachable and
are never reported as executed PCs.

`nba97_game_team_header_initialize_from_match_state_reset` accepts only the two
source-proven events, including call PC, delay PC, entry, invocation, argument
count, and assigned return address. The wrapper
`nba97_game_match_state_reset_with_team_header_initialize` runs the real
`0x800659F0` owner on the same retained memory, routes both team calls through
this owner, and preserves all other reset children as typed callbacks. Each
team call has an independent budget, journal, result, and progress record;
bindings can be reused for repeated match resets.

The focused synthetic test covers both side paths, raw nonstandard sides,
counts 0/1/11/12/255, injury selection, byte extremes, exact access order,
every operation-budget prefix, full machine transport, all 16 byte-known masks,
unknown branches and addresses, malformed late bytes, stores without a
knownness destination, alignment, wraparound, live-count aliasing, and unknown
or misaligned return targets. The natural test executes both calls through the
real recovered reset owner, validates the exact event and delay metadata,
reuses the binding, and retains first-call and second-delay prefixes on nested
failure. Fixtures are generated in memory and contain no retail assets.

Visual classification: **Gameplay shown: NO - no direct visual effect**. This
owner changes CPU-side retained team, status, table, and actor-link state. The
manager composes it into the native diagnostic and owns pixel-identity capture
evidence; this module does not render or advance gameplay.

Manager verification: 1,142 focused checks, 67 natural reset integration checks,
and all 329 asset-free CTests passed. Strict C99 and a private 8,000-case
raw-instruction differential covered all 153 reachable instructions, the full
GPR/HI/LO machine, knownness, and retained memory.

Native self-driving run `game-entry-20260906-050826-d37f687e` reached User
Setup in 98 frames using native input. Both team calls ran on the actual
initializer/reset/roster memory: home count 3, away count 12; statuses, actor
links, copied lineups, direction, and thresholds passed. Runtime-generated
metadata ranks and remaining typed services are explicit diagnostic inputs.
Before/after CPU frames both hashed
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The ignored receipt is `frames/team_header_initialize_verified.json` under
that local run. User Setup is displayed; no advancing match is claimed.

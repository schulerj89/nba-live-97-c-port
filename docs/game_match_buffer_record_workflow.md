# GAMEONLY match-buffer frame recording recovery

`nba97_game_match_buffer_record` owns GAMEONLY
`0x80076B3C..0x80076FC7` inclusive: 1164 bytes and 291 instructions. The fresh
Ghidra evidence is `game_80076b3c.txt`, with instruction SHA-256
`60c9e13e670a1686ddf55fafbc6c5798fab0cac87ebd20c6feedc78fe7e6d5be`.
The ten known callers are `0x80068FE0`, `0x800665D4`, `0x800673D4`,
`0x800674F8`, `0x80067508`, `0x8006782C`, `0x80078678`, `0x80078688`,
`0x800792B8`, and `0x800792C8`.

The readable C99 owner receives all 32 GPRs, HI/LO, mapped retained memory,
per-byte knownness, an operation budget, a typed child callback, and an access
journal. It preserves delay-slot effects and source access order. It optionally
calls the existing rewind owner at `0x80076B50`, selects one of the two retained
snapshots, records controller, global, ball, and physical-entity state, then
calls the typed compression boundary `0x800767FC` at `0x80076E58`. The first
ten physical entities serialize thirteen detail bytes; all eleven serialize
their signed arithmetic-shifted XYZ coordinates. The remaining blocks apply
the unsigned cursor limit, forward record lengths, backward marker pairs,
cursor reset/publication, final cursor and flag stores, callback-live stack
reload, and the JR NOP.

The adapter composes `0x80076AD0` through the existing match-buffer rewind and
memory-zero owners. Compression remains a typed full-machine dependency because
`0x800767FC` is outside this assignment. The period-startup bridge accepts only
the two exact natural events at `0x800674F8` and `0x80067508`, including their
delay PCs, entry, kind, argument count, and assigned return address. Any event
identified by an assigned PC, delay PC, entry, kind, or return address is
claimed before fallback and then checked exactly. Period startup supplies only
GPR state, so the bridge marks HI and LO unknown instead of inventing values.
Valid GPR mutations remain visible when an accepted child returns malformed
HI/LO metadata, while the nested result remains `NBA97_TEXT_ARGUMENT`.

The focused test has 647 checks. It covers both snapshot and compression
argument orders, switch and rewind branches, signed clock clamping, byte and
halfword truncation, all thirteen detailed fields, the untouched eleventh
entity fields, arithmetic coordinate shifts, controller alias ordering,
cursor lengths 0, 21, 22, 34, and 255, forward and backward marker equality and
mismatch, limit publication, unsigned wrap, bounded runaway prefixes, every operation-budget prefix of
the main fixture, child refusals, callback-live SP/HI/LO, malformed accepted
GPR/HI/LO states, exact parent guards, malformed later bytes, partial stores to
memory without knownness tracking, mapping overlap, stack wrap, JR failures,
untouched machine words and masks, and deterministic bytes, knownness, and
machine state. The 28-check natural test runs the real period-startup owner,
both natural recording calls, the real rewind and zero owners on the first
call, and the typed compression callback. It also checks wrapper reuse with the
same binding and RAM, the first-call budget failure, and valid GPR prefix
propagation for malformed HI and LO child state.
All fixtures are generated on the heap at runtime and contain no retail assets.

Strict Clang C99 with `-pedantic-errors -Wall -Wextra -Werror`, strict Clang
C++17, and MSVC C11/C++17 `/W4 /WX` compile cleanly. The focused and natural
executables pass. The manager's independent original-instruction comparison
passes 7,376 cases covering all 291 PCs, all 34 machine words and masks, full
2 MiB RAM, callback machines, mutable stack/cursor/backward-marker paths, and
all 430 access prefixes for the first sixteen fixtures. Its ignored receipt is
`match_buffer_record_differential.json`.

Manager integration needs the new owner and adapter plus the existing
`game_match_buffer_rewind`, `game_memory_zero`, `game_period_startup`,
`game_match_state_reset`, and `game_match_buffer_initialize` owners and the
existing match-buffer rewind adapter. No shared build file is changed here.

Visual classification: **Gameplay shown: NO - no direct visual effect**. The
routine records retained CPU-side match state and advances buffer cursors; it
does not render or directly advance gameplay.

Manager integration passed all 341 asset-free CTests and all progress/metadata
freshness checks. Native input-API run `game-entry-20260906-061120-9bb034d1`
composes both exact period calls on the same retained diagnostic memory, with
the real rewind and zero owners on the first call. Each recording serializes
eleven physical entities, makes 209 reads and 194 stores, and clears the
preceding real pending flag. Total operations are 405/404 with two/one
callbacks. Both return to the natural caller at SP=0x801FFEE8.

The controller/entity pointers and records are explicit runtime fixtures; the
compression callback supplies only a declared cursor result. It does not
produce encoded data or claim the missing compression owner. Both CPU proof
frames have SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The captured native screen remains User Setup, with no advancing match.

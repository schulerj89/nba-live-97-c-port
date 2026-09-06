# GAMEONLY first-period speech announcement recovery

`nba97_game_tipoff_announcement` owns GAMEONLY `0x8007EF4C..0x8007F073`
(296 bytes, 74 instructions). The boundary comes from the fresh Ghidra listing
`game_8007ef4c.txt`; the instruction-byte SHA-256 is
`ae33453495d7aee1dd8f89080731150e43cd7c65a72b5e2151d213dd0172473d`.
The sole observed caller is first-period startup at call PC `0x80067450`.

The C99 owner retains all 32 GPRs with per-byte knownness. It creates the
0x20-byte source frame and performs the `s0` save in the first JAL delay slot,
then applies the signed `v0 < 8` gate. A passing result restores and returns
without reading announcement globals. The active path calls `0x8007FA50(0)`
and reads the unsigned mode byte at `0x80021D70`.

Mode 2 builds the two-part announcement in source order. It preserves `s2`
through the `0x8007FA9C` delay, calls `0x8007ECA4`, performs the first
`0x800B1E14`, reads the live words at `0x80021D74` and `0x80021D78`, and calls
`0x80083748` for each. The first lookup result is captured in the second lookup
JAL delay. The next JAL delay wrapping-adds the second result into live `s1`.
The two `0x8007FA9C` results and live `s2/s1/s0` then feed `0x8007ECEC`.

Other modes start with `s1=3` in the `0x8007F00C` branch delay. Only mode 1
reads signed word `0x8001EC94`; a value strictly greater than zero changes the
selector to 5. Two `0x8007FA9C` calls feed `0x8007E8C4`. Both paths finish with
`0x800B1E14`, then reload `ra/s2/s1/s0` through the callback-mutable live stack
pointer and execute wrapping `sp += 0x20` before consuming `ra`.

Every original child remains a typed callback carrying the exact call PC,
delay PC, entry, invocation, operation order, argument count, and complete live
GPR file. No child algorithm or audio output is fabricated. Guest memory uses
validated `uint32_t` regions with exact little-endian widths, alignment checks,
mapping failures, overlap rejection, and byte-knownness propagation.

`nba97_game_tipoff_announcement_from_first_period_startup` is the production
adapter for the natural `0x80067450` event. It shares retained memory and the
full GPR file with the recovered first-period owner, returns the exact nested
failure prefix, and rejects any other parent event.

The focused synthetic runtime test covers signed gate boundaries, modes
0/1/2/255, signed mode-1 values, exact mode-2 call arguments and wrapping sum,
mutable `s2/s1/s0/sp` and saved words, partial knownness, unknown branch and JR
targets, aliasing, wrapping stack addresses, alignment/mapping failures, every
child refusal, and all 23 operation-budget prefixes. The integration test
executes the actual recovered first-period owner through its `0x80067450`
event for early-gate, mode-1, and mode-2 paths, including a nested failure.

This routine has no direct visual effect. It only orchestrates CPU-side speech
services through typed callbacks, so it proves neither audible output nor an
advancing rendered tip-off. Manager-owned native capture and shared test
registration provide repository-level evidence after integration.

The native input-driven game-entry diagnostic now composes this exact adapter
under first-period startup. Runtime-generated speech services cover mode 2's
wrapping sum (0xFFFFFFF0 + 0x30 = 0x20) and mode 1's positive-flag selection 5.
The nested receipt records 23/16 operations, 7/6 reads, four stack stores,
12/6 calls, exact announcement arguments and restored RA=0x80067458. Matching
before/after diagnostic scanout hashes prove no direct pixel effect; the
separate frontend screenshot remains User Setup. Speech services are fixtures,
so audible playback and an advancing tip-off are not claimed.

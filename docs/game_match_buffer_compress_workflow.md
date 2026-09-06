# Match-buffer delta compressor recovery

This recovery owns only GAMEONLY `0x800767FC..0x800768EF`, the 244-byte,
61-instruction compressor called by the frame-recording routine at
`0x80076E58`. The fresh Ghidra instruction stream has SHA-256
`903fcb4272bf882ee14901f4938131758c86949ebe45770b5d04959e84b98638`.
There are no child calls.

The owner receives the full live machine and mapped retained memory. It reserves
the low 16 bits of `((a3 + 7) >> 2)` bytes for the record length and grouped
flags. It then reads each `a0` halfword before its paired `a1` halfword and
encodes the modular difference with two-bit code 0, 1, or 3. Code 1 writes the
low byte for differences in the modular signed-byte interval -128 through 127;
code 3 writes low then high bytes. Four codes form each flag byte. The final
partial flag is shifted into the source position, the low total-length byte is
written at both record ends, and the full halfword at `0x800F9FFC` is toggled
with XOR 1.

Count termination observes only the decremented low halfword. Count zero and
65536 therefore process 65536 pairs, while the original full count still
affects the initial header calculation. The implementation preserves wrapping
arithmetic, every live register and HI/LO mask, source/output/global aliases,
ordered failure prefixes, alignment and mapping traps, and the `v0=a2+1` JR
delay result before return-address validation. Every attempted retained-memory
access consumes one operation and can be inspected through the access journal.

`nba97_game_match_buffer_compress_from_record` accepts only BW's assigned
compression event: call PC `0x80076E58`, delay PC `0x80076E5C`, entry
`0x800767FC`, return address `0x80076E60`, invocation 1, argument count 4, and
the delay-assigned `a3=0x82`. The wrapper claims the assigned kind, call PC,
delay PC, entry, or known return address before exact validation, so malformed
assigned events cannot reach an unrelated fallback.
`nba97_game_match_buffer_record_with_compress` runs the actual BW owner and
keeps its rewind event as a separate typed dependency.

The focused test builds heap-backed 2 MiB retained-memory fixtures and covers
all delta widths, counts 1 through 9, 130, 65535, 65536, zero, high-word and
wrapping counts, exact layouts and access cutoffs, partial knownness, malformed
bytes, aliases, mapping/alignment failures, return traps, and deterministic
machine/memory output. The natural test composes the real BW frame owner in both
snapshot-selection paths, tests typed rewind fallback, child failure promotion,
exact event guards, and binding reuse. The focused suite reports 261 checks and
the natural BW suite reports 291 checks. The independent manager differential
compared 9,136 cases against the original instructions across all 61 PCs, all
34 machine words and masks, full 2 MiB RAM, aliases, and 540 access cutoffs.

Gameplay shown: NO - no direct visual effect. The routine changes retained CPU
record bytes and a selector halfword; later match-buffer consumers determine
any visible behavior.

The complete asset-free Debug CTest suite passed 343/343 tests (5.17 seconds).
Progress, C recovery, instruction-semantics, and roster freshness checks passed.
The native input-driven run `game-entry-20260906-062026-ff1fa198` exercised six
compressor calls through the real period startup and frame recorder on retained
synthetic RAM. Each pair produced independently decoded 222-byte and 35-byte
records, with selector transitions 0 to 1 and 1 to 0. These calls respectively
performed 484/297 accesses, including 261 reads and 223/36 stores. No extra
compressor input fixture was introduced. The CPU-only before/after frames share
SHA-256 `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The displayed frontend remains User Setup; this is not an advancing match.
Private partial-byte classification validation additionally checked 1,280 cases
against all 65,536 concrete halfword differences.

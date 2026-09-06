# GAMEONLY rotation matrix recovery

## Evidence

The owner recovers GAMEONLY `0x80056080..0x800562CB` from the fresh Ghidra
listing `game_80056080.txt`: 588 bytes and 147 instructions with instruction
SHA-256
`dead4863c9e67d31eb16d64b222c9a6e5b13a8d118ff0cc66251fac9928a5133`.
Known callers are `0x8004C1C0`, `0x8004C2F4`, `0x80051168`, and `0x80052078`.
The routine has no children.

The C99 owner uses direct, source-addressed control flow. It preserves all 32
GPRs and HI/LO, per-byte knownness, signed angle branches and delay masks,
unsigned MULTU products, negation before arithmetic shift, and the JR delay.
It reads the packed 4096-word table directly through validated guest memory;
no floating-point trigonometry, host pointer cast, opcode table, generated
instruction corpus, or runtime asset is present.

## Source memory order and aliases

The source reads signed angles at A0+0, A0+2, and A0+4. X and Y table words are
read before any matrix write. The Z angle is cached before the first write, but
its table word is read only after stores at A1+4, A1+0xA, and A1+0x10. The
remaining stores occur at A1+0, +2, +6, +0xC, +8, and +0xE. This exact order
preserves A0/A1 aliases and lets those first three writes change the live third
table lookup.

The natural adapter accepts only the recovered camera-frame-transform event
`0x80051168 -> 0x80056080`, its delay at `0x8005116C`, two arguments, matrix
event kind, and known RA `0x80051170`. It passes the parent's full machine and
retained memory into the owner and returns the exact completed or failure
prefix.

## Runtime checks

The focused executable performs 237 always-active checks. It covers all eight
angle-sign combinations; angles -32768, -4096, -1, 0, 1, 4095, 4096, and
32767; all nine store PCs, addresses, widths, and order; packed signed-halfword
extrema; unsigned HI/LO behavior that differs from signed multiplication;
negation-before-SRA and fixed-point wrap; preserved GPR, SP, A0, A1, and RA
state; table/output/angle aliases; the live third lookup; partially known table
words, source-domain invariant MULTU bytes, the negated `-32768` table value
that becomes `+32768`, independent LO/HI completion masks, and the redundant
post-negation BGEZ prefixes; unknown angle and output addresses; every
memory-operation budget prefix; alignment, missing and overlapping regions;
malformed known bytes, masks, and zero register; wrapped A0/A1 addresses;
unknown RA after all nine writes; and an unavailable-knownness store that
leaves output bytes unchanged.

The integration executable performs 10 always-active checks through the actual
recovered `0x80051168` caller. It proves exact event metadata and RA, full
matrix completion within the parent, the parent's remaining typed children,
nested budget failure prefixes, and adapter rejection without machine mutation.

The private original-instruction differential passed 2,045 cases across all
147 PCs, full 2 MB memory, all 32 GPRs plus HI/LO, budgets 0 through 15, every
angle sign, angle extrema, unsigned MULTU, and output/angle/table aliases.

Both executables are generated entirely from runtime synthetic memory and a
synthetic packed table. Strict validation uses C99/C++17 with `-Wall -Wextra
-Werror -pedantic-errors` and an exact count of 147 source instruction sites.

## Dependencies and presentation

Production dependencies are the shared mapped-memory/full-machine types and
the recovered camera-frame-transform event contract. No new PS1 routine or
native math dependency is introduced.

Gameplay shown: **NO - no direct visual effect**. This owner updates CPU matrix
memory; visible output requires an advancing native renderer path to consume
that matrix. The focused and natural tests prove the matrix writes and preserve
the expected CPU-only classification.

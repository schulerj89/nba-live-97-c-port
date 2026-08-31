# Texture-page offset service

`game_page_offset.c` is the reusable typed C owner for GAME
`8009BF98..8009C060`. It implements the original texture-page calculation and
retains `993DC` as a live byte-read boundary at `800C55C0`. Saved registers and
stack traffic are private ABI details; the original function has no visible
stores.

## Preserved source behavior

The first `993DC` result is compared with one. A value of one selects the
alternate encoding and skips the second call. Every other first value,
including two, causes a fresh `993DC` read. Only a second value of two selects
the alternate encoding. This double-read is intentional: synchronous readers
may change retained memory between samples, and the implementation never
caches a graphics mode across them.

Both encodings mask texture mode and ABR to two bits, X to ten bits before its
six-bit shift, and the source-specific Y page bits. Full 32-bit argument words
are accepted because the original receives registers and performs its own
masks. The full original V0 result is returned as a `uint32_t`; no platform
graphics default, coordinate clamp, or normalized texture descriptor is
substituted.

The read callback sees the actual `993E0` load PC, address `800C55C0`, and byte
width. Unknown data, malformed knowledge, a missing reader, or reader refusal
leaves the result untouched. A failed second sample retains the fact that the
first read completed. Calls are not resumable.

## Verification boundary

Private evidence under
`.local/verification/native_completion/game_page_offset/` audits every word of
`9BF98` and `993DC` against the retained GAME image. Strict MSVC Debug/Release
and GCC/UBSan builds run the public tests. Original-R3000 comparisons cover both
branch encodings, all live first/second mode transitions, argument masks and
edge words, and exact read counts/order. The comparison is a bounded original
CPU experiment over declared inputs, not a natural startup, rendered frame,
first possession, or gameplay claim.

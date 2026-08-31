# Native per-frame net transform (`2DC88`)

`nba97_game_net_transform` owns all 63 GAME instructions at `2DC88..2DD84`.
It is the fixed-global transform called by `2DD84` before the match-frame
renderer. It has no children, does not draw the net, and does not initialize a
match or camera resource.

The owner binds directly to `Nba97PlayerFrameContext` retained memory. Each
fixed original address is resolved at the instant the source accesses it;
callback side effects and refusal prefixes remain observable. No source value,
camera default, address, or zero is manufactured. Only the memory callback,
user pointer, and native operation budget are used.

## Exact source behavior

The first word read is `FDBA0`. A nonzero value returns immediately without
reading any other input or touching any destination. With a zero gate, the
source performs these writes in order:

1. Halfword zero to `FA63C`, `FA630`, `FA632`, then `FA634`.
2. `FA638 = 0x800 - FC9B0` and `FA63A = 0x800 - FC9AC`, stored as halfwords.
3. Position halfwords `FAB98`, `FAB9A`, and `FAB9C` from `FC9A0`, `FC9A8`, and
   `FC9A4`, respectively, multiplied by live `B2044`.

`FC9A8` is not read until after all six transform halfword stores. `FC9A4` is
not read until after the `FAB98` store. The native owner preserves those live
reads rather than preloading the three positions.

MIPS `MULT` is signed, but only its low 32 bits are consumed by `MFLO`; signed
and unsigned multiplication therefore produce the same retained low word. If
that low word is negative, the original adds seven with 32-bit wrapping before
arithmetic shift-right by three, then negates with 32-bit wrapping. This is
signed truncation toward zero of the low word, not division of the full 64-bit
product. Each result is finally truncated by `SH`. The port uses unsigned
arithmetic to preserve these wrap points without native C signed-overflow UB.

All stores are halfword stores in original order. Destination neighbors and
upper bytes are retained. Unknown reached bytes, malformed metadata, missing
ownership, callback refusal, or the native operation bound stop at the exact
access while keeping earlier writes. The function is not resumable or atomic.

## Verification scope

The public asset-free test has 1,603 checks per configuration covering the gate,
exact 16-event active ordering, callback-driven live rereads, all
operation-budget boundaries, every source-read unknown prefix, status
passthrough, malformed normalized references, edge low-word products, and
deterministic random arithmetic cases. Strict MSVC Debug/Release and GCC
Debug/Release pass; GCC Debug also runs undefined-behavior sanitization.

Private Debug and Release verification each execute 932 cases through the actual
63 GAME instruction words: 14,540 ordered retained-memory events, 55,324
original instructions, and 28 byte-unknown refusals. The native owner agrees on
every read/store value, all retained bytes/knownness, status, and stopped
PC/address. All 63 source instruction addresses are reached. The exact source
interval SHA-256 is
`bbb793c5b034dc27f0a2fdfaa82be7a8e9bba8d4ab7e5b4551986d3011486a73`.

This owner is only the `2DC88` transform. It does not prove `2DD84`, natural
match entry, connected gameplay, net pixels, a first possession, or a complete
game.

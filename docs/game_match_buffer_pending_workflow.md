# Game match-buffer pending flag recovery

`nba97_game_match_buffer_pending` owns GAMEONLY
`0x80076B28..0x80076B3B` (20 bytes, 5 instructions). The fresh Ghidra
listing is `game_80076b28.txt`; its instruction bytes have SHA-256
`7a8289f8a38324a1bcda832e1d355ab0d9e95facf5f5cdc4872c81cd6628c7e3`.

The leaf assigns `v0=1`, forms `at=0x80100000`, stores byte 1 at
`0x800FE864`, and returns through live `ra` after a NOP delay. It takes no
arguments and calls no children. Every other GPR and HI/LO remains unchanged.
The two fixed register assignments survive a store failure. A completed store
also survives an unknown or unaligned return target because JR occurs later.

The owner carries all 32 GPRs, HI/LO, and per-byte knownness. Its single guest
write uses validated `uint32_t` retained memory, preserves exact budget and
failure prefixes, and never casts the guest address to a host pointer. The
access journal records the store at `0x80076B30` as operation one.

The natural adapter composes the actual recovered period-startup owner at its
two source calls: `0x800674F0/0x800674F4` with JAL return `0x800674F8`, then
`0x80067500/0x80067504` with return `0x80067508`. It claims any event matching
the assigned kind, entry, or either call PC, then validates the complete
boundary before execution so malformed assigned events cannot reach fallback.
Other period-startup children retain their typed fallback path.

Period startup transports only its full GPR file. The adapter therefore copies
those GPRs into the leaf machine, explicitly marks unavailable HI/LO unknown,
and copies only GPRs back. This is source-safe because the five instructions do
not read or write HI/LO.

Asset-free focused tests generate memory at runtime and cover every prior flag
byte, budgets zero and one, all return-address known masks, an unaligned JR,
unmapped and malformed-knownness stores, a raw destination without a knownness
plane, invalid machine metadata, malformed and overlapping regions, exact
journaling, full-machine preservation, and deterministic memory and knownness.
Natural tests execute the real period-startup owner, verify both calls and all
interleaved fallback children in source order, cover first- and second-call
failure prefixes, repeated binding use, and malformed call guards.

Gameplay shown: NO - no direct visual effect. This CPU leaf changes one
retained match-buffer byte; the manager-owned native period capture verifies
matching pixels while state evidence records the write.

Manager verification passed 8,788 focused checks, 373 natural-caller checks,
strict Clang C99, all 339 asset-free CTests, and progress/metadata freshness.
An independent original-byte decoder compared 8,192 cases across all five
instructions, all 34 machine words and byte-known masks, every prior byte,
budgets zero/one, and aligned/misaligned/unknown JR outcomes. Private receipt:
`.local/evidence/tipoff-recovery/match_buffer_pending_differential.json`.

Native input-API run `game-entry-20260906-055614-72070e0e` recorded both
actual period-startup call sites on the same retained memory in three period
fixtures. Each call stores one at 0x800FE864, returns V0=1/AT=0x80100000,
and preserves caller SP=0x801FFEE8. HI/LO remain explicitly unknown across
the older GPR-only caller. This is synthetic period-state proof, with no
advancing match. Both CPU frames have SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The visible native screen remains User Setup.

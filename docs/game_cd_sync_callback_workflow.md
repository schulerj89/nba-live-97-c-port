# GAMEONLY CdSyncCallback recovery

`src/recovered/game_cd_sync_callback.c` owns all six instructions of GAMEONLY
`0x8009DBF8..0x8009DC0F`. A fresh read-only Ghidra listing and decompile show
the complete operation:

```text
previous = *(uint32_t *)0x800C57E8;
*(uint32_t *)0x800C57E8 = replacement;
return previous;
```

The original bytes have SHA-256
`a5f87457838841a01d7e1d1695406ed58575fa304d34b46e5ef4eb106cadddae`.
Main reaches the owner at call PC `0x80029B44`, immediately after
`CdReadyCallback`, and passes NULL to remove the command-completion callback.
Other source call PCs are `0x8002B70C`, `0x8002BB14`, `0x80091F44`,
`0x80091FC4`, `0x8009D988`, `0x8009F8F0`, `0x8009F998`, `0x8002D244`,
`0x80092360`, `0x80092760`, `0x8009FE74`, `0x8009FEF4`, `0x800A0044`, and
`0x800A0158`.

The old PsyQ SDK signatures for `CdSyncCallback` and `CdReadyCallback` have the
same normalized 24-byte body, so the byte shape alone cannot name this routine.
The global resolves the ambiguity: internal `CD_sync` `0x8009E740` reads
`0x800C57E8` at `0x8009E8BC`. The adjacent recovered `CdReadyCallback`
`0x8009DBE0` exchanges the distinct callback word at `0x800C57E4`.

The diagnostic preloads `0x8009DA04`, the default sync callback installed by
earlier `CdInit` `0x8009D94C`, then proves that main receives that pointer and
clears the slot. `CdInit` is still a typed boundary, so this retained input does
not claim its complete body. This owner stores emulated PS1 state only: it does
not register a Windows callback, invoke native code through the raw address,
access a CD drive, or participate in the host filesystem loader.

Compatibility preserves the source behavior:

- the previous word is read before the replacement store;
- any raw replacement is accepted, including NULL, unaligned, or unmapped
  pointer values;
- neither the old nor new callback is invoked here;
- a partially unknown old word remains an unknown return value, but does not
  suppress the known replacement store; and
- there is no pointer ownership check, synchronization guard, or repaired
  return convention.

Native mapping, knownness-metadata validation, and operation-budget failures
are explicit host safety boundaries, not original branches. A stopped prefix
retains the exact source-visible read/store progress and register value.

`tests/game_cd_sync_callback_tests.cpp` covers the main NULL exchange,
arbitrary raw pointers, NULL-to-NULL replacement, unknown prior bytes, exact
two-operation execution, both bounded prefixes, malformed mappings, and
argument validation. `tests/game_main_tests.cpp` composes it at the natural
main call and checks the old and new retained values.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers—never computer-control clicks—
then reaches this owner through recovered main. It captures
`cd-sync-callback-before.ppm`, `cd-sync-callback-after.ppm`, the old/new values,
source receipt, and trace. The frames must be pixel-identical because callback
exchange performs no rendering. They prove reachability and absence of a direct
visual effect, not a retail frame, CD-device backend, or playable possession.

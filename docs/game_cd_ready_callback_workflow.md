# GAMEONLY CdReadyCallback recovery

`src/recovered/game_cd_ready_callback.c` owns all six instructions of GAMEONLY
`0x8009DBE0..0x8009DBF7`. A fresh read-only Ghidra listing and decompile show
the complete operation:

```text
previous = *(uint32_t *)0x800C57E4;
*(uint32_t *)0x800C57E4 = replacement;
return previous;
```

The original bytes have SHA-256
`98c5f9f745cd61ca8a7268bf74d7dea2419d421b67d277c31d38f64b41113414`.
Main reaches the owner at call PC `0x80029B3C`, immediately after `CdSync`,
and passes NULL to remove the ready callback. Other source call PCs are
`0x8009D978`, `0x8009FABC`, `0x8009FC4C`, `0x8009FC80`, `0x8009FE64`,
`0x8009FEEC`, and `0x800A0144`.

The old PsyQ SDK signatures for `CdReadyCallback` and `CdSyncCallback` have the
same 24-byte normalized body, so the byte shape alone cannot name this routine.
The global resolves the ambiguity: internal `CdReady` `0x8009E9C0` reads
`0x800C57E4` at `0x8009EB78` and invokes it on its ready path. The adjacent
six-instruction routine `0x8009DBF8` exchanges the distinct callback word at
`0x800C57E8` and remains the next untranslated routine.

The diagnostic preloads `0x8009D9DC`, the default ready callback installed by
earlier `CdInit` `0x8009D94C`, then proves that main receives that pointer and
clears the slot. `CdInit` is still a typed boundary, so this retained input does
not claim its complete body. Likewise, this owner stores PS1 callback state only:
it does not register a Windows callback, invoke native code through the raw
address, access a CD drive, or participate in the host filesystem loader.

Compatibility preserves the source behavior rather than sanitizing it:

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

`tests/game_cd_ready_callback_tests.cpp` covers the main NULL exchange, arbitrary
raw pointers, NULL-to-NULL replacement, unknown prior bytes, exact two-operation
execution, both bounded prefixes, malformed mappings, and argument validation.
`tests/game_main_tests.cpp` composes it at the natural main call and checks the
old and new retained values.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers—never computer-control clicks—
then reaches this owner through recovered main. It captures
`cd-ready-callback-before.ppm`, `cd-ready-callback-after.ppm`, the old/new values,
source receipt, and trace. The frames must be pixel-identical because callback
exchange performs no rendering. They prove reachability and the absence of a
direct visual effect, not a retail frame, CD-device backend, or playable
possession.

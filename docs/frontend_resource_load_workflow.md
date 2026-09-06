# FEONLY frontend resource-load descriptor recovery

`nba97_frontend_resource_load` owns `0x8007B1D0..0x8007B2BB` (236 bytes,
59 instructions), source SHA-256
`16756cd9554b869085b0f84eb6b2f1b9fe0931e7bb07f40c9f08ce90a3677c26`.
It preserves the exact stack, branch-delay, descriptor, global publication,
close, optional dynamic-callback, and callback-live epilogue behavior.

The nonzero result from `0x8008A2C8` is deliberately discarded. Child
`0x8008A594` receives its fifth mode argument through `sp+16` in the JAL delay.
All stack output words, the allocated descriptor word, `0x800D9AE8`, and the
callback at `0x800D9B50` are reloaded after intervening callbacks. The dynamic
target is latched at the JALR and validated after its delay writes `a3`.

The natural adapter accepts only the committed `frontend_load_payload` call at
`0x8007B164`, delay `0x8007B168`, entry `0x8007B1D0`, argc three, FEONLY,
invocation one, and fully-known `ra=0x8007B16C`. The six children remain typed
dependencies: cache lookup, descriptor open, allocation, postload, close, and
optional callback. Tests and capture provide synthetic implementations; those
fixtures do not establish filesystem, heap, pump, or loader ABIs.

The focused receipt covers all 59 PCs, all branch paths, exact child arguments,
operation limits, refusal prefixes, knownness and target failures. The natural
integration composes actual DD, DE, and DF owners with explicit fixtures only
at DF's unresolved children. The separate standalone DF CPU capture records
the full path; its first missing boundaries are `0x8007B1F0 -> 0x8008A2C8`
(lookup) and then `0x8007B214 -> 0x8008A594` (info/open). This routine has no direct visual output. Gameplay shown:
**BLOCKED** until the real resource lifecycle and advancing native match loop
exist.

Manager validation passed on final MSVC Debug sources: 231 focused checks, 126 counted integration checks, and all 411 asset-free CTest tests in 31.95 seconds. Integration asserts the full 34-word/mask returned machine for normal, relocated, and absent-knownness-plane paths and the exact two parent stop PCs for nested refusal/limit cases. The private original comparison passed 6,912 cases across all 59 source PCs and all six call sites, comparing full 2 MiB retained RAM, all 34 CPU words/masks, and exact access/callback/PC journals. Progress, recovery, instruction-semantics, and roster freshness checks passed.

Native input verification passed 124 captured frames in ignored run `.local/verification/team_select/game-entry-20260906-144508-215e3f7b`. The `frontend_resource_load_verified.json` receipt checks all 59 executed PCs, all 19 exact accesses, six complete callback machines and the full epilogue machine. Native before/after PPM pixel SHA-256 both equal `42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7`. The exact next dependencies are lookup `0x8007B1F0 -> 0x8008A2C8` and info/open `0x8007B214 -> 0x8008A594`, followed by heap, read, close and optional callback services.

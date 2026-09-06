# Game match-buffer initialization recovery

`nba97_game_match_buffer_initialize` owns GAMEONLY
`0x8006432C..0x80064387` (92 bytes, 23 instructions). The fresh Ghidra
listing is `game_8006432c.txt`; its instruction bytes have SHA-256
`e3f3b0accccac912d89dda8e1b1357e39cc3159ed73359ad0532d8cb765f260a`.

The owner allocates a 0x20-byte frame, forms destination `0x800F9FFC`, and
saves `ra`. It calls the recovered zero entry `0x800A3A74` with delay-slot
length `0x378`. After the clear, it stores halfword `0x76` at `0x800FA000`,
word `0x800CCC00` at `0x800FA004`, and the wrapped sum `0x800D5734` at
`0x800FA008`, in that order. It then calls the typed `0x80076AD0` boundary,
reloads `ra` through callback-live `sp`, and advances that `sp` by 0x20. The
final child's raw `v0` remains live.

The native owner transports all 32 GPRs, HI/LO, per-byte knownness, mapped
guest memory, exact access order, and operation budgets. Both children may
mutate the entire machine and retained memory. The fixed register assignments
and header stores after the first child overwrite only the registers and bytes
written by the original instructions. Aliases between the cleared range,
header words, and saved frame remain observable.

The adapter composes the existing complete `0x800A3A74/0x800A3A78` zero owner
without copying its algorithm. Fresh listings prove the entry forces `a2=0`
and falls through the shared core with the fixed positive length `0x378`.
The bridge preserves incoming `v0` byte-knownness and HI/LO, establishes known
zero `at/a2/t2` before the first SWR, updates `t0/t1` only after a successful
store, and returns the zero owner's exact working `a0/a1`. The second child
`0x80076AD0` remains an explicit full-machine callback.

The natural adapter intercepts only BN match-state reset's non-mode-98 event
at `0x80065AF8`, including delay `0x80065AFC`, entry `0x8006432C`, and
JAL return `0x80065B00`. All other BN services use the configured fallback;
in particular, mode 98 continues to call its original `0x80076AD0` event at
`0x80065AE8`. Asset-free integration executes the actual BN owner, this owner,
and the actual zero owner, checks BN continuation and epilogue restoration,
and covers nested zero limits, core limits, final-child refusal, and repeated
binding reuse.

Focused tests cover the full journal and both call machines, every operation
cutoff, callback refusal and malformed machines, partial saved-register
knownness, atomic invalid-known-byte failures, mapping and alignment errors,
live-SP restoration failures, header/stack/zero aliases, 32-bit stack wrap,
full GPR/HI/LO mutation, and deterministic repetition. All fixtures are
generated in heap-backed memory and contain no retail assets.

Gameplay shown: NO - no direct visual effect. This CPU initializer changes a
retained buffer and dispatches initialization services; matching pixels are
expected until later match systems consume the state.

Manager verification passed 178 focused checks, 21 natural BN/reset integration
checks, all 333 asset-free CTests, strict C99, and a private 3,840-case
raw-instruction comparison covering all 23 instructions, full GPR/HI/LO,
knownness, callback entry machines and 2 MB retained memory. A separate
400-case zero-adapter comparison covered 73 reachable instructions of the
existing zero owner, all incoming V0 masks, and first/middle/tail budgets.

Native run `game-entry-20260906-052322-4320d973` drove 98 frames through the
port input API and displayed User Setup. The actual initializer/reset invokes
this owner and the real zero owner on the same retained memory. An explicit
runtime sentinel proves all 888 buffer bytes after clearing and header writes;
the remaining typed cursor service observes both initialized pointers. The
owner records seven operations, one read, four stores and two callbacks; zero
records 223 stores. The ignored receipt is
`frames/match_buffer_initialize_verified.json` under that local run.
Before/after CPU frames both hash
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
No advancing match or gameplay is claimed.

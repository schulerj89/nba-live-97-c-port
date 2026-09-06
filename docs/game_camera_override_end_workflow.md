# Game camera override end recovery

`nba97_game_camera_override_end` owns GAMEONLY
`0x8007A36C..0x8007A39F` (52 bytes, 13 instructions). The source is the fresh
Ghidra listing `game_8007a36c.txt`, whose instruction bytes have SHA-256
`c0c28c611a2e440826289a731844389556bef691a7048f82d5409a147cf90a91`.

The routine loads byte `0x800BC1F0` before allocating its 0x18-byte frame. Its
branch delay saves `ra` for zero, nonzero, and unknown flag values. A zero flag
skips the child and preserves the loaded zero in `v0`. A nonzero flag calls the
typed `0x8007A114` boundary with `ra=0x8007A388` and delay-slot `a0=0`.
Only a completed child reaches `lui at,0x800C` and the byte clear. The child's
raw `v0` remains the routine return. Refusal or an invalid returned machine
leaves the flag uncleared and exposes the exact stopped prefix.

All 32 GPRs, HI/LO, per-byte knownness, guest memory, access order, and
operation budgets remain observable. Child mutations to registers, memory,
`sp`, and saved stack words are live. The active path overwrites the child's
`at`, clears the flag even if the child changed it, then reloads `ra` through
the child's live `sp`. Tests cover flag values 0, 1, and 255, unknown branch
state after the save delay, every operation cutoff, callback refusal and
invalid machine state, raw `v0`, full-machine mutations, flag/stack aliases,
32-bit stack wrap, unknown pointers and return addresses, alignment, mapping,
knownness, metadata, and repeatability.

The natural caller at `0x800653E8` is an existing normalized controller
selection owner. Its output records ordered selection writes, a
`call_7a36c` request, and the final tail-state write, but intentionally carries
no live GPR/SP/HI/LO state. The adapter therefore runs the actual selection
owner and requires a separately proven full machine with the real call return
address `0x80065578` only when the leaf is requested. It applies ordered
selection writes first, executes this owner, and publishes the tail-state write
only after success. Child changes to selection state are preserved because the
adapter does not replay final arrays after the callback. On refusal, ordered
writes and child changes remain while final tail state is withheld. This is an
explicit composition boundary, not a fabricated caller ABI.

The second caller at `0x80063A88` remains outside the recovered composition.
The sole callee `0x8007A114` remains typed; the already recovered `0x800799CC`
is only a transitive service and does not establish ownership of `0x8007A114`.

Gameplay shown: NO - no direct visual effect. This owner proves CPU flag and
dispatch state only; matching pixels are expected until `0x8007A114` and a
live camera path are recovered and connected.
Manager verification: 131 focused checks, 17 actual selection integration checks, strict C99 and all 273 asset-free CTests passed. Private original comparison passed 7,168 cases covering all 13 PCs, every flag byte, 34 machine words, full 2 MB memory, callbacks, all budget prefixes, stack/flag aliasing and frame relocation. Native input run game-entry-20260906-000754-7eb16dc5 captured 98 driver states; CPU receipt proves flag 1->0, tail state 2->1, controller selection 4/claim0 before child, raw V0 CAFEBABE, 5 operations/2 reads/2 stores/1 child. Before/after CPU frame SHA-256 both 391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d. Visible frontend remains User Setup; independent CPU fixture does not prove a live match or camera.

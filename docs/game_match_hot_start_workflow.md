# GAMEONLY match hot-data startup recovery

`nba97_game_match_hot_start` owns GAMEONLY `0x80066F88..0x800670A7`
inclusive: 288 bytes and 72 MIPS instructions. A fresh read-only Ghidra export
from `/GAMEONLY.BIN` reports function `FUN_80066f88`, the same inclusive range,
72 instructions, and source-byte SHA-256
`cb3d0a2d3babd3aa2b21eecd88e29bf5252b08189303ea11c287b23c81ff8092`.
The independently refreshed export is byte-for-byte identical to
`.local/evidence/tipoff-recovery/game_80066f88.txt`. Its callers are the
recovered match tick at `0x80068C24`, `FUN_8002dff4` at `0x8002E070`, and
`FUN_8002e0ec` at `0x8002E1B0`. Fresh exports independently confirm the first
two containing functions as `0x8002DFF4..0x8002E0EB` (62 instructions, hash
`8edcdaf4309115409d6e5e58076f1a4194e0b14f632c4d6e7476f298d4a9d2f1`) and
`0x8002E0EC..0x8002E1FF` (69 instructions, hash
`f53c5722b557c150da40e9d4c5dccee6c67dd579d05269eff6e7a84e0c320303`).
Repository ownership search found only the
documented prefix-table projection in `game_pose_sample`; that helper explicitly
excludes this routine's stack, live registers, resource calls, retry loop, and
temporary state, so it is not a complete owner.

The routine first creates its `0x20`-byte frame and saves `ra`, `s1`, and `s0`.
For each of 84 slots it stores the low 16 bits of the running offset, reads the
left pointer and optional unsigned byte at `+7`, then reads the right pointer
and optional unsigned byte at `+7`. Null pointers contribute zero. The signed
MIPS `slt` selects the larger zero-extended value, including ties and `255`, and
the running `addu` retains 32-bit wraparound. Pointer cursors advance right then
left, and the prefix cursor advances in the loop branch delay slot.

After rereading the live root at `0x80020BEC`, the routine calls `0x80051ED8`
with `a0=*s0`, `a1=0x4E`, and `s1=1` assigned in the JAL delay slot. Its source
do-loop then clears `0x800D7AF8`, calls `0x800A72BC` with the `zhots.bin` name at
`0x800275B8` and destination `0x800C6400`, stores raw `v0` at `0x800FE91C`, and
stores callback-live `s1` at `0x800D7AF8`. A zero `v0` retries without a source
timeout. The native operation budget can expose a deterministic completed
prefix; it does not turn that prefix into success or resumable state.

On success the routine clears the halfword at `0x8002148C`, uses callback-live
`s0` to read `*(s0+0x20)`, `*s0`, and the unsigned byte at the first result's
`+9`, then calls `0x80051ED8` again. It reloads saved `ra`, `s1`, and `s0` through
the child-mutated live `sp`, advances `sp` by `0x20`, and consumes the restored
`ra` at the source JR. Every retained access is little-endian, alignment and
mapping checked, and journaled with its source PC. Per-byte unknown values flow
through stores and arithmetic until a branch or guest address actually needs
them. Address/target aliases such as `lbu v0,7(v0)` and
`lw s0,0xBEC(s0)` snapshot their effective address before replacing the GPR;
the access journal retains that address while observers see the post-load GPR.

Both dependency entries remain full-32-GPR typed callbacks. The existing
`game_player_attributes` owner includes `0x80051ED8`, but its aggregate API
does not expose this call's full GPR and mutable guest-stack/access prefix;
using it here would require an explicit interface extension, not an ABI guess.
The first callback
observes the `s1=1` delay-slot assignment, and all callbacks may mutate retained
memory and any live register. No behavior of `0x80051ED8` or `0x800A72BC` is
invented or translated here.

`nba97_game_match_hot_start_dispatch_tick` is the narrow natural-caller
adapter. The existing match-tick service record proves only call PC, target,
and source arguments; it has no entry stack pointer or full GPR image. The
adapter therefore requires a separately supplied `Nba97GameMatchHotStartContext`
whose full register and retained-memory provenance has already been established.
It never creates production fixture registers. A completed owner call lets the
tick proceed to `0x80068C2C -> 0x80079664`; an incomplete owner remains an
incomplete service and retains its exact result in adapter state.

The focused asset-free runtime test generates all memory at run time and covers
all 84 prefix outputs, null and dual pointers, equal values, unsigned `255`,
wrap/truncation, exact table/payload order, zero retries followed by success,
persistent bounded retries, child and observer refusal, raw partially unknown
loader returns, live `s0`/`s1`/memory/stack mutation, final arguments and delay
slots, every operation-budget prefix, alignment, address wrap, missing mapping,
unknown data, source-region aliasing, and malformed metadata. The integration
fixture supplies an explicit synthetic full entry context, invokes the existing
`nba97_game_match_tick`, completes this exact first service, and proves that the
natural caller stops at its next missing service.

Visual classification: **Gameplay shown: NO - no direct visual effect**. This
owner changes CPU prefix data, a retained resource pointer, and startup flags.
It has no rendering call, and the natural-caller fixture stops before an
advancing rendered match. No frame or gameplay claim follows from these state
changes.

Manager verification: the direct test and natural-caller integration pass with
211/211 asset-free CTests. An ignored original-instruction differential ran
4,050 cases and visited all 72 instructions, comparing full memory, all 32
final GPRs, every child-entry GPR image, and all operation-budget prefixes.
The native input-driven run records `match_hot_start_verified.json` under
`game-entry-20260905-183644-0b5725e2`. Its explicit synthetic full entry context
produces 84 checked prefixes, 393 operations (295 reads, 94 stores, four child
calls), two loader attempts, pointer `0x80130000`, flag 1 and cleared halfword.
The existing tick then refuses camera startup at `0x80068C2C -> 0x80079664`.
This CPU fixture does not fill the legacy tick's missing live GPR/stack API.
Before/after scanout SHA-256 is identical:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.

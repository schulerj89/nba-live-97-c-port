# GAMEONLY stream queue count recovery

`nba97_game_stream_queue_count` owns GAMEONLY
`0x80084448..0x80084587`, 320 bytes and 80 instructions. Fresh Ghidra evidence
identifies instruction SHA-256
`9c34b212baa379cc2205a2b9858b6cfe771c32d183acb141da987b39859a59c1`.
Known callers are `0x80088D30`, `0x8008BC68`, and `0x80088C4C`. The ordered
children are the typed no-argument boundaries `0x80093D94` at `0x8008447C`
and `0x80093DD4` at `0x8008455C`.

The owner creates a 0x20-byte frame and clears its local count. An initially
null queue head at `0x800C43A0` returns `-1` immediately, without calling a
child or touching the counter. Every nonnull initial head calls the lock child,
increments `0x800C4410` with 32-bit wrap, rereads the head, and stores it through
callback-live `s8`. A null head introduced by the callback is not accepted as
an early empty result: it becomes the traversal pointer and is dereferenced if
address zero is mapped, or reports the corresponding mapping failure.

Traversal reloads the frame pointer independently for comparisons with
`0xFFFFFFFE` and `0xFFFFFFFF`, then reloads it again before reading the node.
A nonzero node increments the wrapping local count. The code reads the local
pointer and node again, branches on that second node value, and for nonzero
values repeats both reads once more before updating the local pointer. These
reads are retained because stack/node/global aliases can make their results
differ after the intervening count store. Neither sentinel is dereferenced.
One focused alias case makes the lock callback publish `frame+0x14` as the
head while changing that count word to `0xFFFFFFFF`. The first node read sees
nonzero, the count increment wraps the aliased node to zero, and the second
node read skips the third reread and pointer store. The next loop retains the
old local pointer and exits on that zero word, returning zero after unlock.

On an ordinary exit the owner rereads and decrements the global counter before
calling the unlock child. It then loads the raw count through unlock-mutable
`s8`, moves live `s8` to `sp`, restores `ra` then `s8`, advances `sp`, and
returns. Both children may mutate all 32 GPRs, HI/LO, memory, `sp`, `s8`, and
saved stack values. Their failure prefixes remain exact. A cyclic list remains
cyclic; the native operation budget stops a deterministic prefix without
decrementing the counter, calling unlock, restoring the frame, or fabricating
resumability.

All words retain one knownness bit per little-endian byte. Wrapping ADDIU
arithmetic derives only invariant bytes, equality can disprove a sentinel from
one known mismatching byte, and unknown branch predicates stop after the
source NOP delay. Raw partially-known local counts can return successfully when
restored `ra` is known. Guest addresses remain validated `uint32_t` values;
unaligned, unmapped, wrapped, and aliased accesses never become host pointers.

`nba97_game_stream_queue_count_from_stream_readiness` accepts only AD's exact
full-machine no-argument `0x80088D30 -> 0x80084448` event with NOP delay at
`0x80088D34` and JAL `ra=0x80088D38`. The composed adapter executes the actual
recovered AD owner. Synthetic natural cases prove AG raw results `-1`, zero,
one, and two are consumed by AD with its signed `<2` rule; AD returns one for
the first three and zero for two. A zero AD flag skips AG completely.

Runtime-generated focused tests cover null and sentinel heads, zero/one/many
links, sentinel termination, exact repeated-read order, both counter wraps,
all operation-budget prefixes, bounded cycles without cleanup, callback
refusal and malformed machines, mutable GPR/HI/LO/sp/s8/ra/frame state,
post-unlock raw knownness, stack/node aliases, unknown predicates and pointers,
alignment, mapping, true stack wrap from entry `sp=0x10` through frame
`0xFFFFFFF0` and low addresses `0x4/0x8/0xC`, and invalid metadata. No retail
assets or binary fixtures are used.

Visual classification: **no direct visual effect**. This CPU-only traversal
does not draw, play audio, or advance gameplay. State, access, and callback
receipts provide its evidence; no audible playback or gameplay is claimed.

Manager validation: 158 focused checks, 31 actual readiness integration checks,
strict C99, and all 257 asset-free CTests passed. Private differential proof
compared 3,150 cases against original instructions, covering all 80 PCs, the
complete 2 MB fixture, all 32 GPRs plus HI/LO, callback entry state, and budgets
0 through 44. Cases include null/sentinels, links/cycles, counter wrap, live
SP versus S8, relocated frames, post-unlock result mutation, and local-count
aliasing that makes the second node read zero.

Native run `game-entry-20260905-221241-2b4719a3` composes the actual readiness
call at `0x80088D30` within the existing match-audio diagnostic. Two generated
nodes yield one counted link in two iterations, 30 operations, 20 reads,
eight stores and two explicit critical-section service calls. Counter
`0xFFFFFFFF` wraps to zero then returns to `0xFFFFFFFF`. The owner returns 1
through frame `0x801FFE90`, restored SP `0x801FFEB0` and RA `0x80088D38`.
The parent readiness predicate also returns 1. The ignored
`stream_queue_count_verified.json` records these checkpoints, native input
frames and matching CPU frame hashes:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The CPU scanouts are diagnostic patterns. The separate frontend capture
remains Boston/Chicago User Setup; neither playback nor gameplay is shown.

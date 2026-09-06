# GAMEONLY graphics submission workflow

`nba97_game_graphics_submit` owns GAMEONLY `0x8009B298..0x8009B57B`
(inclusive), 740 bytes and 185 instructions. The source boundary was recovered
from the fresh `game_8009b298.txt` Ghidra listing whose instruction bytes have
SHA-256
`e611ef5c70173153a773cadfb8933306a299c75ba03f1d723a5f824f7f29e9bf`.
A repository ownership search found no earlier complete owner for this range.

The owner receives the complete 32-GPR, HI, and LO state with per-byte
knownness plus validated guest-memory regions. It preserves the source stack
save order and chooses between immediate submission and the 64-entry retained
queue. Immediate submission reads the GPU-status pointer once, polls only the
pointed status word, invokes the live `s3` target, publishes the live
`s3`/`s0`/`s2` values, and restores the prior critical state. Queued submission
copies signed `s1 / 4` words, reloads the live queue head for every destination,
publishes the three queue fields with separate head loads, advances the head
modulo 64, restores the critical state, drains, and returns the modulo-64 queue
depth.

All nine source calls remain typed full-machine dependencies in their exact
source order: start `0x8009BAFC`, wait `0x8009BB30`, drain `0x8009B57C`,
critical-state service `0x800986F8`, live `s3`, critical-state restore,
installer `0x8009863C`, critical-state restore, and drain. Mapped reads at
`0x8009B35C` and `0x8009B390` are ordinary retained-memory reads. The explicit
operation budget bounds the source wait and GPU polling loops while retaining
their completed access and callback prefixes.

The draw-environment adapter intercepts only the actual recovered caller event
at `0x80099B58` with delay slot `0x80099B5C`, return address `0x80099B60`, four
arguments, and entry `0x8009B298`. Other draw-environment children continue
through the caller's typed fallback. The wrapper and direct adapter retain the
same memory and all 34 machine words, and propagate nested result codes and
completed prefixes.

Asset-free focused fixtures cover direct and queued completion, every source
call site and delay, full-queue wait/drain behavior, one-time GPU pointer load
with repeated status polls, all operation budgets for finite multi-wait and
queued executions, all 16 byte-knownness masks through signed truncation,
unknown branch prefixes, ring wrap, signed count edges, callback mutation and
malformed returns, unmapped and unaligned accesses, atomic malformed-byte
failures, immutable unknown stores, aliases, journal truncation, and epilogue
control flow. Natural tests execute both paths through the recovered
draw-environment owner and cover nested failures and invalid returned machine
metadata. An independent local original-instruction differential compared
14,848 cases across all 185 PCs, all 34 machine words, the full 2 MiB memory
image, callback entry states, budgets 0 through 100, queue and device states,
signed sizes, aliases, and live register mutation.

This routine has no direct visual effect. It changes CPU-side scheduling state
and queue records; visible output requires later execution of the submitted
callback and GPU consumption.

Manager natural integration additionally composes the recovered BIOS memory
trampoline after submission and checks its 92-byte environment copy. The native
script runs both direct and queued submissions through the actual scene,
display, draw, packet, both draw-area commands, video query, GPU command and BIOS owners
on shared mapped memory. The queue case copies each 64-byte packet into its ring
entry; direct dispatch reaches the typed packet consumer at 8009B1F8. Scheduler,
critical mode, GPU read status and packet consumption remain explicit synthetic
services. The older GPU-sync closure does not expose this full machine ABI.
No synthetic services or captured packets establish native gameplay.

The independent original-instruction differential passed 14,848 cases covering
all 185 instructions and nine call sites, full 2MB RAM, all 34 machine words,
callback entry state, budgets 0..100, full/empty ring and DMA/GPU busy branches,
signed lengths, ring wrap and mutable S0/S1/S2/S3/SP/HI/LO with copied frames.

Manager verification passed 359 focused checks, 39 natural checks, all 315
asset-free CTests and progress/recovery/instruction/roster freshness checks.
Final-source original differential repeated successfully at 14,848 cases.
Native capture game-entry-20260906-035036-28fba1eb proves two direct submissions
and two queued submissions copying 16 words apiece. The queue head advances
from 1 to 3; direct mode publishes the last packet and raw callback argument.
CPU before/after frames retain SHA-256
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d. The ignored
User Setup screenshot is local evidence; no advancing match is claimed.

Gameplay shown: NO - no direct visual effect.

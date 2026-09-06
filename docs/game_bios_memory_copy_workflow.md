# BIOS memory-copy trampoline recovery

`game_bios_memory_copy.c` owns the complete GAMEONLY routine
`0x8009CB0C..0x8009CB17`: 12 bytes and three instructions. The fresh Ghidra
listing `game_8009cb0c.txt` has instruction SHA-256
`ad7c5bc50bc07966feaaee9043cbb18cd00d1ff4ffbcfbe37fdad2b22ea2876d`.
Known call sites are `0x8008008C`, `0x80099B68`, `0x8009A128`, `0x80080588`,
`0x80099C44`, `0x80099C84`, and `0x8009A16C`.

The trampoline sets T2 to the low BIOS vector `0x000000A0`, executes `JR T2`,
and sets T1 to service `0x2A` in the delay slot. It creates no link and does
not consume or validate A0, A1, A2, or RA. The owner therefore forwards
arbitrary values and byte-knownness for destination, source, byte count, and
return address. It preserves the other 30 GPRs and HI/LO until the typed BIOS
boundary, which may mutate the complete machine and retained memory.

The owner does not translate the BIOS body and does not call libc `memcpy`.
It performs no guest-memory access itself. A typed tail-transfer event exposes
the exact `0x8009CB10` transfer PC, `0x8009CB14` delay PC, vector, service,
three arguments, operation, full machine, and mapped memory. Budget zero,
missing service, and refused service all retain the known T1/T2 prefix.
Accepted malformed callback machines return `ARGUMENT` with their exact
mutation prefix.

The speech adapter recognizes only the recovered `speech_initialize` event at
call PC `0x8008008C`, delay PC `0x80080090`, target `0x8009CB0C`, three
arguments, and RA `0x80080094`. Other speech calls remain behind the existing
typed callback. Speech exposes all 32 GPRs but lacks HI/LO, so the adapter uses
an optional per-invocation HI/LO provider and supplies unknown HI/LO by default.
It never infers hidden ABI state. The `with_speech` wrapper retains nested BIOS
status: malformed GPR output is accepted so the speech owner detects it, while
HI/LO-only malformed output is refused and promoted from the parent's
`IO_REFUSED` result to the nested `ARGUMENT` result.

The focused executable performs 303 always-active checks. It covers the exact
tail event, all 32 GPRs and HI/LO masks, nonzero and unknown incoming T1/T2,
unchanged A0/A1/A2/RA/SP, zero and wrapping arguments, budget zero and one,
missing and refused services, callback machine and memory mutations, accepted
malformed zero/GPR/HI/LO, invalid initial machines and memory layouts, and
deterministic repetition. Its runtime synthetic copy fixture proves that every
changed destination byte comes from the typed BIOS callback rather than the
recovered owner.

The natural executable performs 6,345 always-active checks through the actual
recovered speech owner. It covers exact speech metadata and RA, explicit and
default HI/LO, provider refusal, nested budget prefixes, all malformed output
classes, fallback routing, and continuation after a callback-mutated machine.
One non-null speech record drives the synthetic BIOS copy; later speech work
observes the copied payload and completes. Both executables compile under
strict C99/C++17 with `-Wall -Wextra -Werror -pedantic-errors` and use only
runtime-generated data.

The sole production dependency is the typed BIOS service callback. Natural
integration additionally depends on the recovered speech-initialize event and
its full-GPR machine. The BIOS memcpy implementation, BA display caller, and
other six known caller paths remain outside BC.

Gameplay shown: **NO - no direct visual effect**. BC exposes a CPU/memory
transfer boundary; it does not submit graphics or render pixels. The test
evidence is the typed event, machine prefix, and callback-authored memory
change.

Manager independent differential passed 8,192 cases across all three original
instructions, all 34 CPU words and masks, budgets zero/one, unknown forwarded
arguments/RA, and typed BIOS mutation/refusal/malformed-HI prefixes. Native
capture composes the same actual speech caller and trampoline with explicitly
synthetic resource/BIOS services and logs changed destination bytes, T1/T2,
callback return state and matching CPU-only frame hashes.

Manager verification passed 303 focused checks, 6,345 natural speech checks and
all 299 asset-free CTests, plus strict C99 and all progress/recovery/instruction/
roster freshness checks. Native run
`.local/verification/team_select/game-entry-20260906-023244-671ec98c`
recorded 98 scripted states and identical before/after SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The frontend remains User Setup; no advancing match was displayed.

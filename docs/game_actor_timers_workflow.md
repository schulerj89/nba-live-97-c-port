# GAMEONLY actor timers and participation counters

`nba97_game_actor_timers` owns GAMEONLY `0x8006830C..0x80068503`
(504 bytes, 126 instructions). The source boundary comes from the fresh Ghidra
listing `game_8006830c.txt`, whose instruction-byte SHA-256 is
`1b2b4229ce1bbb9b927722a1dac657b3cd98cad3a7fe53a0cab1042021344db2`.
The routine was previously unowned: repository references before this recovery
were inventory rows and the unresolved call from match tick at `0x80068E38`.

The owner decrements three signed halfword timers across eleven live actor
pointers. The first ten actors also have two fields cleared, while the eleventh
only receives the `+0xB4` timer path. Each nonzero timer stores its wrapped low
half before the sign test and then stores zero when that half is negative. A
nonzero `+0xE4` timer sets `+0xDD` even when subtraction immediately clamps it.

The second phase reproduces the source's two signed magic-number multiply
pipelines. It compares the match clock's quotient by 60 with the cached signed
halfword at `0x800FDB74`; a changed, nonzero clock updates the cache and ten
team counters. The final loop rereads the clock for every actor with a
nonnegative controller index and compares its quotient by 3600 with the
controller cache. These names describe the evidenced divisors without making
an unsupported time-unit claim. Both `MULT` instructions publish exact HI/LO
state and per-byte knownness rather than using a C division shortcut.

The retained-memory interface preserves source access order and native backing
aliases. Team pointers are loaded once for `+0x1A`, then reread after that store
before `+0x1C`. Controller pointers and the clock are also reread at their
source sites. Guest addresses remain 32-bit values with alignment, mapping,
wraparound, malformed-knownness, and unknown-store checks. Operation budgets
stop before each attempted mapped access and retain the completed prefix.

`nba97_game_actor_timers_from_match_tick` binds only the actual match-tick call
at `0x80068E38` to entry `0x8006830C`, with zero arguments and JAL-produced
`ra=0x80068E40`. Match tick's legacy service ABI has no CPU register file, so
the binding requires an independently supplied full-machine snapshot. Missing
context is refused; the adapter does not infer registers or fabricate HI/LO.
The natural test reaches this call through the actual recovered match-tick
owner using an exact synthetic prerequisite whitelist, then refuses the next
`0x80068E78` service to prove source ordering.

Asset-free focused coverage includes all eleven and ten-entry boundaries,
timer zero/negative/wrap paths, store-before-clamp order, counter wrap,
signed-clock extrema, exact multiply traces, partial knownness, live pointer
and clock aliases, every mapped-access budget prefix, malformed loads,
unknown stores, mapped wraparound, region validation, all return-address masks,
the final JR alignment trap after SP restoration, untouched registers, and
deterministic repeatability. The manager's independent original-instruction
differential passed 12,080 cases across all 126 PCs, full RAM, all 34 machine
words and masks, and failure prefixes. Its receipt is
`.local/evidence/tipoff-recovery/actor_timers_differential.json`.

The native diagnostic now composes this owner immediately after the actual
crossing rule, projecting only the next source JAL's return address into that
owner's full machine. Both owners use the same retained memory. The receipt
proves 240 accesses (156 reads, 84 stores), eleven actor visits, ten team
counter updates, and one participation update for ten duplicate controller
references. Timers [5,1,2] become [4,0,1], cache 59 becomes 60, and participation
5 becomes 6. Comparison frames have identical SHA-256
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
Focused tests pass 928 checks, natural tests pass 34, and the complete suite
passes 367 tests. The visible frontend remains User Setup.

Visual classification: **Gameplay shown: NO - no direct visual effect**. This
CPU-only routine changes retained timers and participation counters. The
synthetic natural caller test is state evidence and does not claim an advancing
rendered match; the diagnostic stops at the next typed recorder service.

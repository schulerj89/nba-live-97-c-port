# Game actor contact coordinate gate recovery

`nba97_game_actor_contact_gate` owns the complete GAMEONLY routine at
`0x8005FAA8..0x8005FAE7` (64 bytes, 16 instructions). It was translated from
the fresh Ghidra listing in `game_8005faa8.txt`, whose instruction bytes have
SHA-256
`45299ab26bd749d0d547a673aaffb5dddf5af971fdab2d446a7a7be20c13bf82`.

The owner saves `ra`, reads the second actor's coordinate word at offset 8,
then reads the first actor's coordinate word at offset 8. It subtracts with
32-bit wrap and performs the source signed comparison against 4097. This is a
one-sided gate: every negative raw difference is accepted, including
`INT32_MIN`; there is no absolute-distance or lower-bound test. The branch
delay always clears `v0`. On acceptance, JAL first publishes
`ra=0x8005FAD4`, then its delay slot arithmetic-shifts `a2` by eight before the
typed `0x8005F948` child observes the three arguments. Any completed child
return is replaced by one. Rejection returns zero without a child call.

The machine interface retains all 32 GPRs, HI/LO, per-byte knownness, guest
memory, exact access order, and bounded failure prefixes. A child can change
the full live machine, `sp`, or the saved stack word. The epilogue reloads `ra`
through that live `sp`, advances it by 0x18 with 32-bit wrap, and consumes the
restored return address after `jr`'s NOP.

The adapter accepts only AJ's actual `0x8006104C` call event, including its
`0x80061050` delay slot and JAL return address `0x80061054`. The natural
composition test runs the complete AJ dispatcher over eleven generated sorted
references. Forty-five actor pairs pass through this owner; child returns
0, 1, 2, and `0xFFFFFFFF` are all normalized to one so AJ continues exactly as
the source wrapper directs. Rejection, nested operation-limit, and child
refusal prefixes are also exercised. The sole `0x8005F948` callee remains an
explicit typed dependency with no invented behavior.

This CPU coordinate gate has no direct visual effect. Its return value proves
only skip or completed child dispatch, not collision, possession, or gameplay;
pixel-identical frames are expected until downstream contact state reaches the
rendered match path.

Validation: 146 focused checks, 10 natural-caller checks, all 265 asset-free
CTest cases, strict C99 compilation, progress and metadata freshness checks
pass. A private original-instruction comparison covers 8,000 cases and every
one of the 16 source instructions, including all operation cutoffs, coordinate
and saved-return aliases, and callback frame relocation.

The native input-driven verifier records 45 actual sorted-dispatch calls. The
last gate reads difference 256, forwards shifted difference 1, and replaces the
typed eligibility child's zero with one; its five operations are three reads,
one store and one callback. Frame SP is `0x801FF000`, returned SP `0x801FF018`,
and restored RA `0x80061054`. Before/after CPU frames both hash to
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The actual frontend remains User Setup; the fixture is not a live match.

Gameplay shown: NO - no direct visual effect.

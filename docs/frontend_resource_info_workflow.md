# FEONLY frontend resource info/open recovery

`nba97_frontend_resource_info` owns the complete FEONLY subroutine at
`0x8008A594..0x8008A6EB`: 344 bytes and 86 instructions with source SHA-256
`494529aeb56f769fbc5f40e3792f83492ad9368f40e6672ce2f4359a6d0a887a`.
Fresh evidence is the ignored `feonly_8008a594_continue.txt` export from
`FEONLY.BIN` loaded at `0x80015000`. The owner records every executed source PC,
retained-memory word access, callback attempt, complete callback machine, and
failure prefix. All guest addresses remain 32-bit values over validated
little-endian regions with an optional byte-knownness plane.

The first child compares the frontend prefix state. A zero result selects the
six-argument `0x80074184` route with `a0..a3`, frame+304 at outgoing stack
offset 16, and the caller's fifth argument at offset 20. A nonzero result enters
the direct route. Each attempt clears the three output words, formats frame+24,
opens it, stores the open return in the caller's handle word, and conditionally
queries and seeks the positive handle. A nonzero query result plus zero seek
result ends successfully and publishes the query result through the size
pointer. Other outcomes decrement mutable `s2`, optionally close a positive
handle, clear the handle and `s1`, and retry while `s2` is nonzero.

The translation preserves source ordering and quirks. The first JAL delay saves
`s3` after the source has installed its return address. The prefix branch delay
always forms frame+24. The delegated child receives both outgoing stack words
before invocation. The open branch delay always stores V0, and the close test's
delay always decrements `s2`, including for zero and negative handles. An open
failure does not initialize `s1` or `s3`; full-machine callback changes or stale
values can therefore affect the later success test. Every later frame and
output access uses callback-live `sp` and saved registers. The final nine loads
restore `ra/s7..s0` from that live frame before adding 352 to `sp`.

All seven original children remain typed full-machine dependencies:
`0x80084910` (three arguments), `0x80074184` (six), `0x80083B70` (four),
`0x8007F588` (two), `0x8008A408` (one), `0x8007F318` (three), and
`0x8008A7B0` (one). A callback sees the target program, call PC, delay PC,
argument count, per-site invocation, retained memory, all 32 GPR words and
masks, and HI/LO. It may mutate that entire state. Refusal, malformed returned
state, unknown decisions, alignment traps, unmapped accesses, and exhausted
operation budgets preserve the exact completed prefix. The operation budget is
the explicit host bound for a callback that keeps `s2` nonzero indefinitely.
These callback contracts do not claim the children’s complete CPU ABIs.

`nba97_frontend_resource_info_from_frontend_resource_load` binds the recovered
natural caller at `0x8007B214`, with delay `0x8007B218`, return address
`0x8007B21C`, FEONLY target `0x8008A594`, and five arguments. It validates the
complete parent boundary machine and leaves the fifth stack argument in
retained memory for the callee's source load at frame+368. The composition
wrapper routes only this site through the new owner; DF's lookup and later
allocation, postload, close, and dynamic callback sites remain explicit
fixtures. The already recovered lookup owner is not bound in this particular composition.

The standalone capture returns JSON without writing files. Its explicit
synthetic contract returns prefix V0=1, open V0=`0x44`, info V0=`0x1200`, and
seek V0=0, all fully known. Formatter and all otherwise unspecified children
preserve the complete machine and retained RAM. The receipt contains the exact
67-PC direct-success trace, 25 access records, five callback boundary machines,
all operation counts, the final 34-word machine, and the unresolved dependency
list. It contains no retail data, binary fixture, raster, or claimed child ABI.

The focused asset-free suite covers both routes, the union of all 86 source
PCs, exact call contracts and direct-success access records, ten exhausted
attempts, success after retries, close and no-close paths, callback-live
mutations, stale-register success, saved-frame aliases, callback-live frame
relocation, 32-bit stack wrap, all seven refusals, every operation-budget
prefix on both major routes, zero/nonzero and signed partial knownness, absent
and malformed knownness planes, region overlap, unmapped and misaligned
accesses, and unknown or misaligned restored return addresses. The integration
suite executes the existing DF owner with the new owner at its natural call
site, verifies the real fifth stack argument and shared retained RAM, repeats
the composition without a knownness plane, checks nested failures and exact
adapter guards, and validates the printable capture contract. Local strict
build outputs belong only under the ignored
`.local/build/frontend_resource_info_worker` directory.

Visual classification: no direct visual effect. Gameplay shown: BLOCKED. The
new routine still depends on seven unresolved FEONLY services, and the native
application does not yet provide the real frontend lifecycle, loader handoff,
or advancing match loop. A manager-owned native input verifier must register
the production path and capture identical before/after frame hashes; a
standalone CPU fixture cannot establish gameplay.

Manager final validation: MSVC Debug focused 418 checks, natural integration
102 checks (including complete parent return CPU and knownness), and all 421
asset-free CTest tests passed (34.97 seconds). All required metadata freshness
checks passed. The independent raw-word oracle passed 3,106 cases across all
86 PCs and seven child sites, comparing full retained RAM/knownness, all 34 CPU
words/masks, and exact instruction/access/callback journals against core SHA
`1aa3f9f1d740a9f2bb5ec3fe5428682fc0c799f38d3a1811a027133ed9d02d20`.
Native input verification passed with 134 frames in `.local/verification/team_select/game-entry-20260906-154434-4e4705d4`.
The native verifier independently reconstructs 67 executed PCs, all 25 word
accesses, five complete callback machines and the complete final machine.
Both frames retain pixel SHA-256
`42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7`;
visual inspection confirms User Setup. Prefix/file/seek services remain unbound.

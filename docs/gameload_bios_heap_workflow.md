# GAMELOAD BIOS heap trampoline recovery

The recovered owner `nba97_gameload_bios_heap` covers the complete GAMELOAD
range `0x801E1590..0x801E159B`: 12 bytes and three instructions. Fresh Ghidra
evidence in the ignored local recovery corpus identifies one caller at
`0x801E1498` and records instruction SHA-256
`4487ee3019aae533a71d191483e6876aa40c2530923670ec0e012a78204fb863`.
Repository searches found no prior owner. The same-address FELOAD symbol is a
different overlay program, and the GAMEONLY BIOS memory-copy trampoline is only
an architectural precedent at a different address.

The source loads `t2=0x000000A0`, executes `jr t2`, then writes `t1=0x39` in
the delay slot. The jump does not link, so the live `ra` is unchanged. The
callback receives the post-delay 32 GPRs, HI, LO, optional per-byte knownness,
and the same retained-memory mapping. It owns every BIOS InitHeap return-state
and memory effect. The recovered owner does not inspect `a0`, `a1`, or `ra`,
does not touch RAM itself, does not implement heap allocation, and does not
invent o32-preserved register values.

One host operation bounds the unresolved BIOS service. All three source PCs
are executed and journaled before a zero budget or callback refusal is
reported, preserving the delay-slot prefix. A missing callback, a callback
result other than one, or an absent operation returns the corresponding
bounded status with the post-delay machine visible. A callback that returns
one owns the full resulting machine; invalid knownness masks or a changed zero
register are rejected rather than normalized.

`nba97_gameload_bios_heap_from_gameload_entry` is the narrow natural adapter
for the committed GAMELOAD entry owner. It accepts only the exact first-child
metadata: call `0x801E1498`, delay slot `0x801E149C`, target `0x801E1590`,
operation 2079, invocation one, two arguments, GAMELOAD program identity, and
known `ra=0x801E14A0`. Those checks describe the proven caller boundary; they
do not add argument-knownness requirements to the trampoline itself.

`nba97_gameload_entry_with_recovered_bios_heap` intercepts that child and
forwards the later `0x801E14AC -> 0x801E136C` GAMELOAD-main child to the
caller's explicit callback. A synthetic returned InitHeap fixture allows the
committed caller to continue. If GAMELOAD main returns, the caller executes
its original `BREAK 1`; only an explicit transferred outcome avoids that trap.
The raw static stack-top fixture `0x00800000` consequently produces
`sp=0x807FFFF8`, with no stack access made by this trampoline.

The direct asset-free test covers the exact PC/event journal, operation limits,
callback refusal, full 34-word CPU and knownness mutation, callback RAM
mutation, no-callback/no-direct-RAM behavior, all masks for partially unknown
`a0`, `a1`, and `ra`, malformed machine and API guards, journal prefixes,
optional knownness planes, memory-region bounds, and repeatability. The natural
integration test composes the actual GAMELOAD entry, checks the raw stack
result, both knownness-plane modes, failure propagation, full-machine and RAM
mutation reaching the second child, returned-main `BREAK 1`, explicit transfer,
and every parent-contract guard.

The deterministic capture emits all three PCs, the complete callback and final
34-word machine states, and an explicit fixture contract. This CPU trampoline
has no direct visual effect and creates no synthetic raster. Native input
verification should therefore assert matching before/after frame hashes while
checking the state log.

Gameplay shown: BLOCKED. The BIOS vector A0 service 0x39 host full-state service
is intentionally unbound. After that service returns, the separate GAMELOAD
boundary `0x801E14AC -> 0x801E136C` has a recovered main owner; its
startup, loader and GAMEONLY services still need production wiring. The native
capture refuses that continuation and reports the exact parent stop; its
returned BIOS fixture is not a claim of a working heap service.

Final manager validation: MSVC Debug focused 313 checks and natural integration
44 checks passed; all 419 asset-free CTest tests passed (35.84 seconds).
Progress, recovery, instruction-semantics and roster freshness checks passed.
The private raw-instruction differential passed 40,960 cases against owner SHA
`e2ece1361abdacbf6ca60c95078b123115852713d682d7a6155d5abfcf99b534`,
comparing complete CPU state, byte knownness, retained RAM and exact journals.
Native input verification passed with 132 frames in ignored run
`.local/verification/team_select/game-entry-20260906-153202-910f9ca5`.
Both trampoline frames have pixel SHA-256
`42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7`;
the verifier independently reconstructs the full BIOS boundary and stopped
parent machine plus retained-memory checksum. The unchanged image is User Setup.

# FEONLY frontend memory copy recovery

This recovery owns the complete FEONLY subroutine at `0x800909A8..0x80090CC7`
(800 bytes, 200 instructions). Fresh read-only evidence is recorded by SHA-256
`589207dc7895ba0151f714f53c02c357959170daed411e652ca281ac7216ef4b`.
The similarly shaped GAMEONLY routine at `0x800AA468` remains a distinct owner;
the FEONLY implementation carries its complete live machine rather than using
that owner's narrower memory-only contract.

The routine accepts source in `a0`, destination in `a1`, and byte length in
`a2`. Signed address comparisons select a forward path unless destination lies
inside a signed-address source span. The initial `or v0,a0,a1` is the branch
delay and therefore executes even when the backward path is selected. A
non-overlapping `source < destination` route branches back to that same OR and
executes it twice. The returned `v0` is the final source/destination alignment
bits, not a destination pointer.

Aligned forward groups snapshot eight words before each half-group is written,
twice for every 64 bytes, then use 16-byte, four-byte, and byte tails. Unaligned
groups retain every little-endian LWL/LWR merge and SWL/SWR store separately.
Backward aligned groups copy four words at a time, but their final four-byte
tail deliberately uses LWL/LWR and SWL/SWR even when both endpoints are
aligned. Access journals record the source PC, selected guest byte address,
logical word address, full register value and known mask, selected byte-lane
mask, width, direction, and operation index.

The owner returns all 32 GPR words and byte-known masks plus HI/LO. It preserves
the original path-dependent values of `at`, `v0`, `a0..a3`, and `t0..t7`,
including partial register merges when a later access fails. The signed ADDs at
`0x80090B9C`, `0x80090BAC`, and `0x80090BB0` retain trapping semantics. Negative
or wrapping lengths are not repaired; the access budget exposes the exact
completed prefix and returns the bounded condition. The three `jr ra` exits run
their NOP delay slots before unknown or misaligned return addresses are
reported.

`nba97_frontend_main_with_recovered_memory_copy` composes this owner at the
already recovered frontend main's natural JAL at `0x80028B54`; the delay slot
moves `s2` into `a2`, RA must be the fully known `0x80028B5C`, and the call has
three arguments. All other frontend-main services remain typed callbacks. The
deterministic fixture supplies a standalone synthetic main machine and 2 MiB
retained memory. Startup writes `0x80015098=1`; two allocation services return
`v0=0x80130000`; the clock service returns `v0=0`; the loader and size services
return `v0=0x80140000` and `v0=4096`; every other FEONLY service preserves the
full CPU and guest RAM. The generated 4,096-byte payload is copied to
`0x801E0000`, and its first word becomes dynamic GAMELOAD target `0x801E1410`.
That target is intentionally unbound and refused at main PC `0x80028B68`; no
GAMELOAD code, native match lifecycle, or advancing gameplay is manufactured.

The direct asset-free test covers lengths 0 through 130, all source and
destination alignments, separated and equal aliases, both overlap directions,
signed-address boundaries, arithmetic traps, negative-length bounded prefixes,
all actual partial-word lane masks, unknown-byte propagation, destinations
without a knownness plane, malformed regions and bytes, partial merge failure
prefixes, partial-known branch and arithmetic decisions, all 200 source PCs,
all three return sites, late RA faults, full machine output, and deterministic
reuse. The natural-main integration test checks the exact call event and
callback-live machine, the complete 4,096-byte copy, no-plane operation, owner
failure visibility, the unbound GAMELOAD stop, and deterministic receipt hashes.
The receipt publishes numeric main result/stopped PC/stopped target and copy
result/completion, both complete 34-word input/output machines, exact access and
PC counts, and canonical FNV-1a-64 fingerprints. The declared seed is
`0xcbf29ce484222325`. Machine hashes serialize 170 bytes: `gpr0..gpr31`, HI,
and LO, each as a little-endian 32-bit word followed by its one-byte known mask.
PC hashes serialize each executed PC as little-endian 32-bit. Access hashes
serialize each event as four little-endian 32-bit fields (`pc`, `address`,
`logical_address`, `value`), one little-endian 64-bit operation index, then the
one-byte width, known mask, transfer mask, and kind. No host structure padding
or `sizeof` participates in these fingerprints.

Visual classification: `Gameplay shown: BLOCKED`. The copy has no direct visual
effect. Native frame evidence is produced by the manager's existing verifier,
outside this CPU receipt. Actual gameplay still requires an owned GAMELOAD
dynamic entry and an advancing native match loop reached through that lifecycle.

Manager validation after exclusive review freeze: final MSVC Debug focused 738,605 checks and natural integration 36,828 checks passed. All 409 asset-free CTest tests passed in 31.78 seconds. The private original comparison passed 14,814 cases across all 200 PCs and all three return sites, checking all 34 CPU words/masks, retained memory and knownness, exact partial-lane access and PC journals, signed traps, overlap and failure prefixes. Progress, recovery, instruction-semantics, and roster freshness checks passed.

The native input verifier passed 122 captured frames in ignored run `.local/verification/team_select/game-entry-20260906-143108-3597e265`. Its `frontend_memory_copy_verified.json` independently reconstructs the generated payload, all 2,048 memory events and 2,329 executed PCs for canonical fingerprint comparison, checks full boundary and returned machines, and verifies the refused GAMELOAD target. Native before/after PPM pixel SHA-256 both equal `42378915a6f4b3706f54ada89180d18f2a570fe937baabf1f702191a0fc825d7`.

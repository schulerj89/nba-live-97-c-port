# Court-frame composition adapter

`GameCourtFrameCompose` is the C++ join between the retained original-address
memory used by `GamePlayerFrame` and the already recovered C owner for GAME
`4AC68`. It does not recover another copy of `4AC68`; all court traversal,
packet construction, original rereads, links, and preserved quirks remain in
`nba97_game_court_frame` in `src/recovered/game_court_packets.c`.

The adapter borrows a checked `Nba97PlayerFrameContext`. It owns no allocations,
resources, camera defaults, platform services, or source-address mappings. The
enclosing frame must keep the context, descriptors, buffers, cells, and address
metadata alive for the synchronous call. Refusal publishes the reached memory
and geometry prefix, as the direct C owner does; callers needing atomicity must
clone the entire retained owner before dispatch.

## Memory and pointer contract

Court widths 1, 2, and 4 map directly to frame reads/writes. Width 3 is the
aligned low 24 bits of a packet tag. It stays a scalar `NBA97_FRAME_WRITE`, so
the untouched high byte and its knownness survive. The five full-word cursor
publications at `4ACDC`, `4ACE4`, `4ADE4`, `4AF0C`, and `4B0EC` use
`NBA97_FRAME_WRITE_POINTER`; this lets the retained frame memory preserve
allocation identity when the numeric source address resolves to a known
allocation. Packet tags and ordering-table links are not pointer publications,
even when their low 24 bits encode a link.

The court API has all-or-nothing known values while frame memory tracks each
byte. A fully known frame read becomes a known court value. Any partial byte
knownness becomes canonical unknown `{0,0}` and the direct owner refuses at
that exact access. Normalized references are accepted only as aligned full-word
reads: an unknown reference must have zero bytes/word, and a known reference
must carry all four known bytes. Malformed metadata refuses as an argument; no
address, zero, padding, or reference identity is invented.

The raw C owner performs its alignment check before invoking memory. Its
`NBA97_COURT_ALIGNMENT` result maps to
`NBA97_PROJECTION_UNSUPPORTED_ALIGNMENT`. Its native operation bound maps from
`NBA97_COURT_LIMIT` to `NBA97_BODY_JOURNAL_LIMIT`. A non-court status returned
by borrowed frame memory is carried through a private bridge slot and returned
unchanged, avoiding numeric collisions between the court and body result
enums. Court unknown and argument results map explicitly to their body
counterparts.

## Shared retained geometry

The adapter translates each court math request into the existing retained
`GameNetGeometry`/player projection operations. Rotation, translation, vertex
loads, RTPT/RTPS, NCLIP, screen FIFO reads, AVSZ4, and OTZ therefore mutate the
same camera, FIFO, MAC, IR, and FLAG values used by the preceding native frame
passes. `ZSF4` remains separate from `ZSF3`.

The original court helpers test GTE **data register 31 (LZCR)**, not control
register 31 (FLAG). `leading_bits` is therefore a distinct retained input. It is
never derived from projection flags, and unknown or values above 32 refuse.
Loading the rotation and translation matrix does not make unknown OFX, OFY, H,
DQA, DQB, or ZSF4 controls known. These original oddities are integration
requirements, not candidates for repair.

## Verification scope

The public test runs the actual adapter and the existing direct `4AC68` owner on
cloned retained memory. Its 845 checks per configuration compare ordered
reads/writes, every retained byte and known byte, progress, packet/link totals,
and the touched geometry state. They also cover all five normalized cursor
publications, scalar low-24 tag stores, reference rereads, partial-known refusal,
unknown controls, independent LZCR, intrinsic alignment, budget exhaustion,
malformed reference metadata, and exact translation of every nontrivial
borrowed body-memory status. Strict MSVC Debug/Release and GCC Debug/Release
pass; GCC Debug also runs undefined-behavior sanitization.

Private Debug and Release verification each compare 555 actual-adapter cases
against execution of the original GAME `4AC68` instruction words: 127,351
ordered retained-memory events, 379 unknown-input refusals, all retained
bytes/knownness, all 64 retained geometry words, exact refusal status/PC/address,
and all 507 owned source instruction addresses agree.

This proof covers only the frame-context adapter and the already recovered court
pass. It is not evidence of natural match entry, a visible gameplay frame, a
first possession, or a completed match.

# Owned animation resources and runtime composition

`GameplayAnimation` retains one immutable `GameplaySetup` generation and the
original remapping data used by5703C. Its C views borrow from that owner;
copying or moving the object itself is disabled so an internal pointer cannot
outlive the vector it describes. Shared resource handles retain the generation.

The private `animation_maps.bin` format is `NBA97ANI`, version1, payload length
`0x30084`, CRC32, then GAME bytes from800A850C through800D858F. Seven views expose
65,536 halfwords each. B8564/B8590 begin at unsigned index0; the other five begin
at signed index-32768. The ordinary tables have22 entries, but the source has
no22-entry guard. These larger overlapping windows retain every possible raw
halfword-indexed adjacent read without publishing original data in the repo.
The owned bytes are data and are never executed by the native port.

Each of168 clip views contains normalized flags, count and timing plus source
mode byte2. Timing must come from `Nba97GameMocapHeader::timing`:640D8 halves
byte3 on the first flag8 normalization. An independent review caught a native
adapter defect that read the raw timing byte; this was corrected before the
checkpoint. The synthetic timing regression checks raw173 becoming86.

`advanceMatchRuntimeAnimation` maps the current entity and knownness to the
complete579FC owner. It requires an explicit simulation tick6C and the same
immutable setup generation as the match. Only written animation, extra-halfword
and queue-byte effects are applied. `queueMatchRuntimeAnimation` similarly
composes56CE0 or its single-channel owners. Both stage only one244-byte record,
not the accepted rosters on every player tick. A native refusal or source
nontermination leaves the live entity unchanged.

Neither adapter invents actor-state transitions, height, input, physics or an
entire6801C tick. Original full-queue drops, channel divergence, stale auxiliary
bytes and zero-step nontermination remain as documented by the C owners.

`tools/extract_gameplay_motion_assets.py` verifies the supported disc/overlay
hashes and extracts this window, ZHOTS and the foot trig table. It only writes
under `.local`, rejects source/output aliases and conflicting existing files,
and preserves identical existing bytes and timestamps. It adds new artifacts
without replacing the previous immutable period/setup manifest. The normal
asset extraction script invokes it. Private extraction checks verify repeated
extraction, conflict refusal, alias refusal and refusal to publish originals.

Public tests cover both index-range endpoints, signed/unsigned map selection,
normalized timing, null clips, borrowed-view lifetime, malformed packs and
generation mismatch. Match tests cover actual frame advance, exact-width queue
stores, stale aux bytes, silently discarded full queues, independent channel
locks and unresolved simulation ticks. Original core proofs remain in
`game_animation_advance_workflow.md` and `game_animation_queue_workflow.md`.

An independent composed comparison in private `checkpoint7/animation-review-*.json`
verifies all458,752 map words and168 normalized header/timing views.
Each build runs54 composed cases,4,158 queue/advance calls and19,470,024 known
record-byte comparisons against the original. Original65DB0 and its reached
callees establish each explicit fixture, followed by the original queue and
full579FC owners without semantic callee hooks. This is source composition
under supplied state, not a natural match-entry or input/physics claim.

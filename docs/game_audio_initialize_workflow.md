# GAMEONLY match audio initializer recovery

`src/recovered/game_audio_initialize.c` owns all 59 instructions of GAMEONLY
`0x80029114..0x800291FF` (236 bytes). The private read-only Ghidra listing
`game_80029114.txt` records routine SHA-256
`71cd42b84299dd4d1b730301841a8897bf119088269cfcdf9b1929b928cb249f`.
It identifies the natural match initializer call at `0x8002DBD0` and another
call at `0x8002D2A0`; the function inventory places the latter inside
`FUN_8002CD84` (`0x8002CD84..0x8002D2DB`). No complete owner previously
claimed this range.

The recovered owner performs the exact source sequence:

1. Read the old bank header at `0x8001502C`, form the 0x18-byte frame, save
   `ra`, and save `s0` in the conditional branch delay slot.
2. Release a nonzero old header through `0x80090698`.
3. Load `zaudiofx.vh` and `zaudiofx.vb` through the retry wrapper
   `0x80029BFC`, publishing the first raw result back to `0x8001502C`.
4. Invoke `0x8008F4F0(-1,0)`, capture the second load result in its JAL delay
   slot, then call `0x800ADB48`, `0x8008CDC0(4,11000,0,0)`, and `0x8008CC28`.
5. Reload the header live, upload `(0x80021D6C, header, s0)` through
   `0x800AD360`, release `s0`, and apply master volume 127.
6. Read the unsigned setting byte at `0x80021D7C`, multiply it by 15, clamp
   values of 128 or greater to 127, call `0x80088E84(volume,-1)`, and store
   its raw `v0` at `0x80021EE0`.
7. Reload `ra` then `s0` through the callback-mutated live `sp`, restore that
   `sp`, and return with the raw volume-service `v0` still live.

The owner represents all PS1 addresses as 32-bit guest integers and routes
every byte through validated retained-memory regions. Reads and stores retain
per-byte knownness. Word alignment, 32-bit stack-address wrap, native aliases,
callback mutations, refusal prefixes, malformed register results, and every
operation-budget prefix remain observable through typed progress and journal
records. A native bound reports an incomplete source prefix; it does not make
that prefix resumable or successful.

`src/game_audio_initialize_adapter.cpp` composes the existing complete
`0x80029BFC` resource-loader owner when its narrower input API can represent
the live state. It maps child results and source-proven epilogue registers back
into the full parent register file. Scratch outputs that API does not expose
become unknown. If input knownness does not fit the child API, the original
typed callback remains in control. Although `0x80090698` also has a complete
CPU owner, its public API excludes active-stack aliases and does not expose
stack or output GPRs. This full-register caller therefore keeps both releases
at the typed boundary instead of relying on ABI assumptions. The adapter makes
no host-audio, file, heap, or audible-output claim; all other audio calls remain
explicit service boundaries.

The asset-free focused test generates all state at runtime. It checks both
old-header branches, exact call PCs and delay slots, all 256 setting bytes,
the 8/9 clamp boundary, raw partial-known returns, live header and `s0`/`ra`/
`sp` mutations, access order, native aliases, alignment, address wrap,
unknown and malformed data, callback refusal, and every operation-budget
prefix on both branch paths. The integration test runs the existing retry
loader owner against a synthetic service and invokes this owner naturally from the
recovered match initializer's `0x8002DBD0` event.

Gameplay shown: NO - no direct visual effect. This routine changes retained
CPU audio state and crosses typed audio-service boundaries. It does not draw a
frame, and the synthetic service tests do not establish audible output.

The native game-entry driver invokes the same audio adapter from the recovered
match initializer. Both recovered loaders retry one generated null response,
then publish explicit synthetic handles. A typed service mutates the retained
header before upload; the verifier requires that new live header, body handle,
volume 9 clamped to 127, and raw `0xFEEDBEEF` publication. It records unchanged
scanout hashes in `audio_initialize_verified.json`. Logs and native frames are
ignored local evidence, with no audible or gameplay claim.

Validation passes 4,769 focused checks, 95 integration checks and all 201
asset-free Debug CTests. An ignored independent instruction harness executes
all 59 original instructions across 672 cases and compares all retained bytes,
all 32 final GPRs, all child-entry GPRs and operation-budget prefixes over both
old-header branches and all volume bytes. This review also corrected the
pre-store body-name register prefix and unknown-volume branch state before
acceptance. Progress and recovery metadata freshness checks pass.

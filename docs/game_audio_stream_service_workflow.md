# GAMEONLY audio stream service recovery

`nba97_game_audio_stream_service` owns GAMEONLY
`0x80086190..0x800861E3` (84 bytes, 21 instructions). Fresh Ghidra evidence in
the private recovery workspace has SHA-256
`b83c6d9aff01ad310de9d79ab81294ebfb885942103169424a50f7fa76da2b80`.
The source has thirteen known callers; the recovered audio stream pump reaches
it naturally at `0x80083F78` and `0x80084034`.

The owner creates the 24-byte stack frame, saves `ra` and `s8`, reads the live
header pointer at `0x8010473C`, and reads the state word at the unchecked
wrapping address `header+0x24`. It executes `ORI v0,zero,1` before deciding the
branch. Exact state 1 skips the child and returns 1. Every state proven unequal
calls the sole typed dependency, `0x800861E4`, at `0x800861C4` and returns its
raw `v0`. Partial data that can be either state 1 or another value stops at the
branch with `v0=1` already visible.

All guest addresses stay 32-bit and pass through mapped retained-memory
regions. Reads and stores preserve little-endian per-byte knownness, access
order, alignment failures, 32-bit wrapping, and completed prefixes. The child
receives all 32 live GPRs after JAL publishes `ra=0x800861CC`; it receives no
source arguments and may mutate every live register and retained byte. The
epilogue deliberately uses child-live `s8` as its frame even if child `sp`
differs, then reloads `ra` and `s8` in source order. The adjacent worker at
`0x800861E4` is not translated here.

The production adapter binds both real audio stream pump call sites to this
owner and can simultaneously bind the already recovered `0x8008472C` status
leaf. All remaining pump services and `0x800861E4` stay explicit typed
providers. Focused fixtures are generated at runtime and cover state branches,
partial knownness, aliases, pointer wrapping, mapping and alignment failures,
mutable GPRs and frame state, callback refusal, and every operation-budget
prefix. Integration fixtures execute both natural V call sites through the same
adapter with X supplying the real gate result.

Visual classification: `Gameplay shown: NO - no direct visual effect`. This is
a CPU audio-service wrapper with a typed adjacent worker. The tests make no
audible-playback or advancing-gameplay claim.

Manager validation: 159 focused checks, 18 natural integration checks, strict
C99 compilation and all 241 asset-free CTests passed. Private instruction-byte
comparison passed 2,250 cases covering all 21 instructions, full memory/GPRs,
child-entry state, aliases and bounded prefixes. The separate worker build's
roster-save failure did not reproduce in the integrated manager suite.

Native verification executes ten calls through five recovered stream pumps:
each first call reads synthetic header state 0, calls the explicit adjacent
worker fixture once, and returns its raw 0x13572468. That fixture sets the header
state to 1; the second wrapper invocation skips the child and returns 1.
The fixture effect is not attributed to an implementation of 0x800861E4.
Both real parent call sites and child-mutable frame return are asserted.
Natively captured diagnostic frames remain identical at SHA-256
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d.
Separate ignored User Setup frames prove only menu display. No advancing match
or audible streaming is claimed.

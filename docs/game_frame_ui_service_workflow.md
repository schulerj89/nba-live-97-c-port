# Frame UI service recovery

This module owns GAMEONLY `0x80032B10..0x80032BB7`, 168 bytes and 42
instructions. The fresh Ghidra listing has SHA-256
`59cb86799b2fb1c4a46167abe168c76499666269eb593de50ac21205b40506a9`.
Its sole recorded caller is the frame pump at `0x8002DDAC`; the source call has
no arguments, a NOP at `0x8002DDB0`, and return address `0x8002DDB4`.

The routine creates a 0x18-byte stack frame and first calls `0x8003287C`. It
then reads signed halfword `0x800FA038`. A zero halfword reads presentation
byte `0x800EB680` and calls `0x80032774` only when that byte is zero. A nonzero
halfword queries `0x80031C5C` with command `0xD4`. A nonzero low result byte
issues `0xD3` and `0xD4` to `0x8003066C`; a zero byte queries `0xC8` and, when
that low result byte is nonzero, issues `0xC8` and `2` to `0x8003066C`.

All eight static call sites are typed and remain unresolved service
dependencies. JAL updates to `ra`, delay-slot arguments, raw callback `v0`,
the two low-byte truncations, signed LH extension, LUI writes, and every NOP
remain in source order. Callbacks receive and may replace all 32 GPRs and
HI/LO. The epilogue reloads `ra` through callback-live `sp`, advances that same
`sp`, executes the JR's NOP delay, and then validates the resulting target.
Mapped stack/global aliases, partial knownness, and stopped prefixes therefore
remain observable.

The actual recovered match-tick parent exposes `0x80032B10` only as a narrow
service event. It does not carry a live CPU machine or stack. The production
adapter consequently requires an explicit full-machine snapshot with known
`ra=0x8002DDB4`; absence or malformed metadata is an argument refusal at the
actual `0x8002DDAC` boundary. The natural test uses a clearly synthetic,
fully specified caller snapshot over the same retained bytes as the parent.
It does not infer register preservation from the MIPS ABI. All other match-tick
services and its player, ball, net, and match-frame owners remain typed and are
forwarded to their existing callbacks.

Focused asset-free fixtures cover every control-flow exit and static refusal,
all operation cutoffs, exact call PCs/delay arguments/invocations, negative and
partial retained values, all sixteen query-result and return-address masks,
callback-live GPR/HI/LO/sp changes, stack/global aliases, mapped wraparound,
alignment and resource failures, malformed late bytes, unknown stores without
a knownness plane, invalid/overlapping/oversized regions, epilogue failures,
and deterministic complete RAM/mask/machine results. The natural fixture
reaches the actual `0x8002DDAC` call through `nba97_game_match_tick`, proves the
explicit-context success path, and proves missing context refuses before any
frame UI child executes. It whitelists the exact 31 synthetic prerequisite
service calls, records parent accesses and typed owner calls, and checks the
`0x8002DD8C`, `0x8002DD98`, `0x8002DD9C`, net, UI, and match-frame order. The
focused suite passes 275 checks and the natural suite passes 23 checks under
NDEBUG. The manager's independent original comparison passes 5,056 cases over
all 42 instructions, all 34 machine words and masks, the complete 2 MiB RAM
image, callback stack state, and every tested stopped prefix.

Gameplay shown: BLOCKED. This is a UI/menu orchestration routine, but the
render/update services at `0x8003287C`, `0x80031C5C`, `0x8003066C`, and
`0x80032774` are still typed dependencies. Until those owners are recovered and
composed with the native renderer, this assignment cannot produce or verify a
rendered UI frame. Synthetic callback completion is not visual evidence.

The native self-driving verifier also records an independent synthetic actual
match-tick invocation in `frame_ui_service_probe`: 18 source instructions,
three reads, one stack store, one UI dependency call, and all 31 whitelisted
prerequisite service calls. It checks the camera-service, delta-read, timing,
net, UI, and synthetic frame-completion order. The caller snapshot is explicit;
this does not establish a live full-machine bridge from the frontend. The
native display still shows User Setup. CPU-only diagnostic frames have matching
SHA-256 `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The complete asset-free CTest suite passes 359 tests. Captures and private
original comparisons remain ignored local evidence.

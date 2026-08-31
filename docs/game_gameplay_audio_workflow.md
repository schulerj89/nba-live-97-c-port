# Gameplay audio request routing

`game_gameplay_audio.c` recovers the GAME CPU request path at `29258` and
`29590`. It owns `29200..29257`, `29258..294F7`, `29590..295BF`,
`AB0B8..AB1E3`, `93D94..93DD3` and `93DD4..93E7B`. These routines select a
program and volume, update sequence/lock state, and synchronously dispatch to
the existing program-play owner or the general audio scheduler. They do not
decode a sample, key on an SPU voice, submit host PCM, or prove audible output.

## Corrected source identity

The earlier scoring receipt called `4C374` a gameplay-audio request. Raw GAME
source disproves that label: `4C374..4C5E7` writes the basket/net counters at
`B7A00..B7A0C`, the two net angles, and net/rim vertex deformation. Its only
callee is integer angle helper `A5B7C`; it has no audio or SPU call. The sole
caller `2D358` is merely its signed-16 wrapper. This owner therefore leaves
`4C374` to the net path and binds actual gameplay audio calls `29258` and
`29590`. The private correction retains all 157 original `4C374` instructions.

## `29258` sound-program request

The incoming argument is truncated and sign-extended to 16 bits for routing.
Signed values below 32 set their low-five-bit flag in `FE860`, unless live
halfword `F9FFE` suppresses that store. Programs zero through three derive a
level from the live object at `FDC48`: zero uses signed field `+18`, one uses
field `+14`, and two/three run the complete `29200` approximation over
`+14/+16/+18`. The `FDC48` pointer is visibly reread between the two `29200`
calls. Programs `60..63` remap to `2,4,6,0`; other programs retain their signed
low half.

The source clamps the dynamic levels to minimum 32 or 64 and maximum 128,
multiplies by live unsigned byte `21D7E`, rounds a negative product by adding
127 before arithmetic shift, multiplies by 12, and clamps at 127. The negative
branch at `294B8` is statically unreachable with the preceding source domain,
but remains part of the 168-instruction inventory. The final synchronous
request is `AC080(bank=*(u32*)21D6C, program, volume)`.

`29200` is deliberately not replaced by `abs`, `hypot` or floating point. Its
negation of signed `-32768`, subsequent low-half sign extension, quarter-term,
and wrapped additions are retained exactly. These are original arithmetic
quirks, not port fixes.

## `29590` sequence request

Live byte `21D7F==0` returns zero without entering the audio engine. Otherwise
the signed-low-half request enters complete `AB0B8`. That owner checks the
audio-enabled byte `D7B89`, mode byte `D793C==2`, unsigned sequence range
`0..15`, and the live record table. Read-only Ghidra program context proves
GAME `gp=800D79C8`; therefore the source `gp+29C` load is actual pointer
`D7C64`, not the nearby clock globals and not an invented host pointer.

Each 16-byte sequence record begins at `*(u32*)D7C64 + 28 + index*10` hex.
A zero active byte returns `-8`. The active path increments the wrapping lock
counter `C4B0C`, then compares current clock `D7948` with signed record end
`record+6 << 16`. If active, it publishes the proposed/clamped end at `D794C`,
clears `D7940`, writes the sequence byte at `D793F`, and stores wrapped
`3 * *(u32*)(record+8)` at `D7950`. A nonnegative signed program byte invokes
`AC080` with signed bytes `D793E`, `record+1`, and `D793D`; its return is
originally ignored. Negative program bytes skip playback but retain clock
state.

`93DD4` decrements and rereads `C4B0C` twice. Only a resulting zero drains
pending count `C4B08`. Each iteration rereads, decrements and rereads the count
before synchronously invoking scheduler `93734`; callback changes to pending
state affect the next loop. Nested lock depths therefore preserve pending work.
A native refusal retains the exact completed prefix, including a lock left
incremented when program play refuses or a pending count already decremented
when the scheduler refuses. The port does not invent rollback or resumability.

## Typed audio boundaries

`AC080` is the GAME-overlay ABI equivalent of frozen
`nba97_voice_program_play`: it checks the same shared enabled state and ten-bank
table, applies legacy/versioned program counts, reads the selected PATl/PT
pointer, and dispatches the same eight launch arguments. The request callback
must map `(bank, program, volume)` to that retained owner and all of its real
allocator/voice/channel/SPU requirements. A successful integer callback in a
unit test is not a production launch proof.

`93734..93D93` is a separate general scheduler owner. This slice exposes it as
the synchronous zero-argument `SCHEDULER_93734` boundary because it performs a
large timer/callback/24-channel update with its own downstream services. A
refusal is explicit; no successful no-op scheduler is permitted. `295C8` is an
unrelated two-instruction no-op and is not included.

## Verification and integration

The build owner adds the C source and test to shared CMake and maps `AC080` to
the same retained `Nba97VoicePrograms` instance used by other audio routes.
It must provide actual GAME bytes/knownness for every reached global and
sequence record. Unknown data, missing service ownership, budget exhaustion,
and malformed callback results refuse without preflighting or erasing earlier
stores.

Private evidence is under
`.local/verification/native_completion/gameplay_audio_request/`. The raw source
is bound to GAME SHA-256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
Strict MSVC Debug/Release and GCC/Clang C99/C++17 builds exercise the public
contract. An independent R3000 comparison executes the original instructions,
checks ordered reads/stores/calls and retained memory, covers every reachable
owned PC, and tests unknown/budget/service prefixes, nested locks, pending
zero/one/many, callback mutation, all event branches and clock stores.

This receipt proves CPU request routing only. It is not a natural gameplay
entry, audible device capture, decoded gameplay sound inventory, SPU timing
proof, first possession, or full-match completion.

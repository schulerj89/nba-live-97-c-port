# Original conditional ball attachment

`game_ball_attachment.c` owns the complete GAME functions `80057F5C`,
`80058120`, `800581C0` and their only callee, `8002D37C`. These are finite
native C implementations, using the existing player-frame address and byte
knowledge contract. They contain no instruction decoder, emulator, floating
point approximation, generated possession or fixture defaults.

`nba97_game_ball_attachment` selects one of the three original entry addresses.
Its native status and progress are separate from the original `v0` return
value. `nba97_game_hand_endpoint` exposes the complete standalone lookup with
three original numeric destination addresses. The math and child callback
slots in `Nba97PlayerFrameContext` are unused; the native hand lookup is composed
directly and successful calls are counted in `child_calls`. The
`GamePlayerFrame::attachment` adapter uses the same retained buffers,
normalized reference cells and original address mappings as the player pass.
The three source entries do not consume any incoming argument registers;
the actor argument passed to the hand lookup is assigned inside each entry.

## Source behavior retained

Each attachment reads signed possession at `FDBCC`. A negative value skips
all actor and ball accesses. `57F5C` then returns 32; the other two entries
return the sign-extended negative possession value.

For nonnegative possession the actual actor pointer is read from
`20BEC + possession*4`, followed by the ball pointer at `FDC48`. Neither is
invented or clamped. The pointers remain captured across subsequent accesses.
Hand lookup compares the complete hand argument against actor `+9A` bit zero.
Equality selects `FED20`; inequality selects `FAA04`. It reads the actor's
word ID and multiplies by eight with 32-bit wrapping. Table halfwords are
signed and shifted left by five. Their order is X at `+0`, Z at `+4`, and
height at `+2`; actor `+10` is added to the height.

The standalone lookup stores X before reading Z, and stores Z before reading
the endpoint height and actor height. Real destination aliases therefore
change later reads. Internal attachment output slots correspond to private
ABI stack locals, which cannot alias visible input allocations. Their stores
are not manufactured as observable RAM writes.

`57F5C` first obtains hand zero. Only actor states 21 or 22 also obtain hand
one and interpolate X/Z. Actor `+B8 == 1` uses the unsigned animation-frame
halfword minus 16 as its numerator factor. Other values use the animation
descriptor's byte `+7` minus the frame halfword minus one. The descriptor
pointer at `1ECEC` is reloaded for the denominator, whose value is descriptor
byte `+7` minus 16. Low 32-bit multiplication occurs before signed division.
No widened mathematical shortcut, endpoint clamp or frame repair replaces it.
Division by zero and signed `INT_MIN / -1` preserve the original BREAK7/BREAK6
positions, reported as `NBA97_FRAME_ARITHMETIC_TRAP`.

The mode-one quirk is retained: `57F5C` writes ball X/Z and attachment state
one, and returns the first hand's height, **without writing ball height**.
`58120` and `581C0` use hand zero or one, write all three ball coordinates,
write attachment state two or three, and return that same state value.
Every caller rereads live actor Z after the ball X store, so an actual
ball/actor alias changes the subsequent Z result.

## Ownership and refusal

The caller provides original address provenance, canonical metadata, retained
memory and sufficient operation budget. Full reached spans are validated;
unvisited allocations and bytes are not consulted. `2D37C` discards the high
byte of actor `+9A` after the original LHU, so that byte may be unknown while
its metadata must still be valid. Unknown ball-pointer bytes are not consumed
until the first ball store; the preceding hand and actor-X reads still occur.

Bounds, unknown consumed data, native operation limits and unsupported source
alignment stop with the preceding mutations retained. These are native
ownership boundaries, not newly attributed original gameplay behavior. The
source's explicit arithmetic BREAKs are distinguished by their exact PCs.
The result value is published only on successful completion. A partial call
is not resumable; clone and rebind the complete retained state before a call
when publication must be atomic.

The functions require established actor and hand-endpoint arrays. They do not
choose an attachment mode, acquire possession, perform loose-ball physics,
initialize player geometry, load a match or make the outer frame run. The
`49018` orchestrator owns when these entries are reached.

## Verification

Private evidence lives in
`.local/verification/native_completion/ball_attachment/`. A fresh raw-source
scope check reads all 222 words of the four complete functions from GAME
SHA256 `d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
No COP2 instructions or further callees are hidden in that closure.

Strict MSVC Debug/Release and GCC C99/C++17 builds each pass 624 public checks.
These include selected-hand identity, unclamped hand arguments, sign extension,
ID wrapping, endpoint output aliases, actor/ball aliases, interpolation
directions, unchanged mode-one height, both overflow traps, divide by zero,
partial knownness, and every operation cutoff for the three basic entries.
The actual C++ adapter is also exercised with unavailable serialized pointer
bytes and explicit normalized references; rejected mapping metadata clears
the returned value and completion receipt.

Each private Debug/Release comparison executes 1,756 original/native cases,
comparing 4,147 ordered visible stores, 19,734 ordered reads, successful `v0`
returns and all persistent nonstack bytes across 109,472 original instructions.
The source reference executes original branch and load delay slots. Its DIV0
experiment preserves the R3000 quotient/remainder long enough to reach the
original following BREAK, instead of stopping prematurely at DIV.

There are 592 matching retained refusal/trap prefixes. Coverage reaches 221
of 222 words. The only unexecuted word, `800580A4`, is the second divide-by-zero
BREAK: the first divide uses the same denominator and necessarily traps first.
No executed coverage is claimed for that impossible branch. Fixtures explicitly
supply actor, table, descriptor and possession state; these tests establish
the bounded owners' fidelity, not a natural gameplay frame or complete match.

An additional 153 cases in each MSVC configuration compare original execution
against the actual `GamePlayerFrame::attachment` method, using normalized
actor, ball and animation-descriptor reference cells whose raw pointer bytes
are deliberately unusable. All persistent bytes and byte knowledge, successful
return values, 510 visible stores and 15 retained refusal prefixes agree over
10,854 original instructions. Cases include ball/actor aliases, a ball output
overwriting its own captured pointer cell, and unknown unused flag padding.

# Original ball, reflection and ground-shadow pass

`game_ball_frame.c` owns GAME `49300` and `49D34`, including their camera loads,
`56624` center projections and `55FE4` shadow projection. It reuses the native
`56914` packet linker. The `ball` and `ballShadow` methods on `GamePlayerFrame`
use the same retained allocation mappings and geometry as the player pass;
they do not copy, reset or manufacture frame state.

The original `49018` frame calls these after the player and court passes and
the conditional ball-attachment update. These render routines do not implement
ball simulation, acquire possession, choose an attachment, initialize textures
or start a match. Ball/reflection/shadow packets and texture uploads must come
from the original `4D490` initialization. The pass consumes their retained
bytes; it does not need to dereference the old released image-container links.

## Preserved source behavior

The animation update first observes its actual pause/forced-update conditions.
When selected, it subtracts three absolute physical-entity motion halfwords
from `DC7FC`, wrapping to 32 bits. A negative result resets the counter to 500
and increments the animation frame using wrapped addition and signed remainder
by 15. Negative incoming frames are not repaired into the ordinary 0..14 range.

Sixteen UV writes update ball and reflected packet banks. Every byte reloads
both frame `103F9C` and bank `1EDE8`; no whole-packet snapshot replaces those
reads. The two sprite images retain different vertical UV directions and the
original endpoint values. Neither bank nor frame is clamped by the port.

The ball entity comes through two pointer reads at `FC658`. The caller retains
some pointer values while reloading others in the original order. Coordinates
use `(signed32 >> 8) << 3` followed by halfword truncation, in X/height/Z order.
After the first projection, the reflected color clamps to 1..128 based on the
stored height halfword. Each color byte reloads the bank. The reflected height
is then read from the live entity again and negated before its final shift.

Both projected depths come from the original unsigned 16-bit SZ3 value shifted
right by two. Each zero depth becomes one before division. Screen-size
arithmetic preserves the source's intermediate 32-bit shifts and subtraction
before dividing by 1000; it does not compute an unbounded floating-point size.
The primary size at least 1000 suppresses both links after the earlier UV,
center and color writes. Smaller primary sizes clamp upward to 100.
**The reflected size does not receive that clamp.** Its packet can still be
updated even when the later `DCF10` condition suppresses its link.

Both packets link using the primary ball depth's low twelve bits. The first
link uses the captured ordering-table pointer and bank; the second reloads both
after the first link. Raw packet tag length bytes survive. The source's exact
halfword-store order, including the reflection's last store in the link call's
delay slot, is retained.

`49D34` snapshots ball X/Z before constructing the ground square. It writes four
zero Y values first, then the X/Z corners with half-extent `0x20`. It reloads the
camera, projects three vertices with RTPT and the fourth with RTPS, and links
the selected 40-byte shadow packet at `D9234 + bank*40` to main OT `+3FF8`.
There is no new projection-flag cull.

## Ownership and failure

The C entry points use the existing player-frame access and math contracts.
Original numeric addresses, canonical byte knowledge, source alignment and
retained geometry are required. The C++ adapter supports normalized reference
cells whose raw pointer bytes are deliberately unavailable; it resolves their
source encoding from the supplied allocation addresses. Metadata is not a
substitute for original address provenance.

Original private ABI stack slots cannot alias visible allocations. IR0 and
FLAG results stored only to dead private stack locals are not fabricated as
visible memory. Original word loads still access and validate their full span;
unused matrix/vector high halves may remain unknown where the geometry write
discards them.

Operation limits, unknown consumed bytes, unavailable memory and unsupported
packet-link alignment refuse with the completed CPU/math prefix retained.
`Nba97PlayerFrameProgress` records the stopped PC/address and completed stores,
math operations and links. Copying `GamePlayerFrame` does not clone its borrowed
buffers. Clone/rebind the whole retained memory and geometry before execution
if publication must be atomic; never retry a partial call in place.

## Verification

Private evidence is in `.local/verification/native_completion/ball_frame/`.
A fresh read-only Ghidra export checks all 807 words across both callers and
their five projection/link helpers against raw GAME SHA256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.

Strict MSVC Debug/Release and GCC C99/C++17 builds each pass 184 public checks.
The existing player-pass tests also pass all 1,129 checks in both MSVC
configurations after the two new C++ method declarations.

Each private Debug/Release comparison runs 585 original/native C cases,
comparing 16,111 ordered visible stores and all 64 retained geometry words over
242,236 original instructions. Ninety-five cases stop at a matching retained
prefix. Coverage includes all 90 ground-shadow instructions and 645 of 653
ball instructions. The eight unexecuted instructions are divide guards:
the original SZ3 range and zero-to-one clamps make zero and -1 denominators
unreachable. No claim of executing these impossible branches is made.

Directed cases cover signed frame arithmetic, both banks, height/H extremes,
entity/packet/ordering aliases, incomplete memory, source alignment traps,
unknown overwritten inputs and unused word padding. An additional 53 cases per
configuration compare the actual C++ adapter against original execution with
normalized reference cells, including deliberately unusable raw pointer bytes:
1,509 stores, all persistent byte knowledge, and all 64 geometry words agree.

These tests use explicit camera, entity, ordering and packet inputs. They do
not prove resource arrival, a natural gameplay frame, physical input, exact
hardware rasterization or a complete match. Original instruction execution and
the independent geometry reference remain private validation tools only.

An additional initialized-resource comparison consumes the retained RAM and
VRAM produced by the complete `4D490` initializer with actual BALL/ASDW bytes.
Both released source containers are made unavailable before rendering; their
old numeric globals are not dereferenced. Camera, entity, control and empty
ordering-table state remain explicit fixtures, and packet templates are never
overwritten by those fixtures. Sequential original execution and the actual
normalized C++ `ball` then `ballShadow` owners agree in 60 combinations per
configuration: fifteen animation frames, both banks and reflected-link enable
or suppression. All 3,720 stores, persistent bytes and byte knowledge, and
64 geometry words agree across 45,030 original instructions.

Six private native pixel diagnostics use the resulting packets and uploaded
VRAM at frames 0, 7 and 14 in both banks. The renderer completes six triangles,
494 pixel writes and 4,099 ordering links in each view. The new byte-knowledge
packet reader accepts the original unused UV high halves without marking them
known; missing consumed fields still refuse. Images and provenance remain in
`.local/progress-screenshots/diagnostics/initialized-ball-checkpoint28/` and are
labeled as fixed-camera diagnostics with no court or players. They demonstrate
this resource-to-render composition, not natural match entry or gameplay.

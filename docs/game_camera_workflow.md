# Original live camera and controller pass

`game_camera.c` recovers GAME `51098`, its `4EA88` controller/visibility pass and
the actual cached logical-input reader `8F224`. It also owns the reached
26-byte `AA468` copies, six-word `935C4` generator, scratch monitor tests,
visibility helpers, height writer and `56080` Euler matrix construction.
The camera uses the existing native `GamePlayerRootGeometry` callback for
retained fixed-point geometry. No instruction interpreter or emulator runs in
the production owner.

This is the camera called by `49018`; the earlier startup camera is a separate
routine. The entry requires actual retained memory and input/device services.
It does not connect User Setup to a match, establish cold-start camera values,
load resources, or submit a complete gameplay frame. The application and CTest
builds include it alongside the [player pass](game_player_frame_workflow.md).

## Input and controller behavior

`8F224` reads the optional wait flag, requests the actual clock value, and
compares it with the retained tick. A new tick is stored before the device
refresh request. The reader then polls eight ports, rereading the mode before
each call, maps the original fourteen raw pad bits to logical bits, and writes
each result before the aggregate. The cached path does not rewrite the
aggregate. An unsigned controller index above seven selects the aggregate
slot, including a caller-supplied negative value encoded as unsigned.

`4EA88` calls that reader for all eight controller slots and again for camera
controls. Its unlock sequence reads the actual jump table, rather than using
a guessed state-to-key mapping. Unknown branch destinations explicitly refuse.
The state-one height edit retains the separate physical and selected entity
lookups: if their attribute pointers alias, the same height byte can be
incremented or decremented twice. Wrapping byte arithmetic precedes the
original clamps at `0x12` and `0x90`; the final source height scale is retained.

The three template copy requests are exactly 26 bytes. Their forward and
overlapping backward paths preserve the source load/store batches, including
LWL/LWR and SWL/SWR byte spans. They propagate unknown copied bytes without
inventing knowledge. This is not a general replacement for every `AA468`
caller or length.

The pass preserves debug-selection wrapping, independent simultaneous held
camera bits, angle masking, visibility-mask rereads and exact court boundary
comparisons. It retains the source random generator's carry, wrap and write
order. Scratch monitor flags and screen-copy rectangle requests follow their
original branches. None of these behaviors is simplified into host keyboard
state or a modern camera model.

## Matrix and retained geometry

`51098` conditionally runs the controller, snapshots angles, base translation
and offsets, then performs the original angle stores and Euler construction.
`56080` wraps products to 32 bits and negates before the arithmetic shift;
negative terms retain their asymmetric rounding. The camera scales the first
matrix row by signed `16/10`, installs zero translation, and transforms the
original camera vector through `56650` into `FC61C`.

The final matrix translation in `F9FD8` is the transformed vector plus the
saved signed base halves. **The retained geometry translation remains zero
at this point.** Later source callers reload the final matrix. Updating the
geometry translation early would change source behavior. Unused matrix-word
padding and the high half of the final vector load are accessed and validated
but need not be known when the original geometry operation discards them.

## Memory, services and refusal

The API uses `Nba97GameTextMemory` with actual original numeric addresses.
Regions must be nonempty, nonwrapping and disjoint in that address space;
descriptors, journal and progress cannot alias mutable storage. Canonical
per-byte knowledge is required at each reached access. A null knowledge array
means all bytes are known; an opaque unknown copy into such a destination
refuses because that destination cannot represent the result. Source code and
private ABI stack must not alias mutable regions. Private stack rectangles
are passed as typed values rather than fabricated original stack addresses.

External services are explicit synchronous requests:

| Target | Required service |
| --- | --- |
| `8F1D4` | Optional input wait/service call |
| `A5810` | Actual clock value |
| `90F6C` | Device refresh |
| `913BC` | Raw controller read |
| `997E4` | Requested rectangle transfer |
| `A8DF4` | Interactive monitor entry |
| `536A0`, through `53680` | Monitor command |

The callback must implement a request or refuse; there are no successful
default clock, input, transfer or monitor implementations. Calls may mutate
retained memory synchronously, so later reads observe those changes. Math
callbacks use the existing named fixed-point operations and must not mutate
the memory mapping. This entry supplies neither a physical input backend nor
an interactive debugger implementation.

Every completed store and requested external call has an ordered journal
entry. Capacity exhaustion, unavailable memory, unknown consumed data,
alignment faults and refused services retain the completed prefix. Progress
also records the stopped PC/address and original math callback result. To
publish atomically, clone both memory and retained geometry, run on that
clone, and publish only on success. Do not rerun a partly applied invocation
as though it were a continuation.

## Verification

Private evidence resides in `.local/verification/native_completion/game_camera/`.
A fresh read-only Ghidra export checks 2,076 instruction words across sixteen
functions against raw GAME SHA256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
That inventory includes external service targets; it is not a statement that
every inventoried function is implemented or every branch is exercised.

Strict MSVC Debug/Release and GCC C99/C++17 builds each pass 364 public checks.
The private comparisons execute the original caller and input-reader
instructions with explicit clock/pad/transfer fixtures and refusals at
unimplemented interactive monitor requests. Each configuration compares 787
controller-only cases (20,436 stores, 34,189 ordered effects) and 787 complete
camera-entry cases (39,714 stores, 53,467 ordered effects). The latter compare
all 64 retained geometry words using the independent private geometry
reference, plus every persistent byte and its knowledge state. Private ABI
stack/register saves are excluded from visible-memory comparisons.

These cases include all 127 camera-entry instructions and all 147 Euler
instructions; they exercise 1,081 of the controller's 1,196 instructions.
Controller branch coverage is incomplete. Directed cases cover all 24 unlock
table actions, height wrapping/clamping, overlapping copies, five opaque-copy
knowledge cases, monitor-present branches and 110 journal cuts. The private
oracle tracks per-byte LWL/LWR knowledge so adjacent partial loads can
establish a complete word without falsely declaring still-unknown bytes
known. Source execution, fixtures and the independent geometry reference
remain private validation tools.

These receipts are not a no$psx gameplay capture, a physical-controller test,
or proof that the native app reaches a natural match frame.

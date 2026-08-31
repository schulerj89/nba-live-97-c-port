# Native ball release: GAME58610

`game_ball_release.c` closes the complete 294-instruction `58610` owner plus
its shared `2AB70` RNG calls. It supplies the real ball-release implementation
required by the tipoff owner `5BC34`; no callback is needed inside this routine.
The result is a loose ball with computed velocity and an intended receiver,
not completed possession or a rendered frame.

The native interface takes the actual thrower and receiver addresses and the
checked live-state adapter from `game_tipoff_phase.h`. An outer callback handling
owner `80058610` calls `nba97_game_ball_release` with both original arguments
and the same context. The adapter must map retained native objects and actual
original resource bytes; it must not allocate fake original addresses or supply
default player ratings, trajectory tables, ball state or RNG seeds.

## Original state and resource order

`58610` first clears FDBD4, captures the receiver word0, FE8CC and ball reference
FDC48, writes FDBD6=1, clears possession FDBCC toFFFF, and copies receiver word0
low16 to FDBD2. For the ordinary FE8CC=0 path it writes throwerB4=30 and selects
a trajectory table using signed FDC02 and unsigned FDC00:

| Selection | Table |
| --- | --- |
| FE8CC nonzero | B8198, row3 |
| FDC02 negative | B8198, rowFDC00 |
| FDC02 positive | B81B0, rowFDC00 |
| FDC02 zero | B81C8, rowFDC00; adds an arithmetic quarter to both horizontal velocities |

Each entry is two halfwords. The signed first value is the captured travel
duration. The second is read much later, after horizontal ball stores, and
selects fixed negative vertical velocity or computed arc velocity. These are
the actual GAME tables, not motion duration windows or frontend preview data.
The source does not clamp the row index to six; an unavailable reached span
must refuse in the native adapter.

The owner publishes the captured ball reference to FDC34, predicts receiver
X/Z from current coordinates and signed velocity times duration, and clamps
the predicted position to the original court limits. A clamp adjusts that
receiver velocity halfword through signed division. It then computes the ball
velocities from the predicted target and current ball position, optionally
applies the quarter boost, and sets the receiver's B6 delay unless actor1A=15.

The original accuracy path reads live FE8CC, option byte21D72, FDC00, optional
status20 and player byte1A, phaseFDB90, claim04 and shared1EDEE RNG. It can add
random error in phase81; only phase82 takes the phase-specific suppression.
The routine finally writes ball14/16, loads and writes the vertical entry,
conditionally writes a second computed ball18 value, and clears phase when
the final signed phase test is below128 and FE8CC is zero.

## Fidelity and retained prefixes

- Multiplication, addition and subtraction wrap to32 bits before signed tests.
  Signed division truncates toward zero; the arithmetic quarter rounds negative
  values down. The native owner does not use floating-point trajectories.
- Original division-by-zero and INT_MIN/-1 BREAK outcomes have explicit native
  results and the actual BREAK PC. Earlier mutations are retained.
- The duration is captured once. The vertical entry is read after the ball
  horizontal stores, so a genuine source-address alias may change that entry.
- The ball reference, receiver ID and FE8CC can initially be opaque values.
  The intervening stores happen before their first required interpretation.
  An unknown vertical entry is copied to ball18 before refusing at its signed
  interpretation. Unknown bytes are never promoted to known zero.
- RNG operates on shared1EDEE, including the source's two stores for seed zero
  and its bit14 feedback test. There is no private replacement random stream.
- Every reached access uses the live native adapter. Missing spans, misalignment
  and noncanonical knownness refuse without preflighting untouched allocations.
  Per-byte knowledge that cannot fit the whole-value interface must explicitly
  refuse. Receipts are not resumable; clone all owned state for atomic publish.

The source has four compiler BREAK sites that cannot be reached in ordinary
execution with retained register values: later zero-divisor checks follow an
earlier mandatory division by the same captured duration, and the two clamp
overflow checks cannot produce INT_MIN/-1 given the signed16 receiver velocity
range. The guards remain implemented. Private evidence enumerates all 262,144
clamp boundary/velocity combinations relevant to the overflow condition; it
does not corrupt source registers merely to manufacture coverage.

## Verification and remaining gameplay boundary

Private evidence lives in `.local/verification/native_completion/ball_release/`.
Strict Debug and Release builds each pass the public directed tests and 1,940
native/original comparisons with 24,011 ordered stores and 50 actual source
division traps. The original CPU executes the entire `58610` and `2AB70` with
no callee hooks. The first 1,800 cases use the original static trajectory bytes;
additional cases cover exceptional durations and actual address aliases. All 290
reachable owner instructions are exercised; the four excluded BREAK sites are
documented separately. Another 331 cases compare each refused access's entire
write prefix with original execution stopped at that same dynamic source PC.

The native tipoff continuation, `5BC34`, and this owner are composed in 96 cases
per build. Their inputs are obtained by executing actual `602CC` acceptance and
`5D140`, then the full native continuation/release is compared with original
execution. All 2,946 ordered events match. Sound29590 remains an explicit external
boundary; the conditional582DC reset boundary is not reached in these fixtures.
They demonstrate actual source composition, not a natural device/input run.

An initial tipoff frame is earlier: `67468` calls `2DD84` after period setup and
attributes, before a tip release. The current rendering work can show that
raised-ball state without pretending to have completed possession. For a live
first contact, the remaining gameplay requirements are the real `52914` body
geometry outputs FED20/FAA04, upstream collision/acquisition `602CC/5D140`, and
the ball-time owner `6EF60` within the actual loop. Sampled motion angles alone
do not establish hand geometry. This owner is separate from existing player
physics `6CFE0` and does not replace the loop or caller state producers.

The C source and standalone C++ test target are registered in CMake. Only
the shared tipoff header is required; no audio, renderer, platform SDK, emulator
or new resource allocator is linked by this owner.

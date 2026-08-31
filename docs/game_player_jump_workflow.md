# GAME6A2E4 jump request

`game_player_jump.c` recovers the complete 303-instruction direct owner and the actual 17-instruction GAME2AB70 RNG. It does not synthesize a jump button, bypass GAME61760, or implement GAME5A570. Animation calls have exact synchronous boundaries so the existing motion and queue owners can execute them.

The entry accepts one explicit owned entity index and the original raw 32-bit `a1`. GAME61760 supplies zero at callsite `80062158`; other callers may supply nonzero values and face additional rating, ball-height, threshold and probability rejections. No zero/nonzero argument is inferred from controller identity.

## State and resource ownership

The typed state contains eleven entity field views, the actual FDC48 ball reference, owned player/status references, up to 24 owned player/status records, eleven globals and the shared raw halfword at 1EDEE. Player byte09 and byte17 come from the entity's current `+20` binding; status halfword20 comes from its current `+1C` binding. They are not roster-slot defaults. Ball/entity aliases are preserved. The original captures the ball reference before the rating gates, and this owner retains that captured reference until its last use.

The resources are caller-owned, immutable source data:

| View | Source access | Native boundary |
|---|---|---|
| `threshold[0]` | Signed halfword at B89C4 + 2 × index | Explicit halfword count |
| `threshold[1]` | Signed halfword at B89CA + 2 × index, phase81 | Explicit halfword count |
| `motion_b86f4` | Four byte row at B86F4 + 4 × unsigned FDC04 | Explicit row count |

The threshold index comes from arithmetic-shifted signed ball height and unsigned player byte17, after a nonnegative delta test. Raw heights can produce indices outside the owned source image. The port reports `REFERENCE` for unavailable storage; it does not clamp the index or invent a threshold. Argument zero still performs the threshold load even though its rejection is disabled. A complete raw FDC04 row window is 262,144 bytes, B86F4 through F86F3. Ordinary rows are only a subset. No retail table bytes are embedded publicly.

## Ordered effects and original behavior

The mutable candidate and receipt preserve direct stores, RNG stores and calls in source order. A successful native return has `receipt.completed=1`; `receipt.accepted` separately records the original return value. **An accepted value of zero is an executed rejection, not a failed transaction.** In particular, its RNG draw must remain visible to the caller before fallback logic or another entity runs.

The preserved source behaviors include:

- GAME2AB70 reads the same shared 1EDEE state as other users. A zero state first stores A5A5, then advances. Its SLL17 tests bit14, not bit15. The source can produce zero again; the next invocation performs the replacement again.
- Argument zero still reads enabled status data and consumes RNG, although it ignores the random rejection. Status20 is signed after the original LHU/shift sequence; its bonus ranges from -32 to 31. A later distance rejection keeps the consumed draw.
- Ball-relative X/Y differences wrap at 32 bits before signed division by 20. Rating below66 then arithmetic-halves both results, rounding negative odd values downward. The distance gate accepts each component from -512 through512 inclusive.
- Phase81 quarters these newly calculated signed halfwords before requesting77 and queuing78/79. Other phases copy C0 to A6, request74 and queue75/76. The separate GAME61760 fallback has different velocity semantics.
- Conditional motion locks, cached flags and full queues remain the responsibility of the actual existing callees. They can skip or drop requests while GAME6A2E4 still returns1. An accepted request is not proof that a new motion started.
- The special branch resets locks4C then48, flags64 then60. It captures the FDC04 row after GAME5A570, so later callback changes to FDC04 do not change the chosen row. FDC0C is reread after both setters and again after the queue call; FDC14 and the divisor are read after that call.
- The special signed division retains the original zero-divisor trap after the vertical-velocity store. Actor1A=19, rate9E and phase0 are not written on this trap. The compiler's separate INT_MIN/-1 trap sequence is unreachable because the divisor is loaded unsigned16; no fictional negative divisor is introduced.

Unknown values are copied as unknown where the original merely copies data. A needed branch, pointer dereference or arithmetic operand must be known. Unknown payloads are canonical zero metadata, never permission to write a fabricated source zero. Invalid representations refuse entry. Runtime failures retain the executed prefix in the candidate/receipt; a caller needing atomic publication must use its complete candidate transaction.

## Exact callback composition

The callback receives the captured entity, original callsite and declared raw arguments. It must execute the actual callee, publish all relevant mutations back into the typed view, and only then return1. Null callbacks and zero returns mean pending; negative returns mean failure. Internal GAME2AB70 is recorded but never delegated.

| Owner | Callsites | Arguments after entity |
|---|---|---|
| 5A570 | 6A55C | Sign-extended entity BC, FFFFFFFF |
| 5699C | 6A5A4 | Captured motion-row byte0 |
| 56AA4 | 6A5B0 | Captured row byte1 |
| 56B78 | 6A724 / 6A750 | 77 / 74 |
| 56CE0 | 6A5D0 | Captured row byte2, byte3 |
| 56CE0 | 6A734 / 6A760 / 6A770 | 78 / 75 / 79-or76, blend0 |

Use `nba97_game_period_switch_motion` for the setters and conditional switch, and `nba97_game_animation_queue` for56CE0, with the actor's actual prior animation fields and retained normalized motion headers. Import only written fields; refresh the shared jump view before resuming. The caller must also preserve these callees' effects outside the jump view.

GAME5A570 is a substantial 498-instruction owner, not a status notification. It writes shared FDC04..FDC24 setup, player movement/angles/status and several globals, consumes shared RNG, invokes72C40,29590,2D3F0, trigonometric/fixed-point helpers and5A438, and can trap in division. Its actual return mutations are required before the special tail resumes. This module deliberately leaves that exact call pending instead of choosing a motion row or fabricating its outputs.

## GAME61760 producer audit

The source-only audit follows the logical-mask20 path without claiming native input support. GAME686B8 walks the actual entity table, resolves the selected controller through entity signed halfword04 and the controller-reference table, requires controller halfword28 zero, and calls8F224. It passes the returned value through2D2DC using the actual controller mapping at3F or47, then700E4. At68808, GAME61760 receives the current entity, actual controller reference,700E4 result as`a2`, and the earlier mapped value as`a3`. Thus logical20 is not a raw device-button constant.

Before62158, the ordinary input branch can redirect to other owners for mixed logical bits, possession actions or passes. Its relevant gates include FE8CC=0, FDB7C=0, actor1A!=20, zero height/vertical velocity, and zero low two bits of cached flags60/64. Phase82 adds side/possessor checks. For the actual6A2E4 call, the actor must not be the current possessor, logical20 must be present, BE must be below41, signed FDBD2 must be negative, the side must differ from FDB94 or the possessor must be negative, and FDBD4 must be zero. The call supplies `a1=0` and returns immediately only if originalv0 is1.

When these later gates fail, GAME61760 can still take its fallback at62174. In phase81 it writes C4=600 and requests77/78/79 **without quartering existing velocities**. Outside phase81 it arithmetic-quarters existing velocities and requests68/69/70. FDBD4's alternate angle gate can instead call6A144. These are separate source paths, not substitutes for GAME6A2E4.

Initial65DB0 writes FDB7C=120. On its countdown branch, GAME68BF8 calls7A668 and subtracts FDB6C only when the returned low byte is zero; it clamps a negative signed-halfword result to zero at68DC0..68DE4. The alternative invokes7001C and conditionally7A680. Until the actual countdown reaches zero, GAME61760 refuses this route. Calling the recovered jump owner directly from a host key event would bypass these gates.

The next bounded integration work is an actual ordinary-input dispatcher for61760, with exact early delegation/pending boundaries and both jump/fallback paths, followed by its700E4 edge/held-mask producer and686B8 caller composition. The play-call branch must remain explicit pending until recovered. This audit did not expand61760's complete685-instruction implementation or guess a device mapping.

## Verification

Public tests cover RNG fallback/bit14, signed status, consumed-draw rejection, signed velocity rounding, aliases, missing resources/unknowns, pending callbacks, captured rows, callback rereads and the source division trap. Private original-instruction comparisons cover every reachable direct instruction; the three unreachable overflow-tail PCs are explicitly identified rather than counted as executed.

Ignored receipts are under `.local/verification/native_completion/player_jump/`. `verify_jump.py` compares4,096 direct cases per build with synthetic mutable callback boundaries and exhausts all65,536 RNG states. `verify_motion.py` separately compares600 cases per build using actual normalized mocap headers and actual motion/queue callees, including every child-entry image and the final full entity records. No successful semantic hooks replace those original callees. The special5A570 path is excluded from that composition proof and retains its pending contract. `verify_gate.py` checks18 original61760 gate/redirect cases by stopping at the actual next call, without asserting native gating or successful unavailable callees.

These proofs use explicit synthetic entity/controller states and owned original resource data. They do not establish cold-entry provenance, live input,5A570 completion, possession, rendering or a complete playable match loop.

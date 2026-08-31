# GAME61760 ordinary input and700E4 logical-mask producer

`game_player_input.c` recovers all373 instructions belonging to GAME61760's
initial selection-marker prefix and complete ordinary branch. Its remaining
312 instructions are the explicitly pending play-call continuation at617D0.
The same module recovers complete GAME700E4 (74 instructions) and its actual
GAME7A498 direction helper (55 instructions). No physical button mapping,
countdown, audio readiness, controller polling or whole match loop is inferred.

## Owned state and boundaries

The typed state views eleven entities, eight controller records, eleven entity
table references, actual player bindings and24 player byte1D values. It retains
the current team reference FDC40 and reference FDC34 separately. All table
references identify owned records, preserving aliases rather than deriving an
entity from a slot number. A positive source index beyond the eleven owned
table entries returns `REFERENCE`; eleven is a native storage boundary, not a
new check attributed to the original unchecked table access.

Entity fields are00,04,10,14,16,18,1A,60,64,A4,BA,BC,BE,C0,C4,D8,D9,E4.
Controller fields are26,2A,2E,30,32,34,38,3C. The offset and width accessors
describe every view: entity00/10 and controller38 are words; actor1A and
entityD8/D9 and controller3C are bytes; the remaining listed fields are halves.
The ordinary globals FDB9C,FE918,FDBCC,FDB94,FE8E2,FE8CC,FDB90,FDB7C,FE880,
FDBD4,FDBD2 are raw halfwords. The direction helper additionally reads raw
wordsD8EEC/FC99C and byteFA378, only on paths that need them.

Entry metadata must be canonical. Unknown payloads are zero metadata and do
not grant permission to store a fabricated source zero. Needed branches,
arithmetic and dereferences require known values; plain copies preserve
unknownness. The state, receipt and immutable resources must not overlap.

Both owners mutate a candidate and record source stores and calls in order.
Failure retains the executed prefix. A stopped run is not a resumable cursor.
For atomic publication, clone the complete owned state and callback context,
run the operation, and publish only a completed supported transaction. Callback
effects outside this narrow view remain the caller's responsibility.

## Exact700E4 behavior

The input is the full raw mapped word returned by2D2DC. GAME700E4 captures
controller30 with a signed halfword load, stores the low16 input into2E and30,
then computes XOR and rising edges with the captured sign-extended value. It
stores the low16 XOR in32 and low16 rising mask in34, but returns the **full32
edge mask**. A negative old30 can therefore suppress high input bits. Do not
truncate the result to controller34 or reinterpret logical bits as host keys.

Direction priority is literal: bit8 precedes bit4, and bit1 precedes bit2 when
opposites coexist. Controller38 receives that direction before GAME7A498.
The helper's low16 direction8 case returns8 before reading mode/camera values.
Otherwise it tests only low8 of its mode argument, reads the required original
camera state, applies the original mode1/4/5/2/default cases, and masks to7.
The private proof also checks the actual five-entry source jump table2794C.

GAME700E4 writes controller2A twice: first the helper's result, then1024 for8 or
the result shifted7 otherwise. These are separate ordered source stores.
Finally, held logical400—not the edge mask—reads signed selected26 and the
actual entity table, writing entityE4=10 for a nonnegative owned selection.
Repeated held400 can therefore write E4 even when the returned edge mask is0.

The internal7A498 call is recorded but never delegated. Its argument-knownness
bits allow a neutral8 request to carry an unused unknown controller3C without
inventing its value. Unknown old30 stops after the two initial input stores;
an unowned selected entity stops after the completed helper and2A writes.

## Exact61760 ordinary behavior

Logical100 first copies entity04 to FDB9C. Logical80 then adds128 to entity04
and stores its low16 value there, so both bits produce two writes and the
second wins. These writes precede either dispatcher route. A nonzero logical
mask with mapped bits3000 enters the play-call continuation. The receipt
retains the captured team/controller references and both masks, reports
`PLAY_CALL_PENDING` at617D0, and stops before the first play-call dereference.
No callback may claim that this unimplemented continuation completed.

The ordinary branch retains all early logical200,30 and800 routes before the
general FE8CC/FDB7C gates. Those early owners may therefore run while later
ordinary actions are gated. After a mutable callee returns, source reads use
its current effects; side, possessor, actor, phase and movement are not frozen
at entry. The current entity argument remains the source's capturedS0 entity.

Later paths preserve the signed possessor lookup, actual table aliases,
CPU-controller tests, actor thresholds, height and vertical-velocity gates,
both cached low flag tests, and phase82 side/possessor checks. Signed halfword
side claims are compared with the full unsigned entity byte; they are not
silently narrowed to a byte. The primary motion43 call precedes the C0-to-A4
copy; C0 is reread after the setter. Motion42 can follow without rechecking the
earlier team/possession condition. The later possessor test is reread.

The three consumed callee returns have different source tests:

- 5BDD8 uses full32 nonzero and then writes entityD8=1.
- 5ADB8 uses only low8 nonzero before calling5C008.
- 6A2E4 returns immediately only for exact full32 value1. An executed rejection
  with value0 keeps its shared RNG draw and other child effects before fallback.

The actual jump boundary requires logical20, BE<41, negative signed FDBD2 and
the original side/possessor relation. FDBD4=0 calls6A2E4 with raw argument0.
Nonzero FDBD4 instead checks the wrapped BC-angle difference against the
captured FDC34 entity and can call6A144. Failed later gates reach fallback.

**The two fallbacks are intentionally different from6A2E4.** Both first write
C4=600 and reread phase. Phase81 requests77, queues78 then79, and keeps existing
velocities unchanged. Other phases arithmetic-quarter the existing signed14/16
values before requesting68 and queuing69/70. Negative odd values round downward.
Do not replace the fallback with an unconditional jump call or reuse6A2E4's
phase81 quartering of its newly computed ball-relative velocity.

Original animation locks, cache flags, full queues and channel divergence are
preserved by the actual motion/queue owners. Successful dispatcher completion
or jump acceptance does not prove that animation changed. These original
quirks are commented in the code; native ownership refusals are distinct from
source bugs or source-side gates.

## Synchronous owner inventory

Callbacks return1 only after the actual requested owner completed and all
relevant mutations were copied back. Null/0 means pending; negative means
failed. A consumed originalv0 must be supplied separately with knownness.
The tests' synthetic mutable callbacks validate ordering only, not completion
of unavailable owners.

| Owner | Source callsite | Actual arguments after a0 entity | Integration status |
|---|---|---|---|
| 6CD50 | 61CE8 | None | Pending owner |
| 612E4 | 61D14 /61E3C | 1 /0 | Pending owner |
| 5BDD8 | 61DC4 | 1 for logical20, otherwise FFFFFFFF | Pending owner; fullv0 consumed |
| 610FC | 61E2C | None | Pending owner |
| 5B258 | 61EDC | Current entity reference | Pending owner; a0 is table-resolved possessor |
| 5C008 | 61F4C /620A8 | None | Pending owner; first a0 is table-resolved possessor |
| 5699C | 62030 /62050 | 43 /42 | Actual `nba97_game_period_switch_motion` primary operation |
| 5ADB8 | 62094 | None | Pending owner; low8v0 consumed |
| 6A144 | 62148 | None | Pending owner |
| 6A2E4 | 62158 | 0 | Actual `nba97_game_player_jump`; exactv0 consumed |
| 56B78 | 6218C /621D0 | 77 /68 | Actual conditional motion owner |
| 56CE0 | 6219C /621E0 /621F0 | 78 /69 /79-or70, each blend0 | Actual animation queue owner |

GAME6A2E4's special5A570 branch remains its own pending boundary. Do not invent
its return state just to let the dispatcher proceed. The play-call block also
requires actual7F3DC,35318,29258,9CB7C,31D18 and3542C, current team/controller
strategy fields, source strings/tables and stack-text effects. None of that
312-instruction block is credited by this ordinary dispatcher implementation.

## Small caller integration contract

The next live caller is GAME686B8. Its actual chain resolves selected controller
references from entity signed04 and tableFDC50, requires controller28=0, calls
8F224, applies2D2DC using the actual3F/47 mapping, calls700E4, and then supplies
its full32 result as61760a2 while preserving the earlier mapped word as a3.
The full686B8 loop and its later63B74/other boundaries are not recovered here.
No controller identity or input mapping may be fabricated from a native key.

To bind these new owners, project canonical controller/entity/global state
into the narrow views, preserve actual entity-table aliases and original
camera globals, run700E4, apply only its ordered stores, then project fresh
state and run61760 with the exact two masks and controller reference. Dispatch
the available jump/motion/queue owners synchronously and refresh the view after
every call. Leave every other reached owner explicitly pending. A6A2E4 result0
is an executed rejection, not permission to roll back its RNG.

Source countdown remains outside this scope: initial65DB0 writes FDB7C=120;
68BF8 calls7A668 and decrements only under the original audio-ready lowbyte
gate. This input module neither decreases that counter nor skips its gate.

## Verification

Both MSVC `/Od` and `/O2` builds pass57 public checks and private comparisons
against independently executed original GAMEONLY instructions:

- 8,192 ordinary/pending-route cases,6,882 synthetic mutable callee entries,
  11,731 ordered events and all373 in-scope61760 instructions.
- All65,536 prior controller halfwords against varied full32 mapped masks,
  plus4,096 standalone raw direction cases; all74+55 producer/helper instructions
  and546,133 ordered events. No successful helper hooks replace7A498.
- A separate actual61760→6A2E4→motion/queue composition uses768 cases with
  retained normalized mocap headers. It naturally reaches598 jump attempts,
  including363 accepted requests and235 executed rejections followed by
  fallback;170 cases take fallback without invoking jump. It checks2,902 actual
  child-entry snapshots, full final entity state and shared RNG, totaling
  9,850,280 entity bytes including entry snapshots. No successful semantic
  hooks replace original callees in this composition.

Ignored source exports and receipts live under
`.local/verification/native_completion/player_input/`. These proofs do not
establish cold camera-state provenance, physical input,8F224/2D2DC/686B8
completion, play calls, countdown/audio completion, possession or a playable
match loop.

# Initial court-startup roster prefix

`game_court_roster_startup.c` owns the complete 192-instruction
`479B8..47CB8` prefix and its complete required game helpers: `4781C` name
classification, `536A0` scratchpad initialization, three resident-name getters,
the `55F00/55F0C` word wrappers and the `9CB5C` BIOS string-comparison thunk.
The source denominator is 342 instructions. Production is native C99; no
instruction interpreter or retail resource bytes are shipped.

This closes the initial roster work before the previously recovered
[court-resource bridge](game_court_startup_workflow.md). It does not skip the
intervening interactive or player-packet work and does not establish natural
match entry. The prior seven bridge files remain unchanged by this work.

## Inputs and integration

Call `nba97_game_court_roster_startup` at the actual beginning of `479B8`, using
the shared retained-memory model. Its completion boundary is `80047CB8`.
Inputs include the actual 24 roster pointers at `800FC664`, their roster
records and paired names at record `+29`, resident paired names at
`800B7254/800B726C/800B7284`, the 24 writable tag records starting at
`80102C8C`, `800DCF10`, and three halfword destinations at
`800B840A/800B846A/800B84CA`.

The caller must also retain the actual scratchpad mapping at `1F800000`.
Scratchpad `+14` is an incoming required word. Its other values are written or
preserved exactly where reached; the port does not invent a zeroed scratchpad
or classify these addresses as MMIO-backed RAM. `536A0` writes the four actual
self-pointer values at `+30/+34/+38/+3C`, and the prefix writes zero at `+0C`.
No host pointer is converted into an original source address.

`nba97_game_court_roster_match` exposes the complete `4781C` owner separately,
with raw original index arithmetic and its classification in
`progress.match_result`. `nba97_game_court_roster_scratch_init` exposes complete
`536A0`. Both share the same memory, journal and budget contracts as the prefix.
They do not create replacement roster or string data.

The existing `52C20` scene/resource caller must produce the roster/body state
before this prefix runs. In particular, known-zero cold entity storage does
not provide these roster pointers, resident-name lifetime, scratchpad state,
body contexts or loader ownership. Existing body, name, marker, font and heap
owners remain separate prerequisites; this implementation does not duplicate
them or seed them with plausible defaults.

## Name classification and BIOS contract

`4781C` calls the three source getters, loads the indexed roster pointer, and
scans all four first-name strings to obtain the following second-name
addresses before making comparisons. Empty first strings still advance past
their NUL. Comparisons occur in their actual short-circuit order:

- Both names matching the first pair produce `1`.
- Both matching the second pair produce `2`.
- Both matching the third pair produce `0`, without testing the roster ID.
- Otherwise the roster pointer is reread and signed halfword ID `01C0` produces
  `3`; every other ID produces `0`.

The two later literal resident pairs have identical original bytes. They are
nevertheless distinct mutable locations and remain separate comparisons.
Deduplicating them would change behavior when their retained contents differ.
Only exact ID `01C0` matches; an ID sharing its low byte is insufficient.

`9CB5C` is the three-instruction tail thunk selecting BIOS `A0/17`, `strcmp`.
The native owner implements the required case-sensitive **equality semantics**
over retained bytes. Only zero/nonzero is consumed by the source. The bounded
BIOS contract reads a left byte then a right byte until a difference or a
shared NUL. It is read-only and never fabricates a successful comparison.
This is an explicit semantic BIOS boundary, not ROM execution, an exact BIOS
memory-read trace, or a claimed raw signed return value. Its journal event
records normalized `0` for equal and `1` for different. A semantic read refusal
reports PC `000000A0`; game-instruction refusals report the original game PC.

The independent CPU comparison executes the actual `9CB5C` thunk and uses the
same declared read-only BIOS service contract at `A0`. All reached game
helpers execute original instructions without callee-result hooks. BIOS
instruction timing, interrupts, asynchronous writers and ROM-internal stack
effects remain outside this bounded proof.

## Preserved source behavior

The prefix first clears `DCF10`, then initializes the four scratchpad
self-pointers and clears `+0C`. If bit zero of scratchpad `+14` is set, it
rereads `+14`, XORs that value with one, and writes the result into **`+4`**.
This surprising destination is retained rather than corrected to `+14`.

For every one of the 24 slots, the source stores tag `FF` and calls the full
matcher. A nonzero first result sets that slot's bit in scratchpad `+0C`, reads
its current roster pointer, writes and rereads scratchpad `+28`, and then calls
the full matcher **again**. The second result becomes the tag. The cached
pointer from before that second match determines the following byte writes.
The first and second results can differ under supported native backing
aliases; the source does not replace the second result with the first.

The cached record receives byte `63` starting at `+0E` through the source's
signed-pointer loop, ordinarily 17 writes through `+1E`. All pointer addition
wraps to 32 bits and the loop retains its signed comparison and initial store.
It is not replaced by a detached fixed-size fill.

If no first match was nonzero, the prefix skips all later rating clamps and
the three final halfword stores. Otherwise it visits **all 24** roster records.
Fields `1C/15` clamp above 96, `12/13/1A/16/17` clamp above 99, and `20` clamps
above 23. The precise conditional and unconditional roster-pointer rereads
remain in source order. It then rereads the slot tag; any tag other than `FF`,
including a second-match result of zero, receives the original overrides:
`1C/15=99`, `12/13/1A/16/17=255`, and `20=23`, with a fresh pointer read before
each store. Finally it writes halfwords `040E`, `02DD` and `042C` at the three
resident destinations.

Only the tag byte of each 32-byte tag record is written. Unused tag padding,
roster fields, scratchpad words and untouched globals preserve their incoming
bytes and knownness. No count, ID, rating or pointer is repaired.

## Ownership and refusal

Mapped regions remain fixed and cannot overlap in source address space;
native backing storage may alias. Context, progress and journal storage must
be disjoint from retained byte/knownness buffers and one another. Original
code and active-stack aliases are excluded. Source alignment is independent
of the host allocation's alignment.

Only reached bytes are checked. Canonical knownness is validated for an entire
access before unknown-data refusal. Each write establishes only its own bytes.
The access budget bounds scans and pointer loops; journal capacity is checked
before each store or comparison event. These are native bounds, not original
game limits. Refusal preserves all earlier events, stores, and partial
comparison state. It is not evidence that the original game rejects the input,
and it is not resumable or transactional. Atomic host publication must stage
all affected retained memory together.

## Verification and next boundary

The public test passes 52,509 checks in strict MSVC Debug and Release and strict
GCC C99/C++17 builds. It checks all classification outcomes, paired and
case-sensitive comparisons, third-pair ID bypass, raw wrapped indices, exact
scratchpad effects, conditional all-roster clamps, selected overrides,
untouched knownness and every journal cutoff of the selected fixture.

Each MSVC build also passes 1,158 independent original-byte cases covering all
342 owned PCs, 101,091 ordered store/comparison events and 2,733,569 original
instructions. Every retained byte and knownness value agrees, as do
classification, the prefix's selected flag, reached access counts and failure
PC/address. Cases cover randomized roster/name layouts and ratings, all event
prefixes, per-access matcher budgets, unknown fields, invalid knownness,
alignment, high-bit and empty strings, and backing aliases. An alias from
scratchpad `+28` onto the first resident pair verifies that the second match
can change while the earlier selected flag remains set.

Private evidence resides in
`.local/verification/native_completion/court_roster_startup/`. The original
GAMEONLY hash is
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
`source-audit.json` hashes each exact interval; every word is disassembled
individually, preventing silent truncation. The build/comparison receipts and
freeze bind the C/header/test/doc and independent CPU oracle.

The still-unclosed predecessor work is now exactly **675 instructions**:
`47CB8..484B8` (512) is the controller-dependent interactive `zcheat.psh`
resource/render interval, and `484B8..48744` (163) is synchronization and
ten-player packet patching. The next owner is the complete first interval,
including both its ordinary reset route and optional interactive loop. It
requires real `8F224` controller/clock behavior, `29BFC` loading,
`994F4` synchronization, `90698` release, SHPP access/upload, display swapping,
and `29BDC/AA0BC` rendering services where reached. Do not force its input
condition or `DCF10` to select the ordinary route.

Only after both intervals finish may the frozen texture-selection bridge run.
Complete `479B8`, its caller and resource services remain necessary before a
pending match handoff can become a natural gameplay entry.

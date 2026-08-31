# Owned match state and period composition

`match_runtime.cpp/.hpp` joins the accepted snapshot, immutable gameplay
resources and recovered C owners. C++ owns bounded records, player copies,
resource lifetimes and transaction publication. Game behavior remains in the
recovered C modules. There is no PS1 address space, CPU interpreter or pointer
translation in this runtime.

The owner copies current accepted player records, including their edited
ratings, height and handedness. Each side's twelve aliases resolve those
copies; no stock-roster reload occurs. Team/status/entity/controller fields
have byte-level knownness. Player/status pointer fields use separate owned
indices; opaque header references retain their original provenance. The
opponent field at entity+CC is a **halfword**, not a pointer. Its store must
leave CE/CF untouched.

`prepareMatchRuntime` applies the actual659F0 header/status clears and the
already verified63D58/655B0/65328 boundary effects. Entity and global state
arrives through an explicit `MatchRuntimeEntry`, whose default is UNKNOWN.
This is not a claim that every intervening loader or audio routine has run.
The actual2DB90 clear covers FDB4C through FE9C7, including all eleven
entities and the render-reference table. Proof that the relevant cleared
fields survive every intervening callee remains a separate entry requirement.
The native host must not invent that provenance from C++ zero initialization.

`initializeMatchRuntimePeriod` executes65DB0 with actual synchronous owners:

- 646A8 direct bindings, then both6459C/64388 chains and both644FC role helpers.
- 65140 recovery, including65070. If it requests an uncomposed649D8
 substitution, publication stops explicitly at that boundary.
- 65B18 with the actual force-reset animation chain and resolved motion headers.
- 653E8 and7066C controller selection; a required7A36C tail remains pending.
- 60EF8 render sorting,5828C/58260 phase reset, and56B78 with both real setters.

The candidate includes every mutable record, reference, scalar and callback
context. Each callback imports the coordinator's completed writes, runs its
owner, and exports its resulting state before the next source read. Only a
complete65DB0 publishes the candidate. Original divide traps, unknown inputs,
native storage guards and missing transitive owners leave the live state
unchanged. This outer transaction does not misrepresent the C owners as
rolling back their own completed prefixes.

Original startup calls65DB0 inside659F0, then calls it again from2DB90 after
659F0 returns. Preserve both calls. In particular,655B0 registers entity
addresses and writes D6; it **does not initialize entity word00 or byteD9**.
With the cleared entry state, the first646A8 can therefore bind every entity
to the first home player.65B18 consumes those existing player references
before setting entity IDs; the later646A8 repairs the normal bindings. A
second period preparation consequently can use a different handedness for
the away center. This verified ordering must not be flattened into a generic
one-pass player constructor.

After that entry sequence,68BF8 calls67468, which starts with another65DB0.
The two preparations above are the2DB90 sequence, not a claim that the full
game startup contains only two calls.

Public integration tests exercise human and CPU-only periods, both startup
preparations, quarters1 through4, current-roster ownership, actual motion39,
field widths, unknown selected-player production, source divide traps and
atomic refusal at a requested substitution. They use synthetic original-state
fixtures and synthetic resource bytes; they are not a live emulator capture.

An independent private comparison also executes84 original65DB0 runs across
42 cases, with every reached callee executed rather than replaced by a hook.
It uses the real normalizedZMOCAP and period tables, synthetic accepted
players, CPU/human assignments, raw quarters0/1/2/3/4/FFFF/8000, and options
0/4/255. All383,376 known record bytes,68 period globals, auxiliary fields,
active bindings and entity/render references match after both preparations;
every numeric original write also has correct native knownness. No tested
case requests an actual substitution. The only loader hook is the resource
read preceding the separately executed640D8 normalization. This establishes
composition under the supplied entry state, not natural loader completion.
Evidence: private `period_dependencies/runtime_verification.json`.

`initializeMatchRuntimeAttributes` composes63EDC and51ED8 after the period
state is available. It reads the actual first entity reference once and uses
the recovered physical walk, current player bindings, raw FDB64 divisor and
explicit render flag21498. Heights have their own owned eleven-entry table;
unwritten entries remain unchanged or unknown. Player byte20 is metadata[1],
while ratings start at0E. All raw values come from the current owned players,
including changes made after the accepted snapshot; no database reload occurs.

Only the complete flag21498==0 path publishes. A source rating/divisor trap
returns its exact earlier effects as a diagnostic receipt without publishing
the candidate. Unknown state also refuses publication. Nonzero21498 explicitly
requests4D9EC/35A44/38A18; those render tails are not replaced with success.
The entry flag remains unknown until a caller supplies its proven value.

The attribute bridge has a separate original-instruction comparison in
`checkpoint6/attributes-verification-{Debug,RelWithDebInfo}.json`. Each build
passes512 cases, including91 actual source divide traps,30,471 source writes,
and1,919,760 known composed record bytes. It executes63EDC and51ED8 without
callee hooks, compares all nonstack RAM and exact prefix write footprints,
and verifies that traps leave live native state unchanged. Inputs are explicit
post-period fixtures with arbitrary current player bytes, physical starts0/1,
raw aliased IDs and signed divisors. This covers119 of127 coordinator
instructions and all11 height-writer instructions; the six nonzero-render-tail
instructions and two impossible division-overflow BREAKs are outside these
composed cases. The C owner's broader proof remains documented separately.

This checkpoint does not yet connect the period owner to the user-facing
gameplay loop, draw a court, advance a possession, close every substitution
callee, execute full659F0/2DB90, or claim complete matches. The accepted
snapshot's earlier immutable stage receipt remains unchanged.

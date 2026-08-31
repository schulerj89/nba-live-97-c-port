# Team Select inherited text colors and capture contract

The later [arrow flash checkpoint](team_select_arrow_flash_workflow.md) adds
bounded native allocation history and color animation. This document retains
the earlier construction evidence and safe capture procedure; it does not
provide a universal seed for every entry route.

2026-08-30. This checkpoint closes bounded upstream text-history questions and
adds a read-only saved-RAM inspector. Four actual arrow constructions are now
observed and compared with the source. It changes no game behavior or native
color initializer. Full runtime scenarios, flash timing, GPU pixels, audio and
the remaining physical walkthrough are pending. Addresses and field offsets
below are hexadecimal; object and instruction counts are decimal.

## What the source establishes

`35984` creates the frontend text manager once, calling `29B98` at `35A24`.
The only direct destructor call is `3F7C8` at `409D8`, when leaving the frontend.
Title, ordinary Setup, Team Select and optional frontend visits share its pool.
Initialization marks the 200 nodes free; it does not initialize node+30..32
stored RGB. Heap allocation flag20 means search from the high-address end,
not clear the payload. The observed early pool at801F9C18 lies outside the
examined FE/boot CRT clear ranges and the boot executable payload. Its observed
zeros are a snapshot fact, not a recovered allocator guarantee.

The press-start loop in `35984` is a concrete earlier text producer. Each
iteration retires group1, creates its replacement at(256,30), then pumps layers
0/1/2. Input and the1800-iteration timeout determine the allocation hint. Eight
original-instruction cases preserve all200 deliberately distinct stored RGB
triples, while walking slots cyclically. Cleanup frees the remaining title but
does not reset the hint or colors. This kernel/loop fixture is not a complete
boot-to-Setup execution.

The ordinary resource path `31A88 -> 2FB00 -> 38AE0` supplies `3282C` as its wait
callback. That callback renders the previous screen and pumps text layers0/1/2.
It can advance an existing tint or retire expired text. It does not create or
set a new tint in the tested path. `31F48(0/3)` creates graphics and preserves
the entry portrait selectors of-1. Loading time therefore cannot be replaced by an
invented fixed number of text ticks.

`39574 -> 38E84` is an eight-record resource queue, not a generic text callback
queue. Direct enqueue callers supply exactly these three application callbacks:

| Producer | Callback | Effect and bounded eligibility |
|---|---|---|
|2F8F4|2F870|Resolve whole-file resource; no text effect|
|310D8|30E78|Validate/upload portrait graphics; ordinary context+0/+4 are-1|
|31630|314A0|Validate/load announcer audio; not a Team Select action|

`2F870` has another indirect call throughD9B50. Its initializer `8ACB0` installs
`8ABF0`, a trailer/CRC validator, not decompression or text production. `393F0`
does not cancel every request: it leaves pending whole-file state2 to complete
and invoke its callback, while cancelling states1/3 and removing callbacks
from active states4/5. Those surviving completions still have no text effect.
The separate `39574 -> 355A0` branch is disabled byED270=0 on fresh ordinary
entry; visits to other layouts need their own history.

There is a useful **conditional** inference: starting from a known pool whose
200 slots are free with storedRGB0, the successful direct title -> ordinary
Setup -> Team Select route, without Help or optional visits, preserves zero
arrow start colors regardless of title-loop or resource-wait count. The old
title has no active color command. Team Select's22 earlier descriptor nodes
remain occupied while the four arrows allocate, so their pulses cannot seed
the arrow slots by reuse. This does not establish that the saved early dump
followed that route, cover failure/warm/optional paths, or justify a universal
native zero initializer. In particular, the1800-iteration attract/demo timeout
does not satisfy this direct-route premise. The current native arrows remain neutral pending a
verified history policy and flash integration.

The subsequent physical demo exit reached Game Setup. A paused dump at
PC800395EC matches all eight independent FE anchors (14,764 bytes). All200
nodes are free with hint0, but50 stored RGB triples are nonzero. This is a
concrete warm-route observation, unlike the earlier empty/all-zero snapshot.
It does not identify which prior demo operation produced each byte. The exact
private baseline is `runtime-20260830/setup-before-team-2001.bin` and its field
receipt under the Team Select verification directory.

The next physical Enter reached state3. At each of four successive3D51C stops,
the debugger PC/S0 were observed and saved before copying RAM through a newly
validated backing. Only debugger F9 continuations occurred between these stops.
All four pass the inspector and an independent source comparison:

| Arrow | Observed S0 | Observed node | Slot | Stored RGB / both visible primitive pages |
|---|---|---|---|---|
|0|800C1334|801FA198|22|0,0,0 /128,128,128|
|1|800C1344|801FA1D8|23|0,0,0 /128,128,128|
|2|800C1354|801FA218|24|0,0,0 /128,128,128|
|3|800C1364|801FA258|25|0,0,0 /128,128,128|

All are in group120. The2,769-assertion comparison checks every node's lifetime
and stored RGB, all255 group heads,10 temporary/layer heads, arrow scalars and
the manager hint/page against original entry instructions seeded with the actual
Setup pool. Font geometry, allocation and GPU remain controlled boundaries.
The earlier warm-pool prediction separately passes2,165 assertions/4 cases.
These results establish this route's initial colors; they do not establish later
flash output or make every possible warm route zero.

Private originals are `runtime-20260830/arrow0` through `arrow3`, each with
`-stop.png`, `-ram.bin`, `-snapshot.log` and `-receipt.json`. The temporary3D51C
breakpoint was removed, entry completed, and the original screen showed Chicago
home/Seattle away. A subsequent paused state3 dump atPC8008ACD0 verifies all
eight FE anchors, hint28 and27 active text objects. The unobstructed screen was
observed while running; the later paused game screenshot is debugger-occluded,
so neither is claimed as a synchronized pixel comparison.

The settled entry dump also passes1,143 independent assertions against original
entry instructions and freshly compiled native placement C:22 descriptor text
nodes, four arrows, positions/offsets/groups/heads/lifetimes, and all12 value
strings. Glyph UV/CLUT on both pages identify Chicago Bulls ranks2/1/1/1/1 and
Seattle Sonics ranks1/11/4/2/3. Twelve full FE anchors match20,972 bytes. This
establishes the sampled entry fields, not inactive counters, how many ticks
elapsed, or GPU geometry/pixel equivalence. Evidence is private under
`team_select/audit_a/runtime_entry_20260830/`.

## First unknown datum and exact capture

The native renderer currently has neutral visible arrows and no general
stored start-color history. The original datum is each newly allocated
node's RGB bytes at+30..32. Visible primitive RGB128 is a different field.
The four values above are known for one route. Other routes require their own
capture or established history; do not compensate downstream for unknown values.

Use `docs/nopsx_controls.md`. Reach the original frontend with one physical
input at a time, observing each result. Never install FE breakpoints while
GAME occupies the overlay. Pause and verify FEONLY, then capture each stop at
**8003D51C**, immediately after the new arrow-pointer store at3D518. Record
debugger PC and S0 at the same stop as a complete2MiB RAM dump. The cached-arrow
return at3D520 does not establish construction.

Reacquire the backing using `tools/snapshot_nopsx_runtime.ps1`: derive a fresh
signature from local FEONLY (for example source offset2A7C8 maps to emulated
address3F7C8), request dump address0/length200000 hex, and choose a fresh ignored
output. Do not use its old Create Player defaults or a remembered host address.
The snapshot helper itself does not enforce private output or no-overwrite;
verify those paths before calling it. Keep the emulator stopped throughout
register observation and copying. A RAM-only check cannot establish synchrony.

Run the new inspector from the repository root, substituting the observed S0
and the fresh private paths; index0's record is800C1334, with stride10 hex:

~~~powershell
python tools/inspect_team_select_arrow.py .local/verification/team_select/arrow0-ram.bin --pc 0x8003D51C --s0 0x800C1334 --out .local/verification/team_select/arrow0-receipt.json
~~~

The inspector reads files only and never accesses a process, sends input or
modifies original data. It requires:

- Exactly2MiB RAM, the known full FEONLY SHA, and seven complete code anchors
  matching11,740 bytes across initialization, allocation, construction and entry.
- State3; the actual14-descriptor controller with two graphics; a valid page;
  S0 identifying one of its four unconditional, newly existing arrow records.
- Bounded, correctly aligned context/manager/pool/head pointers;200 nodes; the
  arrow at the allocator's last-chosen slot and the current-page group head.
- Fresh lifetime/flags/anchors/offsets, one glyph, and neutral RGB128 on both
  primitive pages. Stored node RGB is measured, never constrained to zero.
- A new output file under the resolved repository `.local` directory. Existing
  outputs, input aliases and public output destinations are refused.

The receipt retains source/RAM hashes, supplied PC/S0, the raw16-byte record,
64-byte node, both40-byte primitives, slot/hint/page/group, teams and clocks.
Four separate stops are required for all four initial colors. Its explicit
scope is one source-consistent construction: callers supply register evidence;
the tool cannot prove that those registers were observed or that RAM is live.
The earlier state0 FE dump is correctly refused even though its code matches.

## Supporting owners and evidence limits

These are overlapping supporting extents, not a new whole-game denominator.
All new instruction credit is0. Full extents/hashes and additional allocator,
movie, queue and hardware callees remain in the private inventories below.
Source fixture boundaries exclude real file I/O, GPU/SPU, allocation and timing
where stated; none proves an entire function's native equivalence.

| Owner | Bounded role / shared dependency | New credit / full instructions | Evidence / remaining scope |
|---|---|---|---|
|80029B98|Initialize manager used by35984|0 /142|Source stores; payload provenance pending|
|80029DD0|Destroy manager on dispatcher exit|0 /12|Direct caller/write references; other history pending|
|80035984|Fonts/legal/movie/title orchestration|0 /255|74-instruction title fragment,8 cases; full boot route pending|
|800360D4|Enter frontend dispatcher|0 /14|Caller trace; runtime pending|
|8002C004|Retire title group|0 /31|Title oracle; other groups pending|
|8002C1B4|Layer cleanup|0 /12|Separate cleanup fixture; whole route pending|
|8002C6B0|Shared node allocation|0 /515|Title/entry fixtures; general layout/lifetime pending|
|8002D348|Layer text pump|0 /171|Title/resource fixtures; full GPU/timing pending|
|80031A88|Ordinary state resource selection|0 /304|40 resource cases; failure/model paths pending|
|8002FB00|Resource teardown/wait caller|0 /76|Fixed callback traced; full audio/driver lifetime pending|
|80038AE0|Wait callback dispatch|0 /61|3282C fixed target; original wait timing pending|
|8003282C|Old-screen presentation callback|0 /223|Real text passes; graphics/GPU boundaries controlled|
|80038BD4|Music wait without text callback|0 /35|Source/fixture; full driver pending|
|80031F48|Ordinary graphics construction|0 /569|49 Setup/20 Team Select objects; other layouts pending|
|8007B79C|FE CRT clear|0 /42|Bounds exclude observed pool; BIOS/prior RAM pending|
|801E3508 (boot)|Boot CRT clear|0 /42|Bounds exclude observed pool; not FE mapping|
|800769E0|Heap setup|0 /56|Source trace; no payload-clear guarantee|
|80076AC0|Heap descriptors|0 /78|Source trace; complete allocator pending|
|800770D4|Allocation wrapper|0 /9|Manager flags traced; other callers pending|
|80077160|Allocation dispatch|0 /36|Source trace; complete allocator pending|
|800771F0|High-end allocation search|0 /231|Flag20 meaning; exact history pending|
|8008BFF4|End guard|0 /16|Writes outside payload; no initializer claim|
|8002F8F4|Queue whole file|0 /26|Queue fixture; actual I/O pending|
|8002F870|Whole-file callback|0 /33|Real callback/validator path; actual I/O pending|
|800310D8|Queue portrait|0 /85|Source trace; excluded ordinary branch|
|80030E78|Portrait completion|0 /152|Source trace only; GPU runtime pending|
|80031630|Queue announcer|0 /80|Source trace; excluded ordinary branch|
|800314A0|Announcer completion|0 /71|Source trace only; SPU runtime pending|
|80038CD8|Enqueue/replace identity|0 /107|Queue fixture; general driver behavior pending|
|80038E84|Pump eight resource records|0 /247|132 cases include callback completion; I/O timing pending|
|80039308|Cancel one identity|0 /58|Queue fixture; actual I/O pending|
|800393F0|Drain with state2 exception|0 /57|Queue fixture; actual I/O pending|
|80039574|Resource pump and separate text presentation|0 /276|Caller/source guards; full presentation pending|
|8008ACB0|Install validator pointer|0 /6|Sole writer/direct caller; no text effect|
|8008ABF0|Resource trailer/CRC validation|0 /48|Queue fixture; general resource inputs pending|

Private evidence under `.local/verification/`:

- `team_select/audit_a/resource_text_history/`:312 assertions/40 original-MIPS
  cases across states0/3,0/1/3/7 wait callbacks, and free/pulsing/neutral/retired
  text. Eight cases chain Setup's49 graphics into Team Select's20 graphics.
  File/CRC/GPU/music/queue are explicit boundaries.
- `gameplay/audit_b/team_text_pump/`:524 assertions/132 original-MIPS cases;
  original queue/drain/whole-file callback with controlled I/O/allocation.
  Portrait and announcer completions are source-traced only.
- `team_select/audit_c/frontend_text_history/`:72 assertions/8 title-loop cases,
  full initialization/allocator inventories, and independent guard review.
  A separate conditional-zero entry fixture passes7,380 assertions/36 cases;
  it is not the observed warm demo-return route.
  `warm_observed_report.md` documents the independent2,769-assertion comparison
  of all four actual arrow captures, separately from the source-only fixtures.
- `team_select/arrow_capture_guard/`:36 original-instruction construction
  fixtures accepted,33 invalid cases refused, plus four independently relocated
  misaligned-pointer refusals and CLI output-safety checks. Fixture heap/glyph
  boundaries and invented distinct colors are not live capture evidence.

No source bytes, emulator dumps, oracle implementations or reference media are
published. Existing native regression results and pending original tiers remain
separate in `team_select_workflow.md`.

This checkpoint rebuilt Debug and RelWithDebInfo:46/46 CTest tests pass in each.
The98 native captures repeat within/across both configurations; the historical
145-score/145-rank fixture still matches. Create Player retains27/27 repeated
captures,753 projected vertices,251 primary packet/order records and zero
missing sampled texels. Real save/config fingerprints are unchanged. Metadata
checks preserve all existing instruction and milestone credit.

Private regression runs: Team Select Debug `run-20260830-195013-e762c3e0`,
release `run-20260830-195009-5e26ba2e`, Create Player `run-20260830-195005`.
Build/test logs use `.local/logs/team_arrow_history_*`.

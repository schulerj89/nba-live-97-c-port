# Court texture and geometry startup bridge

`game_court_startup.c` closes two complete missing intervals of GAME
`479B8`: `48744..487B8` and `48894..48A4C`. They surround the existing
`game_court_textures.c` owner and lead directly to the existing
`game_court_resources.c` owner. The new code owns 139 caller instructions plus
the five-instruction `9C33C` and seven-instruction zero-argument `9C274` arms.
It does not duplicate the texture upload, court relocation, heap allocator,
text pools, font loader, or marker resource owners.

This is a source-exact bridge with required real loader, synchronization and
free operations. It is not complete `479B8`, actual resource arrival, natural
match entry, or a playable match. In particular, completion of either API means
only that named interval completed. No pending match handoff may be cleared
on the strength of these two intervals alone.

## Exact integration order

1. Execute the actual preceding `479B8..48744` work with its retained inputs.
2. Call `nba97_game_court_startup_select_texture`. Its two sentinel writes and
   filename selection precede the required complete `29BFC(filename,0)` call.
   `progress.loaded_resource` is the actual returned original payload address.
3. Resolve that address through actual allocation ownership and execute the
   complete existing `nba97_game_court_textures` owner. Supply the retained
   allocation and byte knownness, actual transfer backend, upload-state
   provenance and original address alignment. Do not use a host-pointer cast,
   a whole-heap view in place of the allocation, or a supplied zero-image fixture
   to claim real resource work. Preserve the existing palette/global effects.
4. Only after that entire texture loop completes, call
   `nba97_game_court_startup_select_geometry` with the same original texture
   payload address. It executes required `994F4(0)`, then `90698(texture)`,
   rereads the selector, conditionally initializes the four special-court
   packets, and calls required `29BFC(geometry_filename,0)`.
5. Pass the returned geometry payload address directly to
   `nba97_game_court_resources` with the same actual retained memory and heap.
   Its `90160/901EC` wrapper must compose the real `9027C` allocator.
6. Continue the actual caller. `479B8`'s `48D28..48D5C` epilogue only restores
   its private saved stack; it does not establish the rest of match startup.

The two stages deliberately do not retain a borrowed image-view descriptor:
the existing texture owner publishes a palette reference, whose native view
must have a stable lifetime supplied by its owner. `90698` ends the actual
texture allocation's lifetime. A surviving numeric source pointer is not a
live native image reference. The bridge neither extends a freed lifetime nor
clears that numeric value as an invented source store. Whole-frame integration
must reconcile existing texture sidecars with retained globals through their
existing adapters before subsequent consumers run.

The new service event uses the source call PC, operation, original argument and
zero load flags. Return `1` only after the operation finishes with actual
effects. There is no successful default. `29BFC` includes `941C8`, its NULL
retry, file/checksum/resize and allocator effects; acknowledging a zero returned
payload is rejected because complete `29BFC` cannot return zero. `994F4` reads
its actual SDK dispatch state and reaches its real callback. `90698` searches
the descriptor banks for the exact payload before freeing. Neither a frame
counter update nor host `free` fulfills those two service contracts. A refusal
retains service-internal effects as well as preceding bridge stores.

`game_court_startup_services.c` supplies a concrete production adapter for the
already recovered release owner. Bind `context.io` to
`nba97_game_court_startup_service_io` and `context.user` to an
`Nba97GameCourtStartupServices`. It directly invokes
`nba97_game_heap_release(..., NBA97_HEAP_RELEASE_PAYLOAD_90698, ...)` on the
bridge's same memory. The separate release journal/status retains the real
descriptor, free-list and lock effects, including partial refusal. The source
return value from free is unused here; its incoming value is explicitly
unknown rather than fabricated as a known zero. The host updates allocation
lifetimes from actual descriptor effects; a completed not-found release does
not release storage. The adapter delegates only LOAD and SYNC to its mandatory
`load_or_sync` callback. Missing services still refuse, and a failed native
release prevents the subsequent geometry load.

## Selection and packet behavior

Texture selection reads `DCF10` before either sentinel write, then stores
`FFFFFFFF` at `103508` and `FCC54` in that order. Nonzero `DCF10` selects the
source string at `260A0`, `ZDOMXEGG.BIN`. Otherwise nonzero `1EC94` selects
slot 31 of the pointer table at `B76C8`; zero `1EC94` selects the raw home word
`21D74`. No away-team read or native team-index repair is introduced. The shift
and pointer addition wrap to 32 bits.

Geometry selection rereads `DCF10` **after** synchronization and free, rather
than caching the earlier texture choice. For zero it similarly selects from
`B763C`, using `1EC94` or the raw home word. For nonzero it initializes the
special packets before selecting `260B0`, `ZDOMYEGG.BIN`. A synchronous service
can change the live selectors; these reads and store prefixes remain visible.

Both source tables have 32 inspected entries. Indices 0 through 28 name the
team-specific X/Y resources; indices 29 and 30 both use the same X/Y `ALE`
filename pointer, and index 31 uses `NEU`. This duplicate is preserved. The
public code reads the actual retained table; it does not embed a replacement
resource catalog or retail payload.

The special path initializes two 36-byte gradient quads in each of two banks
starting at `1041A4`, a 144-byte span. It makes 92 stores. `9C33C` writes only
tag byte 3 and command byte 7. The owner then writes each RGB byte separately,
calls the zero arm of `9C274`, and writes the eight XY halves in source order.
The low 24 tag bits and padding bytes 15, 23 and 31 of each packet retain their
incoming values and knownness. It does not zero a packet, create ordering links,
or infer that an initialized packet was submitted to the GPU.

The retained-memory contract is shared with text and court resources: regions
cannot overlap in source address space, native backing storage may alias,
metadata and lifetimes remain fixed, and canonical per-byte knownness is
checked only where reached. Every source write establishes only its own bytes.
Unknown reads, missing storage, invalid reached knownness, alignment, service
refusal or exhausted native budgets retain the exact earlier prefix. These are
native ownership refusals, not claims that the original game would refuse.
Code/source-stack aliases are outside this adapter. Neither entry is resumable
or transactional; atomic publication must clone all involved memory, heap and
GPU owners together.

## Caller audit and remaining complete owners

The original GAME image is SHA-256
`d95a433c24bd4996daddab43bf802463d82c25da33ca0d4fec93e0941223e3f0`.
All 1,257 words of `479B8..48D5C` were inspected individually, so an unsupported
COP2 instruction cannot truncate the disassembly. A whole-image aligned JAL
scan identifies the sole direct call at `52C20:530C0`. This static scan does not
prove there are no indirect calls.

`48D5C:48E94` calls `52C20`, whose ordinary branch has already prepared marker,
player/body/name and texture resources before `530C0`. `530B8` skips `479B8`
when `EB678` is nonzero. After its return, `530D0` loads `zdomznet.bin` with
flags zero and stores the result at `103F44`. It then returns through
`52C20` to `48D5C`, whose later camera/display setup and nonzero-render-flag
attribute call remain necessary. This is not a direct frontend-to-court call.

The unclosed predecessor within `479B8` is exactly 867 source instructions:

| Interval | Instructions | Required work before the new bridge |
| --- | ---: | --- |
| `479B8..47CB8` | 192 | Clear `DCF10`; run `536A0`; preserve the scratchpad flag reads/writes; scan 24 actual roster pointers at `FC664` using `4781C`; write tag bytes at `102C8C + i*20`; conditionally edit the original roster ratings and three resident halfwords. |
| `47CB8..484B8` | 512 | Branch on actual scratchpad `1F800018` and controller owner `8F224`; conditional `zcheat.psh` load and SHPP header edits; live interactive controller/render/upload loop; special selector writes; real resource free or ordinary scratchpad reset. |
| `484B8..48744` | 163 | Real `994F4(0)`; scan ten bits from scratchpad `1F80000C`; walk the actual `F0ED8` player contexts at stride `BCC`; patch both banks of 20 body-part packet groups and the `BC4` group using `9BF98`; preserve live counts and global stores. |

The next bounded complete owner to recover is the **192-instruction initial
roster prefix together with full `4781C` and `536A0`**. `4781C` has 103
instructions. Its `56790/567A0/567B0` getters return resident string addresses
`B7254/B726C/B7284`; it scans the two source names and the roster record's
names at `+29`, uses the `9CB5C` string comparison service, and preserves its
special roster-ID outcome. `536A0` has 26 instructions and writes four
scratchpad self-pointer words through `55F00` at `1F800030..3C`. `55F00` and
`55F0C` are actual word store/load wrappers, so their scratchpad accesses must
have explicit retained ownership. Do not treat scratchpad as zeroed MMIO.

After that prefix, close the entire conditional interactive resource interval,
then the complete packet-patch interval. The source writes `DCF10=0` at entry,
but the interactive path can set it at `4834C` or clear it at `4835C`; a native
caller cannot force it to zero to skip that work. The early `47A2C` call also
uses the value just read from scratchpad `+14` when writing `+4`; preserve that
source choice rather than normalizing the scratchpad flags.

The earlier `52C20` orchestration remains a separate full owner, not a reason
to redo existing subowners. Its `4D490` marker initializer is already recovered;
`504A8`'s name-UV tail, `50768` body-resource normalization and the name-texture
owner likewise have existing implementations. The ordinary `504A8` loader
prefix publishes its real ten-context `ZDOMTLST` allocation at `F0ED8`, loads
the home and away body resources, and invokes `50768` before its existing tail.
Those actual resource producers, plus fonts/pools, heap initialization and
sync/free/file services, must be composed in source order. Fake player contexts
or successful service stubs cannot establish arrival at `479B8`.

## Verification

`tests/game_court_startup_tests.cpp` passes 5,069 checks in strict MSVC Debug and
Release and strict GCC C99/C++17 builds. Tests cover all 32 table slots,
nonzero selectors, unsigned index wrap, all four packets' exact knownness,
post-free selector changes, every complete fixture's journal cutoff, required
service refusal, NULL-load contract refusal and reached unknown/invalid data.
Service callbacks in these tests are expressly fixtures, not real loaders.

`tests/game_court_startup_services_tests.cpp` adds 145 checks in strict MSVC
Debug and Release against the actual native release owner. It verifies all
seven ordinary release stores, descriptor membership and both neighbor links,
free-list publication, lock/unlock, untouched unknown payload bytes, and
missing-lock and journal-cutoff refusals. Its load and sync remain explicit
fixtures; its free operation executes the production native owner.

Private evidence is under
`.local/verification/native_completion/court_startup_bridge/`. Each MSVC
configuration passes 418 independent original-instruction comparisons,
covering all 151 owned PCs, 11,648 ordered stores/service events and 38,716
original instructions. Comparisons include every retained byte and its
knownness, source return boundary, filename and refusal PC/address. Cases
include actual retained source filename tables, all selector arms, wrapped
indices, every journal prefix, service mutations/refusals, unknown reads and
native backing aliases that redirect sentinel stores onto selectors/table
entries. The original `9C33C/9C274` instructions execute without hooks. Only
complete `29BFC`, `994F4` and `90698` are explicitly declared service fixtures.

`source-audit.json` records the exact caller, original interval hashes, source
call graph and filename tables. `comparison-Debug.json` and
`comparison-Release.json` bind the native source and independent CPU oracle.
No interpreter is shipped in production. The comparisons establish this
bounded bridge, not the unclosed service internals or natural gameplay.

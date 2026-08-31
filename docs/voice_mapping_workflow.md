# Original PATl mapping and SPU transfer boundary

`voice_mapping.c` owns FE `70884` mapping upload and `714B8` mapping unload. It also owns the CPU effects reached through `7E994(0)`, `7E9C8`, `7DDC8(-1)`, `6F7EC`, `7EA04`, and `7E898(1)`. These addresses belong to FEONLY; the GAME overlay has different code at the same addresses. This is a CPU and retained-memory owner, not an SPU allocator, DMA implementation, or a claim of audible announcer playback.

The owner consumes the same `Nba97VoicePatlMemory` registry as `924B4/91AB4`. Every mapping, body reference and global has its original encoded address; native pointers are never converted into source addresses. Reads/writes use the committed registry's requested-width alignment, bounds and knownness checks. Aliased storage remains live across callbacks. Refusal retains preceding writes and callbacks, including `C6D2D=1` if the source has not reached its clearing store. It is not a resumable cursor or an atomic transaction.

## Mapping and table layout

The mapping starts at tone `+40` decimal. This owner reads its channel count at `+6`; body offsets at `+1C/+20`; transfer lengths at `+24/+28`; and SPU byte addresses at `+2C/+30`. The first channel is considered for every mapping. The second is considered only when the channel byte equals two. A preexisting address other than `FFFFFFFF` skips that allocation/transfer after the table scan.

Original argument `a2` is a table, not a scalar output. Records have a twelve-byte stride. Word `+0` contains a body offset, or `FFFFFFFF` to end the scan; word `+4` contains the corresponding SPU byte address. Word `+8` is untouched here. Matching an offset copies the address into the mapping and marks that mapping offset `FFFFFFFF`, making the later unload skip that shared allocation. The scan uses an `if/else if`: a match against the first offset does not also match the second offset in that record.

`voice_mapping_table.c/.h` provides shared checked access.
`Nba97VoiceMappingTable` supplies the actual retained bytes and optional per-byte knownness without inventing a source stack address. Its supported domain is an aligned, nonwrapping original table; its native storage may alias registered allocations. Metadata and lifetime remain fixed. All four reached mask entries are checked before unknown reads or writes, including an unknown first byte followed by an invalid marker. Writes establish knownness without requiring old bytes to be known. Unvisited bytes are not scanned or initialized.

The source `919A0` caller reserves stack bytes `+10..27` before its saved registers and initializes only the first word to `FFFFFFFF` at `91A4C`. The rest must retain incoming values and knownness. Registration enforces exactly24 bytes only at the reached91A4C store,
after vacancy selection and before92628 tag reads. Invalid/full/zero-capacity
paths never inspect the table. Standalone70884 callers can supply a larger
proven extent. A bounded span refuses when an original access would leave its owned extent; it does not manufacture a terminator or execute out-of-bounds native memory access.

Aliases to executable code or the active frames of these callees are outside the supported domain. The caller's retained table is distinct from those callee frames. This owner does not synthesize register-save or executable-memory effects.

## Preserved original behavior

- `70884(NULL,...)` returns `-8` before setting the changing flag. `714B8` has no equivalent NULL guard.
- `C6D2D` is set before waiting for `C6D2C` to become zero. A native scheduling callback represents the real service/interrupt opportunity while waiting; it must not clear the flag by assumption.
- Successful allocation overwrites the table terminator with an offset/address record. The source does **not** write a new terminator. Unwritten bytes, including the third word of each record, stay unchanged. Later scans may reach unknown incoming caller bytes.
- The second transfer requests mapping `+28` bytes but compares its reported byte count against the **first** transfer length at `+24` (`70CAC`). Unequal channel lengths can therefore fail after allocating and transferring the second block. This original bug is retained and commented.
- `7EA04` clamps an unsigned requested size to `7F000`, ignores the lower transfer's raw return, and returns the clamped size. Thus a larger requested length can fail the later size comparison after a transfer. Native callback refusal is separate from this source return convention.
- Failed allocation/transfer validation does not free already allocated SPU memory, undo mapping changes, repair the table, or roll back earlier channels. The ordinary source failure path clears the changing flag and returns `-1`; native refusal/trap stops earlier with its prefix intact.
- `714B8` ignores allocator-free returns and does not clear mapping offsets or addresses. It rereads the second offset and channel byte after the first free callback. A callback or alias can affect whether the second free occurs.
- `7E9C8` returns its incoming byte address even though `7DDC8(-1)` computes a possibly rounded/shifted address unit value for `C75C4`. Consequently the two address-mismatch branches in `70884` are unreachable with the owned callee. The implementation does not invent a lower failure return to reach them.

## Required backend and shared state

The registry must map `800C6D2D` to the **same** `Nba97VoiceStopState::changing` used by the existing voice/channel owners, and `800C6D2C` to the **same** `Nba97VoiceChannels::stream_pending`. Separate copies would break the source synchronization. Stream references `E45E4/E45E9/E460C/E4624`, the SPU SDK globals below and the transfer counter at `F9600` likewise require their actual shared owners. Nothing here supplies cold-entry defaults.

The remaining operations are explicit and required:

| Boundary | What completion must mean |
| --- | --- |
| `7EC2C(size)` | Execute the actual SPU allocator over retained allocator state and return its real address or `FFFFFFFF`. Its `C7A8C` descriptor pointer, `C7A84/C7A88` limits and `7EF44` compaction remain separate owners. Do not generate sequential placeholder SPU addresses. |
| `7DC90(source,size)` | Own the source range and execute the actual transfer mode. The normal mode reads `C75C4/C75EC`, dispatches `7D9E8` commands 2, 1 and 3, and submits the retained source and count; the other mode uses `7D334`. The source count is already clamped. An asynchronous backend must retain borrowed bytes until its actual completion event. |
| `7F568(event)` | Test the real BIOS/SDK event identified by `C7678`. It is the `B0:0B` TestEvent boundary, not WinMM playback state. |
| `7E56C(address)` | Execute the real allocator release and its `7EF44` bookkeeping. There is no successful no-op substitute. |
| `7390C`, `73580` | Execute the actual stream reset and initial staging uploads for the body-NULL path. Those whole owners are not claimed here. A body-NULL request must refuse if they are unavailable. |

The owned CPU wrappers set `C7624/C75E0` to zero; calculate `C75C4` from the retained alignment/shift state `C75E8/C75F0/C75F4/C75EC`; increment `F9600`; and clear `C75F8` after transfer only when live `C75FC` is zero. `7E898(1)` retains the original `C7624==1` and `C75F8==1` shortcuts, otherwise tests the actual event until nonzero and writes `C75F8=1`. Those globals need provenance. Neither the source shortcut nor a native WinMM completion can be used as invented evidence of SPU DMA completion.

This upload lifetime is separate from source voice allocation/generation, SPU key-on/ADSR retirement, and `RecoveredWaveOutput`'s native 64-bit generation. Successful mapping upload does not allocate a voice, start PCM playback, or authorize freeing a still-borrowed native buffer.

## Composed caller API

`voice_programs`, `voice_patl_upload` and `voice_mapping` now share a borrowed
`Nba97VoiceMappingTable*`. Registration's final parameter supplies exactly24
retained incoming bytes and their knownness. The original91A4C first-word store
happens after vacancy selection and before92628 reads the tag. The request's
`mapping_table` is that SAME pointer; `argument[2]` stays zero instead of
encoding a native address. PATl forwards the same object to every tone.
Unload passes NULL. Neither upload owner creates a second table, clears later
records, or invents a terminator. The common helper validates only the four
bytes of each reached word; mapping counts the same source access once before
calling it, so step-budget/refusal accounting is unchanged.

The former `auxiliary` scalar API covered only a callback's first-word domain.
It could not safely serve70884 and is removed without a compatibility cast.
A second tone may reach unknown incoming bytes, and a third record may leave
919A0's24-byte local region. Both refuse with prior source effects intact.
They are explicit native ownership boundaries, not repairs to the source's
missing terminator or claims that the game bounds its stack scan.

The actual chain is now919A0 -> 92628 -> 924B4 -> 70884 over the same retained
registry. The allocator, transfer/event producer, PT operations and PATl play
owner remain required. No current host caller invokes these recovered
registration/upload APIs, so this correction does not imply a completed31A88
resource transition.

## Evidence

Strict private MSVC Debug and Release builds use `/W4 /WX` and pass178 mapping
checks each, plus45 program and66 PATl checks against the same compiled objects. The original-instruction comparison executes `70884/714B8` and the reached CPU wrappers, with only the listed lower boundaries and the explicit busy-wait scheduling opportunity hooked. Per configuration, 1,200 cases compare 1,819 ordered events and 152,527 original instructions, including mapping/table aliases, callback mutations, failures, refusals and alignment division traps. `70884` covers 354 of 358 instructions; its remaining four are the unreachable address-mismatch jumps. All 45 unload instructions are covered. Other uncovered wrapper branches require arguments this caller never supplies; the emulator stops a zero divisor before its explicit source BREAK instruction. Native knownness tests are safety-domain tests, not MIPS mask semantics.

Eighteen real-resource prefixes per configuration—twelve cursor samples and six CRC-validated nonempty Cool Facts slices—now match actual composed919A0 -> 92628 -> 924B4 -> 70884 execution through the first `7EC2C` call, where the missing allocator is explicitly refused. They do not fabricate an address or claim a completed transfer. Their table contains only the caller's four known sentinel bytes; the other twenty remain unknown and unchanged.

The full `Z1COOL.IDX` inventory contains 2,466 entries including the reserved slot: 1,185 nonempty PATl records and 1,281 absent entries, with reserved slot zero absent. Every nonempty record has one tone and one mapping channel, a valid body range, initial SPU addresses `FFFFFFFF`, and first-channel offset zero. Sample sizes range from 31,264 to 191,680 bytes, all below `7F000`. This inventory checks all raw headers/ranges, not all CRCs or decoded waveforms. The selected actual prefixes verify six complete CRC-bearing records. The corrected inventory receipt remains under `.local/verification/native_completion/voice_mapping/`.

Current table API build and proof receipts live in its `table_api/` subdirectory.
The full chain comparison has658 cases (640 synthetic,18 actual prefixes),
2,274 events and262,659 instructions per configuration. Adapted previous-domain
oracles additionally pass5,400 program cases (all191 direct PCs),5,400 PATl cases
(all122 direct PCs), and the1,200 mapping cases described above. Whole24-byte
table snapshots and SAME pointer identity are checked across callbacks.
For native unknown/bounds refusals, the original interpreter stops immediately
before the corresponding table instruction; it does not infer values or read
saved registers. Source behavior and native safety-domain refusal remain separate.

`table_api/freeze.json` supersedes earlier program/PATl/mapping freeze hashes
and the scalar-only callback contract. Those receipts remain untouched
historical evidence. Current public code/test/document hashes and both tested
DLLs are bound by the new receipt. No shared build, CMake, Git, host, device or
Ghidra state was changed by this work.

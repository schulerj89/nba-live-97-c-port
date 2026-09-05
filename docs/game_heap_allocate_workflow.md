# Original retained-heap allocation

`game_heap_allocate.c` owns the complete 231-instruction GAME9027C allocator plus A7098 (23-instruction basename scan), 90D40 (eight-instruction free-descriptor pop), A54BC (16-instruction allocation guard), AA06C (ten-instruction byte writer, as called with count four), and the three-instruction 9D93C BIOS thunk. Independent execution covers all 291 original instruction addresses in this scope.

The actual BIOS A0/1A name copy and complete A3074 eviction/relocation operation remain explicit synchronous callbacks. Neither has a successful default. This owner does not initialize an original heap, invent source addresses, allocate with host `malloc`, clear payloads, execute a BIOS ROM, or claim the recursive reclamation subsystem is complete. The separate heap-initialization owner establishes the source producer state.

## Memory and inputs

The API reuses `Nba97GameTextMemory`: retained original-address mappings with optional per-byte canonical knownness. Mapping metadata and lifetimes remain fixed during callbacks; byte and knownness mutations are synchronous. Source regions cannot overlap, while native storage aliases follow the existing text-memory contract. Source code/stack aliases are excluded. Native pointers are never converted to source pointer values.

Inputs are the original raw name pointer, requested size, flags and unused fourth argument. Required memory includes the selected 24-byte heap context at `103D50 + ((flags >> 8) & 15) * 24`, the descriptor-free-list head at EB688, the serial at C4A8C, the actual linked descriptors/sentinels and name string. Arenas need ownership when reached by writes such as the guard. Merely storing a payload address does not validate or initialize the described payload span.

The caller supplies a finite native access budget and journal capacity. Each reached owned CPU data access consumes one access step. Callback-internal operations belong to their owner's budget and receipt. Exhaustion is a native safety boundary, not an inserted original loop termination or repaired list. This also bounds cyclic lists and allocator retries whose callbacks fail to make progress.

## Search and descriptor effects

The name scan returns the byte after the final slash, backslash or colon. It reads the first byte twice for nonempty names; subsequent iterations reuse their end-of-loop byte load. An empty name is accepted, but unknown or unowned bytes stop at the reached source read.

The allocation size is `(requested + context[14] + mask) & ~mask`, using unsigned 32-bit modular arithmetic. The mask comes from context+8 normally or context+C with flag `0x40`. The source does not validate that it is a conventional alignment mask. With flag `0x40`, it also rounds forward search positions upward and reverse search positions downward using context+C.

Flag `0x20` selects reverse search. It is not a zero-fill flag. Forward search starts at the low sentinel and inserts before the descriptor bounding the chosen gap. Reverse search starts at the high sentinel and inserts after the lower descriptor, placing the payload at `upper_position - aligned_size`. Address comparisons are unsigned; modular gap-versus-size comparisons are signed. List fields and masks are reread at their original accesses, rather than copied to a detached list.

Once a gap is selected, 90D40 reads the free-descriptor head, reads that descriptor's +20 link, and publishes the next free head. Then 9027C reads C4A8C and stores the following fields in exact order:

| Store | Value |
| --- | --- |
| descriptor+18 | raw flags |
| descriptor+14 | requested size |
| descriptor+10 | aligned size |
| C4A8C | cached serial + 1, wrapping |
| descriptor+1C | cached old serial |

The BIOS boundary follows these stores. Its arguments are `(descriptor+4, basename, 12)`. Only after its actual completion does the source store the payload address and insert the descriptor into the doubly linked list. Both insertion variants include repeated live neighbor-link reads; callbacks and source aliases can affect later targets.

After insertion, the allocator rereads context+14. If nonzero, A54BC reads the current descriptor flags, payload address and requested size, sets flag `0x4000`, and uses AA06C to write the four-byte `BEND` marker at `payload + requested_size`. The bytes are written from the highest address downward: `D`, `N`, `E`, then `B`. The source neither clears payload bytes nor protects overlapping descriptor/global fields from this marker.

Without a guard, an ordinary successful search makes eleven owned CPU stores and one BIOS callback. With a guard, it makes sixteen stores and one BIOS callback. BIOS-internal name writes are not counted as stores owned by this module. Completion publishes a known original descriptor address, including a known zero on the source's failure return. It does not substitute the descriptor's payload address.

## Remaining callee boundaries

`NBA97_HEAP_BIOS_A0_1A` represents the actual BIOS `strncpy(destination, basename, 12)` reached through 9D93C. The original GAME contains the thunk, not the BIOS ROM implementation. The callback must perform its actual required name-copy effects or refuse. This module makes no claim about BIOS read footprints, overlap behavior, or implicit padding beyond the callback's proven implementation. Its returned register is unused and may remain unknown. Invalid knownness metadata is still rejected.

`NBA97_HEAP_RECLAIM_A3074` receives the raw flags. Acknowledged completion must include the actual reclamation effects, and the returned `v0` must be known before the original retry branch can execute. A nonzero result restarts the relevant search and rereads the current heap and list. Zero returns a NULL descriptor. An unknown result stops at 90464 or 905DC after retaining the acknowledged callback prefix. Missing or refused callbacks return `NBA97_TEXT_IO_REFUSED`.

A3074 is a 95-instruction eviction/relocation coordinator, not simple arena growth. Its context+10-zero branch reaches 90844 (53 instructions), 90714 (16) and 90D28 (six) to select and unlink a reclaimable descriptor. Its other branch can recursively invoke 9027C and A3074, move bytes through the separately recovered [complete AA468 owner](game_memory_copy_workflow.md), and unlink through 90714. The source copies the name and some flags from its terminal traversal register while using the selected descriptor for other fields; A3074 and those list helpers still need their own exact recovery. They are inventoried privately but remain outside this frozen allocation owner.

## Preserved source behavior and partial failure

The source deliberately remains observable where its behavior is surprising:

- 90D40 at 90D4C dereferences an empty free-list result at address +20 without a NULL check. The retained-memory boundary can refuse the reached access, but does not invent a descriptor or successful allocation.
- 903AC/903B0 and 90500/90504 compare wrapped gaps and sizes as signed values. Directed tests demonstrate a high-bit size passing this comparison and producing a wrapped payload address. The port does not repair this original behavior.
- Masks, descriptor lists and sentinel relationships are unchecked in the original. Native budgets and reached bounds refuse unsupported traversal rather than silently repairing links.
- The descriptor pop and serial update precede the BIOS name call. A refused call leaves that prefix; it does not return the descriptor to the free list.
- Name-call mutations can change context+14 and current descriptor fields before the later guard reads. The guard decision uses the reread field; the aligned size stays cached from before the callback.
- Neighbor links are reread after intervening stores. Aliases can redirect a later write. Independent tests include free-descriptor aliases with sentinels and heap/global storage.
- Allocation leaves untouched payload bytes and their knownness unchanged. Original source behavior is not replaced by zero-filled host storage.

Only reached bytes have their knownness validated. The entire accessed width is checked for invalid values above one, even if earlier bytes are unknown. Unknown read bytes produce `NBA97_TEXT_UNKNOWN`; unknown write destinations become known only where stored. Alignment, unowned memory and budget failures report their reached source PC/address. A journal event is reserved before each store or callback; an unacknowledged callback remains explicitly incomplete.

A stopped prefix is neither resumable nor rolled back. Host atomicity requires staging every CPU allocation and callback owner together and publishing only completion. An acknowledged callback event is a boundary receipt, not proof that its external implementation ran correctly.

## Text-pool integration and verification

The frozen text-pool initializer's 901EC/90160 wrapper can dispatch its actual 9027C event to this owner using the same retained memory and full raw arguments. Forward the returned descriptor only when this owner completes. 90160 still performs the descriptor-word-zero read; do not return the payload pointer in place of the descriptor. Keep the wrapper's actual lock-word stores and live 1029C0 reread around the allocation. The allocator itself does not duplicate those wrapper operations. Caller addresses and initial heap state still require real producer provenance.

Strict MSVC `/W4 /WX` Debug and Release each pass 32,985 public checks. These exercise both search directions and masks, actual descriptor/list/serial stores, unchanged payload knownness, guard byte order, callback mutation and refusal, raw signed/wrapped sizes, NULL free lists, source aliasing boundaries, reached canonical-knownness checks and finite prefixes.

Private `heap_allocate/verify.py` independently executes original R3000 instructions with load and branch delays. Each configuration compares 1,991 cases, 21,504 ordered owned store/callback events and 661,741 instructions, covering all 291 owned PCs. It compares every mapped final byte and knownness byte, returned descriptor, access count and refusal PC/address. The corpus includes 1,500 randomized sorted/overlapping list fixtures, raw sizes and masks, callback mutations, all reached instruction branches, metadata unknowns, alias descriptors, short journals and access budgets. BIOS A0/1A and A3074 are explicitly declared mutable fixture boundaries; no BIOS-ROM or reclaimed-heap execution claim is made for them.

Private raw source listings, dependency inventory, strict build logs, proof and frozen hashes are under `.local/verification/native_completion/heap_allocate/`. Public source/tests contain no retail binary payloads or fabricated natural-runtime heap capture.

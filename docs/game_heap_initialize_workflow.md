# Original heap initialization

`game_heap_initialize.c` owns the complete GAME `8FA6C` startup initializer and
`8FB4C` bank initializer, plus their `90CE4` free-list setup, `90D40` descriptor
pop, and `A4048/A405C/A4088` lock helpers. These are 169 original instructions.
The two `9CB7C` descriptor-name formatting calls remain required synchronous
callbacks. The module does not implement that formatter, initialize the BIOS,
allocate native storage, or complete the game's cold entry.

The API uses the same retained `Nba97GameTextMemory` regions as the text-pool
and heap-allocation owners. Each source address must have caller-established
provenance. Native pointers are never converted into invented original heap
addresses. Regions cannot overlap in source address space; declared native
storage aliases are allowed. Storage, mapping metadata and lifetimes stay fixed
throughout the call, including callbacks.

## Actual startup inputs

The original call at `299C8` supplies 220 descriptors and the arena
`[8010B61C,801FD800)`. Each descriptor occupies 40 bytes, so the payload begins
at `8010D87C`. It must not be rounded up to `8010D880`. Bank-zero alignment masks
become 15, but the initializer stores the original lower payload address.

The original `948A4/948A8` instructions establish GP as `800D79C8`. The API
accepts the proven GP value explicitly because `A4048` stores zero at `gp+274`
but returns the literal address `800D7C3C`. Those two addresses coincide for the
ordinary incoming GP; the original instructions do not require them to coincide
for every possible input. Neither value is inferred from a host pointer.

`8FA6C` does not initialize the allocation serial at `800C4A8C`. The original
GAME bytes contain zero there, below the later BSS clear's start at `800D7BB8`.
The allocator still requires the actual retained incoming serial, including
changes from earlier allocations. The host must load or carry that state; it
must not attribute a serial reset to this initializer.

## Ordered effects

The startup initializer first executes the GP-relative zero store, publishes
the returned lock address to `801029C0`, and writes one through that address.
It publishes the arena at `800C4A84`, then `90CE4` publishes the free-list head
at `800EB688`. It writes only each descriptor's `+20` next-link word. The final
link is zero; other descriptor bytes and the entire payload retain incoming
bytes and knownness.

The initializer rereads `800C4A84`, computes the payload start using the raw
descriptor count, and publishes the cached arena end to `800C4A80`. It invokes
the bank initializer with the original name address `8002802C` (`MB_RAM`),
bank-zero flags, alignments 16 and 16, and zero reclaim and guard arguments.

The bank initializer selects `80103D50 + ((flags >> 8) & 15) * 24`. It stores
both alignment-minus-one masks, the reclaim argument, and guard padding of
either zero or four. It pops the first free descriptor and invokes the actual
formatter with destination `descriptor+4`, format `80028034` (`LOW %-8s`), and
the supplied name. It then writes that descriptor's payload address, zero
requested/aligned sizes, zero previous pointer, and flags OR `8000`. Only then
does it publish the bank's first descriptor pointer.

The second pop and format call use `80028040` (`HIGH %-7s`). The second
descriptor gets the upper payload bound, zero sizes, zero next pointer, and
flags OR `8020`. The first descriptor pointer is loaded before publishing the
second pointer. It is loaded again after linking the first descriptor's next
pointer. These reads must remain live: aliases or formatter mutations can
change the later value. No detached list snapshot or consistency repair is used.

Finally, `8FA6C` rereads the two arena globals and the lock pointer. It stores
their wrapped size difference at `80109B8C`, then writes zero through the
reread lock pointer. Its incidental return is that size difference. The bank
initializer's return is the bank-context address. Both returns are meaningful
only when their respective native invocation completes.

## Preserved quirks and explicit boundaries

- `90CE4` decrements the raw count and compares it as signed. A zero or negative
  count still causes the final next-link store; it is not a successful empty
  descriptor pool. A wrapped decrement can instead request a very long loop.
- `90D40` has no empty-free-list check. A missing second descriptor is reached
  after earlier initialization and first-sentinel publication. Native bounds
  refuse the actual unowned `NULL+20` read without rolling back that prefix.
- Both formatters run before the following descriptor writes. Their output
  footprint is not restricted to the 12-byte name field by this owner. The
  ordinary padded names and trailing NUL can reach the following size field,
  which the original subsequently overwrites. No name truncation is inserted.
- Unlike the conditional lock wrapper in `901EC`, `8FA6C` invokes the final
  unlock setter even if the current lock pointer is NULL. The size publication
  occurs first. There is no invented fallback lock.
- Alignment arguments of zero become `FFFFFFFF` masks; addresses, counts and
  sizes use original 32-bit wrapping. They are not normalized to host allocation
  conventions.

Every reached four-byte operation validates all four knownness entries before
checking payload knowledge or writing. Write-only unknown bytes become known
only when the original store occurs. Malformed knownness, unknown reads,
unaligned accesses, missing regions and exhausted native access/journal budgets
stop with the completed source prefix intact. The journal records all owned
stores and both formatter dispatches, including an unacknowledged final call.
Formatter-internal effects belong to its actual owner and are not counted as
initializer stores. A stopped invocation is not resumable.

For atomic host publication, stage every mapped region and the formatter's
external state together. Then publish only a completed invocation. The C owner
itself performs neither automatic rollback nor a hidden clone.

## Allocation and text-pool integration

The resulting bank contexts and descriptor chain can feed
`nba97_game_heap_allocate` using the same retained regions. That allocator
requires actual BIOS name-copy and reclamation operations when reached.
`nba97_game_text_pools` calls the allocator through its `9027C` boundary. The
bridge must return the allocator's descriptor address; `90160` owns the
subsequent descriptor-word-zero read that obtains the payload. Returning a
payload directly would bypass an original read and break aliases and failure
behavior.

The original outer `901EC` lock operations remain in the text-pool owner.
Neither the allocator nor a bridge should add duplicate lock operations or
clear the allocated payload. Only the actual text-pool stores establish the
font and label initialization state.

## Verification scope

The public tests exercise ordinary descriptors and payload preservation,
formatter refusal prefixes, empty-list failures, canonical metadata, native
limits, changed GP, live arena/lock mutations, and alternate bank flags. A
separate original-instruction review covers the CPU owners and their composed
initializer, allocation and text-pool path with declared external boundaries.
Neither these tests nor the source comparisons establish a complete cold boot,
formatter implementation, BIOS implementation, game frame, or playable match.

Strict Debug and Release each pass 62 public checks. The independent initial
review compares 423 original-instruction cases per configuration, with 34,872
ordered events and 201,920 instructions, covering all 169 owned PCs. It compares
every mapped CPU byte and knownness byte, store/callback order, access counts,
returned value, and the exact refusal PC/address. The formatter is a declared
mutable fixture boundary. Private source listings and receipts are retained in
`heap_initialize_review`; they contain no claim of actual BIOS or formatter
implementation.

The same independent review adds 15 composed cases per configuration through
actual `8FA6C`, `9027C`, and `2E200 -> 90160 -> 901EC -> 9027C` CPU owners.
They compare 21,298 ordered events and 152,368 instructions per configuration,
including all retained bytes and knownness. The serial starts from explicitly
proven original static data. Formatting, BIOS name copy and reclamation remain
declared external operations. Unknown-serial and failed-operation cases retain
the correct prefix rather than manufacturing successful text allocation.

# Original SPU memory allocation and upload connection

`spu_heap.c` owns four complete FEONLY CPU functions: initialization at `7E940`
(21 instructions), allocation at `7EC2C` (178), free at `7E56C` (31), and
descriptor maintenance at `7EF44` (192). Their combined 422 instructions have
no unresolved lower calls. This allocator manages the original SPU memory
descriptors; it does not execute transfers or make sound.

`spu_heap_mapping.c` connects allocation and free directly to the recovered
`70884`/`714B8` upload and unload owners. It uses the exact retained memory
registry supplied by the caller, so descriptor and SDK-global changes remain
visible to all audio owners. Other operations are forwarded to a required
platform callback. With no callback, transfer, event, stream, and service
requests refuse instead of receiving fabricated successful results.

## Actual initialization and remaining startup requirements

The original `700B0` routine calls `7E6EC` before invoking `7E940` with a count
of 128 and descriptor address `800FEE50`. That earlier call reaches hardware
initialization through `7E3FC` and `7CE18`. This checkpoint does not replace or
claim completion of that hardware startup.

Given a positive signed count, `7E940` writes only the first descriptor's two
words and the table, last-index, and capacity globals. It stores the supplied
count unchanged. It does not clear the rest of the descriptor allocation.
Nonpositive signed counts return zero without touching retained memory.

The caller must provide owned descriptor storage and proven incoming SDK
state. The source's count does not establish a safe native buffer extent.
Alignment fields `C75EC/C75F0/C75F4` become 3/8/7 in the observed initialization
code. `C762C` is cleared and `C7630` becomes `FFFE` through its initialization
path; the static file's zero at `C7630` is not equivalent to that later state.
Tests that prepare these values explicitly are conditional CPU tests, not
evidence that the native game has completed hardware startup.

## Preserved original quirks

- Allocation tests the complement of the alignment mask. With mask 7 and
  shift 3, byte requests 1 through 7 round down to zero. Maintenance can then
  replace that zero-size record with the tail descriptor, leaving the tail
  flag in the allocator's raw return value.
- Reserved-space subtraction is unsigned. An excessive reserve can underflow
  and permit an allocation; no protective clamp is inserted.
- A tail at index `count-1` can write a new descriptor at index `count`.
  Native storage bounds refuse at the reached access if that extra record is
  not owned. The port does not silently reduce the original count.
- Without a spare descriptor, allocation can shrink a free block without
  recording the unused remainder. This loss is preserved.
- Maintenance skips tombstones without an inner last-index bound, retains
  stale bytes after compaction, and does not check contiguity when absorbing
  trailing free records. Live table/global aliases follow the original order
  of loads and stores.
- Free invokes maintenance even when it finds no matching address. Both
  functions retain their actual incidental return-register value rather than
  inventing a conventional success code.

The existing upload bugs remain unchanged, including the right transfer's
comparison with the left length, retained allocation on failure, missing new
table terminator, and unload's retained mapping fields. Freeing descriptor
records does not clear transferred sample bytes.

## Usage and verification limits

Install `nba97_spu_heap_mapping_invoke` as the mapping callback and pass its
bridge as context. Zero the bridge progress before a new outer operation;
its access and journal budgets accumulate across heap callbacks. Detailed
heap refusal information remains in bridge progress while the mapping ABI
reports an unavailable lower operation. A refused operation retains its
completed prefix and is not resumable or automatically rolled back.

Public tests cover initialization, splitting, release and reuse, the original
quirks above, exact refusal prefixes, shared-registry upload/unload composition,
and cumulative callback budgets. A successful transfer in a public composition
test uses an explicitly declared test backend that copies bounded bytes and
provides a test event. It is not an implementation of real SPU transfer or
playback. The missing-backend case must stop after the actual allocation.

Independent comparisons execute the original FE instructions and check ordered
stores, all retained bytes, explicit provenance annotations, access counts,
raw returns, and refusal locations. Provenance annotations are a verification
model, not metadata on PS1 hardware. Real-asset prefix checks must stop at the
next unowned transfer operation; reaching that boundary is not an audio
completion or playable-game claim.

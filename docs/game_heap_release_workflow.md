# Original heap lookup and release

`src/recovered/game_heap_release.c` owns the CPU operations at `80090618`,
`80090698`, `800906C4`, `80090714`, and `80090D28`, including the lock setters
`800A405C` and `800A4088`. These seven functions contain 90 original instructions.
This removes the lower CPU call boundary for descriptor release; it does not
establish the surrounding game's ordinary startup or resource lifetimes.

The caller supplies retained mappings for the original heap banks, descriptors,
free-list head, and lock. There is no host `free`, pointer-to-header subtraction,
replacement heap address, payload clearing, or required lower callback. A caller
that needs atomic publication must stage all affected retained state together.
Native resource, knownness, alignment, and execution limits preserve the completed
prefix; they do not represent original failure branches or allow resumption.

## Entry points and preserved behavior

- `NBA97_HEAP_FIND_90618` searches the 16 bank lists for the exact payload value.
  It skips banks with a zero high sentinel and rejects reached descriptors whose
  flags contain `8000`. The original also examines flags after an unsuccessful
  search reaches the high sentinel: if that sentinel lacks `8000`, it returns the
  sentinel even though its payload did not match. The port preserves this quirk.
- `NBA97_HEAP_RELEASE_PAYLOAD_90698` finds the descriptor and calls the original
  lock wrapper. A nonzero payload not found in the lists returns zero without
  modifying the list.
- `NBA97_HEAP_RELEASE_DESCRIPTOR_906C4` locks, unlinks the supplied descriptor,
  and unlocks. Both lock operations are unconditional once the descriptor is
  nonzero, including when the lock pointer is zero. The second load of the lock
  pointer occurs after unlinking; aliases can therefore change its destination.
- `NBA97_HEAP_UNLINK_90714` bypasses the lock and does not test the descriptor for
  zero. It updates the neighbors, clears only descriptor word zero, and pushes
  the descriptor onto the free chain. It rereads both links after the first
  neighbor store. No list consistency repair or full descriptor reset occurs.

Successful unlinking returns the old free-list head, as the original `90D28`
does. The null branches of the two release wrappers leave the incoming `v0`
unchanged, including whether that value is known. The API therefore takes an
explicit incoming value and knownness flag instead of manufacturing zero.

Released payload bytes retain their values and knownness. Other descriptor
fields, including its name, flags, serial, and previous link, also remain
incoming unless a source alias causes one of the actual stores to touch them.

## Validation

The asset-free `game_heap_release` test exercises all 16 banks, missing payloads,
the malformed sentinel behavior, both null wrapper paths, direct unlinking,
live descriptor/global aliases, and bounded refusal prefixes. It checks that
bytes and knownness outside actual stores remain unchanged. These tests use
explicit fixtures; they are not evidence of a complete ordinary game session.

Independent verification passes in both debug and optimized configurations:
1,626 comparisons against the original instructions cover all 90 instructions,
43 checks cover native metadata guards, and 108 composed cases exercise actual
initializer, allocator, release, and reallocation owners in the same retained
memory. These composed cases verify descriptor reuse and preservation of payload
bytes and knownness. The public test passes 170 checks in both configurations.

These comparisons use private source evidence. No original executable or asset is
included in the public tests. The surrounding allocator still requires actual
BIOS string-copy and reclamation operations; initialization still requires its
formatter. This release owner does not substitute successful callbacks for them.

GAMEONLY payload-size wrapper `80090D60` now provides one natural read-only
consumer of `NBA97_HEAP_FIND_90618`. The FELOAD diagnostic publishes a retained
descriptor, then the actual lookup returns it to the wrapper, which reads its
requested-size field. See [heap payload-size query](game_heap_payload_size_workflow.md).

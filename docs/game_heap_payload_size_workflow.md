# GAMEONLY heap payload-size query recovery

`src/recovered/game_heap_payload_size.c` owns all nine instructions of
GAMEONLY `0x80090D60..0x80090D83`. A fresh read-only Ghidra listing and
decompile reduce the routine to the source sequence below:

```text
descriptor = 0x80090618(payload);
return *(uint32_t *)(descriptor + 0x14);
```

The original bytes have SHA-256
`665368c63a001c084cd5c009548768ad5db5a385cad175c378e9f10f7ccdaaa0`.
Main reaches the owner at call PC `0x80029B08`, immediately after loading
`feload.bin`, and passes payload `0x80123400` in the self-driving diagnostic.
The returned requested size, `0x1410` (5136), becomes the length supplied to
the following FELOAD copy/relocation boundary. Ghidra also reports a second
source caller at `0x800A7200`; that surrounding path is not claimed here.

The child is not a fabricated size service. The diagnostic's still-fixtured
`0x800941C8` load attempt publishes one allocation descriptor into the heap
list initialized earlier by `0x8008FA6C`. The new owner then composes the
existing recovered `NBA97_HEAP_FIND_90618` operation. That real lookup takes
five mapped reads, finds descriptor `0x8010B66C`, and performs no stores. The
wrapper reads requested-size word `descriptor+0x14`, then reloads its saved
return address in original instruction order.

Compatibility preserves the unsafe source behavior. There is no null check
after lookup: descriptor zero reads low RAM address `0x00000014` instead of
returning a repaired zero or error. Descriptor addition wraps at 32 bits, the
requested-size read occurs before the live stack `ra` reload, and the child
heap lookup retains its malformed-sentinel behavior. Native missing mappings,
unknown bytes, alignment errors, callback refusal, and operation limits are
reported explicitly; those host safety boundaries are not presented as
original branches.

`tests/game_heap_payload_size_tests.cpp` covers the ordinary call, live size
and stack mutation, unknown values, callback refusal, every operation-budget
prefix, the null-descriptor low-RAM read, wrapping `descriptor+0x14`, and a
composition through the actual recovered heap lookup. `tests/game_main_tests.cpp`
proves the natural FELOAD call frame and retained descriptor chain.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers—never computer-control clicks—
and captures `heap-payload-size-before.ppm` and
`heap-payload-size-after.ppm`. They must be pixel-identical because this query
does no rendering. The JSON receipt and trace record the payload, descriptor,
requested size, child lookup accesses, and preserved quirks. These generated
frames prove the native click-through and non-rendering CPU effect; they are
not retail pixels, a court frame, or a playable possession.

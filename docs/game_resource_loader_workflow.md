# GAMEONLY resource-load retry wrapper recovery

`src/recovered/game_resource_loader.c` owns all 17 instructions of GAMEONLY
`0x80029BFC..0x80029C3F`. A fresh read-only Ghidra listing and decompile show
one synchronous operation:

```text
do {
    resource = 0x800941C8(filename, flags);
} while (resource == 0);
return resource;
```

The original bytes have SHA-256
`9534c90429813e90d899fe455f4d83c249eb738b1bc06b93be4470dd0486f9dc`.
The native source comments retain the address, byte identity, child entry, JAL
delay-slot behavior, stack layout, and exact `ra`, `s1`, `s0` epilogue order.

The wrapper is now composed at both startup uses reached by the native
diagnostic:

- loading-screen call PC `0x80029E70` passes `zloadscr.psh` at `0x800247F8`
  with flags zero;
- main call PC `0x80029AFC` passes `feload.bin` at `0x800247EC` with flags
  zero.

Each attempt crosses typed boundary `0x800941C8`; the wrapper does not invent
file, device, heap, or resource-lifetime effects. The diagnostic service
returns one known null before the loading-screen handle and two known nulls
before the FELOAD handle. Its event log therefore proves five attempt calls,
three null results, unchanged cached arguments, and eventual results
`0x80130000` and `0x80123400`.

For the following recovered `0x80090D60` query only, the successful FELOAD
service fixture also publishes one allocation descriptor into the already
initialized retained heap. That is a declared effect of boundary `0x800941C8`,
not behavior attributed to this retry wrapper. The next owner then uses the
actual recovered `0x80090618` lookup; see
[heap payload-size query](game_heap_payload_size_workflow.md).

Compatibility deliberately preserves the original failure bug. If every
attempt returns zero, the source spins forever without timeout, backoff, input
poll, or failure return. Native tests use an operation budget to stop and report
`NBA97_TEXT_LIMIT`; that diagnostic refusal is not converted into a successful
or null resource. An unknown attempt result cannot drive the source branch and
reports `NBA97_TEXT_UNKNOWN`. Successful child `v0` remains live, and mutable
stack bytes are reloaded in original order.

`tests/game_resource_loader_tests.cpp` covers immediate success, repeated null
results, bounded persistent failure, stable retry arguments, callback refusal,
unknown and malformed results, live stack mutation, unknown epilogue bytes,
all operation prefixes, and mapped-memory validation. The `game_main`
composition test proves both natural call frames and returned handles.

`scripts/verify_game_entry_visual.ps1` drives Game Setup, Team Select, and User
Setup through native recovered-input handlers—never computer-control clicks—
then executes both wrapper invocations. It writes these frames and the exact
attempt log into the diagnostic receipt:

- `resource-loader-zload-before.ppm` and
  `resource-loader-zload-after.ppm`;
- `resource-loader-feload-before.ppm` and
  `resource-loader-feload-after.ppm`.

Both pairs must be pixel-identical. That is the expected visual result: this
wrapper retries resource I/O but performs no rendering. The first successful
result subsequently enables the recovered loading-screen compositor to change
VRAM; the second enables main's FELOAD transfer. Generated handles and pixels
are diagnostic evidence, not retail resources, a court frame, or a playable
possession.

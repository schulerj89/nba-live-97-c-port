# GAMEONLY countdown UI update recovery

This recovery owns only `0x8003287C..0x80032B0F`, 660 bytes and 165 static instructions. The source is the fresh complete Ghidra listing `game_8003287c.txt`, whose instruction SHA-256 is `168714753e5981c88269049193aeb3f167d2f668938e801f0f4780f6b0442821`. The sole proven caller is the frame UI service call at `0x80032B18`, with NOP delay `0x80032B1C`, return address `0x80032B20`, and no arguments.

The owner preserves the full 32-GPR, HI, and LO machine with per-byte knownness. It reads and writes retained mapped guest memory and records exact attempted access and child-call prefixes under an explicit operation budget. The five table words use the source LWL/LWR and SWL/SWR access order. Signed gates, the fixed `0x88888889` multiply, callback-live `sp/s0/s1/s2`, the thirteen palette stores, the final cache store, and the JR NOP delay remain observable. Runtime bytes at `0x800249E4..0x800249F9` are fixture inputs; no retail table or host pointer is embedded.

Three unresolved source calls are typed boundaries:

- `0x8003066C` at `0x8003295C`, delay `0x80032960`, argument `a0=0xC9`.
- `0x80030D18` at `0x800329E8`, delay `0x800329EC`, arguments `a0=0xC9`, `a1=0x800249FC`, `a2=0x1EC`, `a3=0x14`, and stack argument `2`.
- `0x80094540` at `0x80032AE4`, delay `0x80032AE8`, arguments `a0=0x800FB5C0`, `a1=0`, `a2=0`, `a3=0x340`, and stack argument `0xF0`.

The production adapter accepts only the exact first child event emitted by `game_frame_ui_service` and requires the caller-supplied full machine, including known `ra=0x80032B20`. It publishes retained machine prefixes when a nested owner stops and promotes that nested result through the parent wrapper. Binding state is reusable across frame UI invocations.

Focused tests cover signed gate boundaries, clock equality and negative values, cache paths, partial knownness, multiply output masks, access and call budgets, exact copy and record access order, table/stack aliasing, callback-live state, typed refusals, malformed callback machines, atomic late-load failure, null-known-plane unknown-store refusal, alignment/resource/region validation, all return-address masks, JR delay behavior, and deterministic full memory and machine results. The natural test runs the actual frame UI parent through `0x80032B18` on the same retained memory and full machine, including reuse, malformed-event guards, missing-child source prefix, and invalid-machine propagation. An independent original-instruction differential passed 14,528 cases across all 165 PCs, all 34 machine words and masks, full 2 MiB RAM, callback entry state, copied-stack contents, runtime tables, aliases, and operation cutoffs.

The native self-driving capture composes this owner at the actual frame UI call on the same mapped memory. Its generated 22-byte table is copied in source order, and the inactive gate clears cache 7 to 65535 through an explicit typed text-clear completion. The receipt proves 52 instructions, 34 operations, 17 reads, 16 stores, and one callback. CPU comparison frames are identical; the visible frontend remains User Setup. Focused tests pass 182 checks, natural-caller tests pass 12, and the complete asset-free suite passes 365 tests.

Gameplay shown: **BLOCKED**. The recovered routine computes and submits the countdown record, but the typed text and upload services are not rendered UI implementations. The result has no direct visual artifact until those dependencies are recovered and composed.

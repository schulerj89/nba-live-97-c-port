# Camera remainder gate recovery

`game_camera_remainder_gate.c` owns the complete GAMEONLY routine
`0x8007A468..0x8007A497`: 48 bytes and 12 instructions. The fresh Ghidra
listing `game_8007a468.txt` has instruction SHA-256
`23f32766a2cfbb7a3a6a307a53bd48082d5dc9b6230fc612f6d59302b90f39f6`.
Its sole caller is the recovered camera elapsed dispatcher at `0x80079978`,
with NOP delay `0x8007997C`, return address `0x80079980`, and no arguments.
The gate is a leaf and allocates no stack frame.

The owner loads the signed camera source at `0x800FC9AC` and copies it to V0
in BGEZ's delay. Negative values add `0x7FF` with 32-bit wrap before an
arithmetic right shift by eleven, matching signed division by 2048 truncated
toward zero. The source subtracts that reconstructed multiple, adds 50 with
wrap, and evaluates unsigned `< 101` in JR's delay. Its Boolean is therefore
one exactly when the signed remainder lies within -50..50. V1 retains the raw
source word and SP, RA, HI/LO, and every other GPR remain source-faithful.

The strict C99 owner carries all 32 GPRs, HI/LO, and per-byte knownness.
Bytewise carry completion preserves exact known bytes for the negative bias
and final addition. Explicit unsigned sign fill implements SRA portably. The
remainder and predicate consume only the known sign and low eleven bits, so a
bounded 2,048-value completion preserves every invariant result byte without
inventing knowledge from ignored high bits. An unknown sign stops only after
the BGEZ delay has copied the partial source to V0. The SLTIU Boolean similarly
executes before unknown or unaligned RA is refused.

Mapped little-endian memory validates regions, overlap, 32-bit address bounds,
known planes, and journals before returning an atomic source load. The load is
the routine's only operation-budget boundary. The routine never writes guest
memory and uses no retained table or asset.

`game_camera_remainder_gate_adapter.cpp` claims the actual parent event by its
probe kind, call PC `0x80079978`, delay `0x8007997C`, entry `0x8007A468`,
invocation 1, argument count 0, and known RA `0x80079980`. Any identifying
field claims that boundary before exact validation. Its wrapper composes the
already recovered CK state lookup at `0x8007999C` through CK's public adapter,
so CO's zero result reaches the real lookup with the same full machine and RAM.
The parent's dynamic service remains an exact synthetic typed prerequisite.

The focused executable performs 628 always-active checks. It covers source 0,
`+/-1`, `+/-49`, `+/-50`, `+/-51`, `+/-2047`, `+/-2048`, `-2049`, signed
32-bit extrema, low-remainder equivalence with discarded high bits, exact
negative bias wrap, all 16 source knownness masks, partial remainder and SLTIU
knowledge, the unknown-sign delay, the load budget prefix, malformed later
known bytes, unmapped and null memory, overlap, wrapping, `SIZE_MAX`, null
journals, all 16 RA masks, unaligned RA after the SLTIU delay, untouched live
machine state, unchanged RAM and known planes, repeatability, and null inputs.

The natural executable performs 32 always-active checks through the actual CH
caller. A gate result of one reloads and publishes the cached camera state. A
zero result calls the actual CK owner and publishes its generated table word.
The zero indirect gate proves dynamic-service, CO, and CK event order. Tests
also cover every event and machine guard, direct binding reuse, wrapper reuse,
prerequisite refusal before CO, CO budget and unknown-source prefixes, missing
source mapping, malformed source knownness, an uncertain returned predicate,
CK budget failure after CO, unchanged publication on failures, and null
arguments. Both suites generate heap-backed 2 MiB fixtures at runtime
without assets. They pass strict C99/C++17 Clang
`-Wall -Wextra -Werror -pedantic-errors` and MSVC `/W4 /WX` builds.

Production dependencies are the shared mapped-memory/full-machine types, the
existing recovered `game_camera_elapsed_dispatch` parent, and the existing CK
`game_camera_state_lookup` owner and adapter. The parent's dynamic target
remains a typed dependency outside CO.

Gameplay shown: **NO - no direct visual effect**. The routine returns a camera
state predicate. Visible camera motion requires its parent and downstream
camera/rendering work in a running match.

Manager review and independent raw-instruction differential pass 16,384 cases
across all 12 PCs, all 34 machine words and output masks, full 2 MiB RAM, signed
threshold edges and every load-budget prefix. The native camera capture now
requests an explicit second elapsed dispatch using the actual first dispatch's
full return and the same retained cache. Source 0 keeps cache 42 with gate 1;
source -256 returns gate 0 and composes the existing lookup owner before
publishing 42. These are diagnostic invocations, not advancing match ticks.

The self-driving native-input verifier records both cases in ignored
`camera_remainder_gate_verified.json`. CPU checkpoint frames have matching
SHA-256 `391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`;
the visible frontend remains User Setup. Gameplay shown: NO - no direct visual
effect. Focused 628 checks, natural 32 checks, strict Clang/MSVC, the complete
377-test CTest suite and all progress/metadata freshness checks pass.

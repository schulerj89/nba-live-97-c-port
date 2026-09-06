# GAMEONLY actor collision response recovery

## Evidence

The owner recovers `0x8005F3BC..0x8005F887` from the fresh GAMEONLY listing
`game_8005f3bc.txt`. The listing reports 1,228 bytes and 307 instructions with
instruction SHA-256
`d2b867cd620f32dcf2beac1d853b0a6d6883441b361f0b627ef5d509b78cf01e`.
Its sole source caller is the recovered opponent-contact owner at
`0x8005F92C`; the source return address is `0x8005F934`.

The implementation follows the listing as readable, source-addressed basic
blocks. It retains all 32 GPRs, HI/LO, per-byte knownness, the wrapping live-SP
frame, exact memory access prefixes, all branch and JAL delays, both source DIV
BREAK guards, and the restored-RA JR delay. No opcode table, instruction image,
generated corpus, or runtime asset is used.

## Native boundaries

The full-machine callback exposes all twelve source call sites. The geometry
adapter composes the established `nba97_game_selection_distance` owner only for
`0x8005F424 -> 0x8007066C`, after checking the exact PC, delay PC, entry,
argument count, RA, and fully known A0/A1 mapping. It reproduces that owner's
source-proven A0/A1 absolute-value and V0 distance clobbers. The resolver,
angle, and four animation routines remain typed full-machine child contracts
because this source range does not prove narrower native ABIs for them.

The parent adapter accepts only the natural opponent-contact event
`0x8005F92C -> 0x8005F3BC`, its NOP delay at `0x8005F930`, two arguments, and
known RA `0x8005F934`. A failed child retains its exact prefix machine instead
of substituting a native result.

## Runtime checks

The focused executable performs 143 always-active checks. These cover positive
and zero distance; unknown arithmetic predicates; incoming and rejected motion;
normal and tangent projection; clamp inputs around -64, -63, 0, 63, 64 and 65;
factor-table indices 0 and 31 with signed factors; the resolver's eight
arguments and four stack words; all twelve call PCs, delay PCs, RAs and argument
counts; both velocity and animation paths; angle windows 0, 200, 201, and 0x3FF;
E2 writes and the both-zero/height-read quirk; state, motion, and DA gates;
ID-byte truncation and `0x800FDB88`; refusal at every callback site;
invalid-machine retention; callback-live S0/S1/SP relocation and HI/LO;
partial DIV and MULT known-byte prefixes, including zero divided by a partially
known but provably nonzero distance; every operation budget prefix on the
ordinary path; malformed known bytes; missing, overlapping and unaligned
mappings; unknown SP; a wrapped `SP=0x40` frame; unavailable-knownness stores
that leave bytes unchanged; unknown restored RA after the JR delay; and geometry
adapter guards.

The integration executable performs 11 always-active checks through the actual
recovered opponent-contact parent. It verifies the natural call metadata and
RA, full AR completion, native geometry plus typed resolver composition,
contact publication, both nested return addresses, exact nested refusal
prefixes, and invalid-event rejection without machine mutation.

Validation commands use the Visual Studio LLVM tools and no runtime assets:

```text
clang -std=c99 -Wall -Wextra -Werror -pedantic-errors ... game_actor_collision_response.c
clang -std=c++17 -Wall -Wextra -Werror -pedantic-errors ... game_actor_collision_response_tests.cpp
clang -std=c++17 -Wall -Wextra -Werror -pedantic-errors ... game_actor_collision_response_integration_tests.cpp
```

## Dependencies and presentation

Production dependencies are the shared mapped-memory/full-machine types,
`nba97_game_selection_distance`, and the recovered opponent-contact event
contract. The unresolved resolver, angle, and animation children must later be
bound to their complete owners without changing this routine's event metadata
or machine semantics.

Gameplay shown: NO - no direct visual effect. This is CPU collision response
until a native advancing court/player loop exercises the real game path.

Manager validation passed 143 focused checks, 11 natural-caller checks, strict
C99 compilation, and all 283 asset-free CTests. The private original-instruction
comparison passed 6,309 cases over full 2 MB memory, all 34 registers, callback
entry machines, signed boundaries, typed impulse/angle effects, and stack
relocation. It visited all 299 reachable PCs. Eight retained DIV-guard PCs
(`0x8005F444`, `0x8005F454`, `0x8005F458`, `0x8005F45C`, `0x8005F478`,
`0x8005F488`, `0x8005F48C`, `0x8005F490`) cannot execute after the positive
distance gate; no artificial entry is counted as source-path coverage.

The native receipt composes the actual opponent-contact and distance owners
with a typed impulse fixture. It records 51 operations, 30 reads, 19 stores,
two callbacks, contact IDs `[9,120]`, and contact flag 1. Before/after CPU frame
SHA-256 is identical:
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
The frontend remains User Setup, with no advancing match or tip-off claimed.

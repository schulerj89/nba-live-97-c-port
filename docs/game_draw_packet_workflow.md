# GAMEONLY draw-packet recovery

## Boundary and evidence

This owner translates only GAMEONLY `0x8009A344..0x8009A5E7`, 676 bytes and 169 instructions. The fresh Ghidra export is `.local/evidence/tipoff-recovery/game_8009a344.txt`; its embedded instruction-corpus SHA-256 is `a39d124d77f8bebd819dbe4335dba2449efea31d5eeb659df7d165572b0e0da5`.

The two known callers are `0x80099B20` and `0x80099BF0`. An ownership search found only the decomp inventory entry and no earlier full owner. The adjacent packet-word helpers remain separate typed boundaries.

## Source behavior

The routine allocates a wrapping 0x28-byte frame, saves `s0`, `s1`, and `ra`, captures the environment in `s0` and packet in `s1`, and invokes five packet-word helpers. It stores their results at packet offsets 4, 8, 12, 16, and 20. The fifth JAL assigns `ra` before its delay slot stores the fourth result at offset 16. The fifth child therefore sees that store and can replace `v0` for the following offset-20 store. The fixed sixth word at offset 24 is `0xE6000000`.

The byte at environment offset 24 controls the optional background primitive. The branch delay always sets `t0=7`, including an unknown-predicate refusal. A zero flag writes count 6 and enters the epilogue. A nonzero flag copies x, y, width, and height into four live stack halfwords. Negative widths and heights clear separately. Nonnegative dimensions clamp against the signed global halfwords at `0x800C55C4` and `0x800C55C6`, each minus one. Signed negative global limits retain the original wrap behavior when the clamped value is later stored as a low halfword.

If x or the clamped width is not divisible by 64, the routine appends a `0x60BBGGRR` rectangle. It subtracts the signed environment offsets from x and y before storing the coordinate word, writes the clamped extent word, and later restores those offsets only in its stack temporary. If both values are 64-aligned, it appends `0x02BBGGRR`, the original coordinate word, and the clamped extent. Both branches write count 9. RGB bytes are loaded in source order B, G, R.

All loads, stores, stack aliases, and environment aliases execute sequentially. Later reads can observe earlier writes. The owner never snapshots the environment or packet. `v0` finishes as live `t0-1`; `ra`, `s1`, and `s0` reload through callback-live `sp`; `sp` advances by 0x28; and an unknown restored `ra` refuses only after the complete epilogue.

## Typed children

| Call PC | Target | Arguments | Delay slot |
|---|---|---:|---|
| `0x8009A364` | `0x8009A644` | 2 | `nop` |
| `0x8009A39C` | `0x8009A710` | 2 | `sra a1,a1,16` |
| `0x8009A3B0` | `0x8009A7DC` | 2 | `nop` |
| `0x8009A3C8` | `0x8009A5E8` | 3 | `nop` |
| `0x8009A3D4` | `0x8009A824` | 1 | `sw previous-v0,0x10(live-s1)` |

Each callback receives the full 32-GPR, HI, and LO machine after JAL assigns `ra` and after the delay slot completes. It may mutate all machine state and mapped memory. Returning exactly 1 means the original child boundary completed. Refusal and malformed returned machines preserve the reached prefix without fabricating a child result.

## Native mapping and BD composition

Guest pointers remain `uint32_t` addresses resolved through validated, nonoverlapping mapped regions. Each little-endian byte carries independent knownness. Loads validate all consumed knownness bytes before replacing a destination. Stores to a region with `known == NULL` require every consumed source byte to be known and otherwise leave all target bytes unchanged. Unknown comparisons preserve the three always-zero bytes of MIPS Boolean results.

`nba97_game_draw_packet_from_draw_environment` is deliberately narrow. It claims the frozen BD packet event only when kind and entry both identify `0x8009A344`, call PC/delay are `0x80099B20/0x80099B24`, argument count is two, and full-known `ra` is `0x80099B28`. A match on either assigned kind or assigned entry prevents fallback from accepting malformed metadata. Valid execution publishes every child-machine prefix back to BD, including nested limits and faults. Unrelated BD events can use the typed fallback.

The natural test runs the actual frozen `nba97_game_draw_environment` owner with this adapter. It verifies the completed background packet, BD's later packet tag and submission, HI/LO preservation, and a zero-budget nested failure whose child frame mutation remains visible to the parent.

## Validation

The asset-free, always-active focused suite covers no-background, offset, and aligned paths; exact five-call ordering and arguments; the fifth JAL delay store; RGB order; signed negative sizes and signed global extrema; x/width modulo-64 selection; all no-background, offset, and aligned operation-budget cutoffs; child refusal and malformed-machine prefixes; callback-live `s0/s1/sp/t0/HI/LO`; packet/frame and packet/environment aliases; mapped and unknown pointers; malformed late knownness; immutable unknown stores without knownness backing; truncated deterministic journals; untouched GPRs; and an unknown restored `ra` after epilogue completion.

Strict Clang C99 and C++17 builds are required. Manager-owned differential evidence compares the recovered source with the original 169 instructions over full memory, all 32 GPRs, HI/LO, callback entries, budgets, aliases, and signed branch cases.

## Classification

Gameplay shown: **NO - no direct visual effect**. This CPU routine prepares a packet in guest memory. A visible result requires later submission and GPU rendering.

Manager natural integration also composes the recovered BIOS trampoline after
the actual draw owner submits the built packet, checks the 92-byte copied
environment and preserves T1/T2. The native fixture composes the same scene,
display, draw, packet, video, GPU-control and BIOS owners on one mapped memory.
Its first packet omits a background; its second emits an aligned RGB fill.
Five packet-word helpers, submission, MMIO consumption and BIOS transfer remain
explicit synthetic services. Independent original comparison passed 6,416 cases
across all 169 instructions after the final portability and signed-bound fixes.
It checks full memory, all 34 machine words, callback entries, budgets zero
through 69, all five calls, aliases and mutable stacks/registers.

Manager verification passed 2,928 focused checks, 28 natural-caller checks and
all 309 asset-free CTests. Strict C99/C++17 and progress/recovery/instruction/
roster freshness checks passed. Native run
`.local/verification/team_select/game-entry-20260906-032243-8874b580`
recorded 98 scripted states, two complete packets and four BIOS-trampoline
copies. The no-background packet returned count six; the aligned-fill packet
returned nine, including RGB command 0x02332211 and the original rectangle.
Both CPU-only hashes were
`391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d`.
User Setup remained displayed; packet preparation did not advance a match.

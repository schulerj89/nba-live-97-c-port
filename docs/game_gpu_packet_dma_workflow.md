# GAMEONLY GPU packet DMA-start recovery

## Boundary and evidence

`nba97_game_gpu_packet_dma` owns only GAMEONLY `0x8009B1F8..0x8009B243`, 76 bytes and 19 instructions. The fresh Ghidra listing is `.local/evidence/tipoff-recovery/game_8009b1f8.txt`; its instruction SHA-256 is `e1832abf40def80b73adbb94364f1f0d72d2a093be7e33bbe156a68e6f987770`.

The direct caller is `0x8009AC58`. The routine is also the computed graphics-submission target selected through `0x800C5590`, with argument references at `0x80099AA4`, `0x80099B4C`, `0x80099C28`, and `0x80099878`. The ownership audit found data references, typed dispatch references, and capture setup, but no earlier complete owner.

## Source behavior

The leaf alternates four pointer loads with four port stores:

| Load PC / global | Store PC / value |
|---|---|
| `0x8009B200` / `0x800C5694` | `0x8009B208` / `0x04000002` |
| `0x8009B210` / `0x800C5698` | `0x8009B218` / raw `a0` |
| `0x8009B220` / `0x800C569C` | `0x8009B228` / zero |
| `0x8009B230` / `0x800C56A0` | `0x8009B238` / `0x01000401` |

Each pointer is reloaded after the preceding store. If a mapped port aliases later pointer-table storage, the later load observes the earlier write. The owner therefore does not snapshot the four pointers.

The packet value in `a0` is written as raw data. Zero, unaligned values, KSEG values, and `0xFFFFFFFF` are not dereferenced or validated as packet pointers. Partial per-byte knownness is stored exactly when the target mapping has knownness backing. A partial value refuses atomically when the target has `known == NULL`.

Only `v0` and `v1` change. `v0` ends with the fourth loaded port address and `v1` ends as `0x01000401`; all other GPRs and HI/LO pass through. The `v1=0x01000000` LUI executes before the third zero store, so a store fault retains that register prefix. An unknown live `ra` refuses at `0x8009B23C` only after all eight mapped accesses and all four stores.

## Native memory and hardware boundary

Global and port locations remain validated `uint32_t` guest addresses. Word access requires four-byte alignment and a complete mapped range. Loads validate all four knownness bytes before replacing `v0`; malformed late bytes therefore leave the prior LUI value intact. Stores validate target metadata and unknown-store support before changing any byte. Successful accesses appear in an ordered journal with source PC, address, width, value, knownness, kind, and one-based operation index.

The mapped port writes are the complete CPU-visible effects owned here. The routine does not consume the packet, advance DMA, emulate GPU state, or render pixels.

## Natural graphics-submit composition

`nba97_game_gpu_packet_dma_from_graphics_submit` composes the frozen BI owner only when the dynamic target is `0x8009B1F8`. It then requires call PC `0x8009B3A8`, delay `0x8009B3AC`, full-known `ra=0x8009B3B0`, kind `INDIRECT`, and two arguments. Any event naming the assigned entry is claimed and strictly validated, so a wrong kind or malformed metadata cannot reach an accepting fallback. BI's `INDIRECT` kind is generic: another dynamic target remains unresolved and routes to the typed fallback without invoking this owner or producing DMA effects.

A valid nested execution uses the same mapped memory and all 34 machine words. Every successful or failed nested prefix is returned to BI. Because BI's callback contract reports completion as Boolean, a nested limit or fault becomes BI `IO_REFUSED`; the precise nested result remains in `binding.result`. The natural complete fixture executes the actual BI direct path, performs the eight DMA accesses, then verifies BI's last-function, packet, and callback-argument stores and epilogue. The failure fixture proves that a stopped DMA blocks those later BI stores while retaining JAL `ra`, the allocated BI frame, and completed port writes.

## Validation

The asset-free always-active focused suite covers all four pointer loads and stores with exact journals; raw zero, extreme, and unaligned `a0`; partial `a0` data and knownness; all 32 GPRs plus HI/LO; each operation budget from zero through seven; each unknown pointer; arbitrary mapped port addresses; pointer-table and stack-like aliases; unaligned and unmapped globals/ports; malformed late loads and stores; atomic refusal without knownness backing; unknown `ra` after the fourth store; invalid machine, map, and journal contexts; journal truncation; and deterministic replay.

The natural suite compiles the frozen BI owner read-only and covers completed direct submission, nested failure-prefix promotion, mapped port effects, BI's following stores and epilogue, full-machine preservation, and exact adapter guards.

## Classification

Gameplay shown: **NO - no direct visual effect**. This routine programs mapped DMA registers. Visible output requires hardware DMA consumption and GPU rendering.

Manager native integration composes the scene, display, draw, packet, both
draw-area commands, video query, GPU command, BIOS trampoline, graphics-submit
and DMA-start owners. Two direct packet submissions program mapped GP1 and
GPU DMA MADR/BCR/CHCR at 1F801814/1F8010A0/1F8010A4/1F8010A8. Packet addresses
80021F64 and 80021F08 are published in order. The mapped device fixture does
not consume DMA packets; scheduler/critical services and three packet helpers
remain typed synthetic dependencies. The user-visible screen is User Setup.
Independent original comparison passed 18,432 cases across all 19 instructions,
full 34 machine words and masks, 2MB RAM plus a mapped port and knownness,
all argument masks, pointer/table aliases, budgets0..8 and stopped prefixes.

Manager verification passed 200 focused checks, 31 natural checks, all 317
asset-free CTests and progress/recovery/instruction/roster freshness checks.
Native capture game-entry-20260906-035810-689ff8a8 proves two eight-access DMA
executions and the subsequent graphics-submit publication and environment copy.
CPU before/after frames retain SHA-256
391c073b39664372b8277dcd1f82c30d946fc96300959d1bd09fe794f625d58d. The ignored
User Setup screenshot provides local UI evidence; no advancing match is claimed.

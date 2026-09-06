#ifndef NBA97_GAME_GPU_PACKET_DMA_H
#define NBA97_GAME_GPU_PACKET_DMA_H

#include "game_match_clocks.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Nba97GameMatchClocksWord Nba97GameGpuPacketDmaWord;
typedef Nba97GameMatchClocksMachine Nba97GameGpuPacketDmaMachine;
typedef Nba97GameMatchClocksAccess Nba97GameGpuPacketDmaAccess;

typedef struct Nba97GameGpuPacketDmaContext {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameGpuPacketDmaMachine machine;
  Nba97GameGpuPacketDmaAccess *access_journal;
  size_t access_journal_capacity;
} Nba97GameGpuPacketDmaContext;

typedef struct Nba97GameGpuPacketDmaProgress {
  size_t operations;
  size_t accesses;
  size_t reads;
  size_t stores;
  size_t access_events;
  uint32_t stopped_pc;
  uint32_t stopped_address;
  Nba97GameGpuPacketDmaMachine machine;
  uint8_t completed;
} Nba97GameGpuPacketDmaProgress;

/*
 * PS1 SUBROUTINE
 * Program: GAMEONLY
 * Address: 0x8009B1F8
 * Range: 0x8009B1F8..0x8009B243 (inclusive)
 * Source size: 76 bytes / 19 instructions
 * Evidence: fresh Ghidra game_8009b1f8.txt; instruction SHA-256
 * e1832abf40def80b73adbb94364f1f0d72d2a093be7e33bbe156a68e6f987770
 *
 * Purpose: Start GPU linked-list packet DMA by programming four mapped port
 * words in source order.
 * Inputs: Full 32-GPR/HI-LO machine; a0 is the raw guest packet address;
 * globals 0x800C5694, 0x800C5698, 0x800C569C, and 0x800C56A0 contain the four
 * mapped port addresses; ra is the live return target.
 * Returns: v0 retains the fourth loaded port address, v1 is 0x01000401, every
 * other GPR and HI/LO is preserved, and live ra is consumed by JR.
 * Guest memory: Alternating pointer loads and port stores write 0x04000002,
 * raw a0, zero, and 0x01000401; later pointer loads observe earlier aliases.
 * Calls: None observed.
 * Original quirks: a0 is stored as raw bits without dereference or alignment
 * validation; the third LUI occurs before the zero store; an unknown ra
 * refuses only after all four stores.
 * Native mapping: Global and port addresses use validated uint32_t mapped
 * regions with per-byte knownness; the owner does not simulate DMA or GPU
 * consumption and never casts a guest address to a host pointer.
 */
int nba97_game_gpu_packet_dma(Nba97GameGpuPacketDmaContext *,
                              Nba97GameGpuPacketDmaProgress *);

#ifdef __cplusplus
}
#endif
#endif

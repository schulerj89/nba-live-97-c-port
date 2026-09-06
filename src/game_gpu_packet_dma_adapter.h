#ifndef NBA97_GAME_GPU_PACKET_DMA_ADAPTER_H
#define NBA97_GAME_GPU_PACKET_DMA_ADAPTER_H

#include "recovered/game_gpu_packet_dma.h"
#include "recovered/game_graphics_submit.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameGpuPacketDmaGraphicsBinding {
  size_t operation_budget;
  Nba97GameGpuPacketDmaAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameGraphicsSubmitIo fallback;
  void *fallback_user;
  Nba97GameGpuPacketDmaProgress progress;
  int result;
  size_t invocations;
  size_t fallback_invocations;
} Nba97GameGpuPacketDmaGraphicsBinding;

void nba97_game_gpu_packet_dma_graphics_binding_init(
    Nba97GameGpuPacketDmaGraphicsBinding *, size_t operation_budget,
    Nba97GameGpuPacketDmaAccess *, size_t access_journal_capacity,
    Nba97GameGraphicsSubmitIo fallback, void *fallback_user);

int nba97_game_gpu_packet_dma_from_graphics_submit(
    void *, const Nba97GameTextMemory *, const Nba97GameGraphicsSubmitEvent *,
    Nba97GameGraphicsSubmitMachine *);

#ifdef __cplusplus
}
#endif
#endif

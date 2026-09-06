#ifndef NBA97_GAME_ORDERING_TABLE_DMA_ADAPTER_H
#define NBA97_GAME_ORDERING_TABLE_DMA_ADAPTER_H

#include "recovered/game_ordering_table_dma.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameOrderingTableDmaBinding {
    size_t operation_budget;
    Nba97GameOrderingTableDmaIo io;
    void* user;
    Nba97GameOrderingTableDmaAccess* access_journal;
    size_t access_journal_capacity;
    size_t invocations;
    size_t completions;
    size_t fallback_callbacks_completed;
    Nba97GameClearOrderingTableEvent event;
    Nba97GameOrderingTableDmaProgress progress;
    int result;
} Nba97GameOrderingTableDmaBinding;

/* Bind only the actual 0x800999BC dynamic backend event whose live target is
 * 0x8009A97C. The same retained memory and complete parent machine cross the
 * boundary, including a0/a1 and JALR's source-proven ra=0x800999C4. */
int nba97_game_ordering_table_dma_from_clear_ordering_table(void*,
    const Nba97GameTextMemory*,
    const Nba97GameClearOrderingTableEvent*,
    Nba97GameClearOrderingTableMachine*);

/* Execute the complete existing 0x80099960 owner with its exact DMA backend.
 * The optional parent callback remains responsible for the dynamic debug
 * service and for any nonmatching dynamic backend target. */
int nba97_game_clear_ordering_table_with_dma(
    const Nba97GameClearOrderingTableContext*,
    Nba97GameOrderingTableDmaBinding*,
    Nba97GameClearOrderingTableProgress*);

#ifdef __cplusplus
}
#endif
#endif

#ifndef NBA97_GAME_MATCH_INITIALIZE_ADAPTER_H
#define NBA97_GAME_MATCH_INITIALIZE_ADAPTER_H

#include "recovered/game_match_initialize.h"
#include "recovered/game_match_session.h"
#include "recovered/game_memory_zero.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchInitializeAdapterProgress {
    Nba97GameMemoryZeroProgress memory_zero;
    int memory_zero_result;
    size_t memory_zero_invocations;
    size_t unresolved_callbacks_completed;
} Nba97GameMatchInitializeAdapterProgress;

/* Execute the initializer while routing only its proven 0x800A3A74 call to
 * the existing zero-fill owner. All other calls retain context->io/user. */
int nba97_game_match_initialize_with_zero(
    const Nba97GameMatchInitializeContext*, size_t zero_operation_budget,
    Nba97GameMatchInitializeProgress*,
    Nba97GameMatchInitializeAdapterProgress*);

/* Build the live register subset proved by match-session's 0x8002DA7C event.
 * Registers unavailable through that older caller API remain explicitly
 * unknown; the initializer overwrites every such register it depends on. */
int nba97_game_match_initialize_registers_from_session(
    const Nba97GameMatchSessionEvent*, Nba97GameMatchInitializeRegisters*);

#ifdef __cplusplus
}
#endif
#endif

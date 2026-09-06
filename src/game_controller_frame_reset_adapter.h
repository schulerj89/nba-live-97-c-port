#ifndef NBA97_GAME_CONTROLLER_FRAME_RESET_ADAPTER_H
#define NBA97_GAME_CONTROLLER_FRAME_RESET_ADAPTER_H

#include "recovered/game_controller_frame_reset.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameControllerFrameResetAdapterResult {
    NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CONTEXT_REQUIRED = -20,
    NBA97_GAME_CONTROLLER_FRAME_RESET_TICK_CHILD_INCOMPLETE = -21
};

typedef struct Nba97GameControllerFrameResetTickBinding {
    /* Nba97MatchTickCall carries no full GPR/SP state. This independent entry
     * fixture must be explicitly marked source-proven; no register is inferred
     * from the legacy tick event. */
    Nba97GameTextMemory memory;
    Nba97GameControllerFrameResetRegisters entry_registers;
    uint8_t entry_context_source_proven;
    size_t operation_budget;
    Nba97GameControllerFrameResetIo io;
    void* user;
    Nba97GameControllerFrameResetAccess* access_journal;
    size_t access_journal_capacity;
    Nba97MatchTickService fallback_service;
    void* fallback_user;
    Nba97GameControllerFrameResetProgress progress;
    int result;
    size_t invocations;
} Nba97GameControllerFrameResetTickBinding;

/* Bind only the natural 0x80068CF4 -> 0x800675E4 tick event. Earlier and later
 * services remain explicit through fallback_service. */
int nba97_game_controller_frame_reset_from_match_tick(void*,
    const Nba97MatchTickCall*, Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif

#ifndef NBA97_GAME_CAMERA_STARTUP_ADAPTER_H
#define NBA97_GAME_CAMERA_STARTUP_ADAPTER_H

#include "recovered/game_camera_startup.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE = -20
};

typedef struct Nba97GameCameraStartupTickBinding {
    /* The legacy tick service record has no retained-memory or full-GPR entry
     * state. These two fields are an explicit source fixture. The adapter does
     * not fabricate absent GPRs and requires its known a0 to match the tick's
     * source-proven call argument zero exactly. */
    Nba97GameTextMemory memory;
    Nba97GameCameraStartupRegisters entry_registers;
    size_t operation_budget;
    Nba97GameCameraStartupIo io;
    void* user;
    Nba97GameCameraStartupAccess* access_journal;
    size_t access_journal_capacity;
    Nba97MatchTickService fallback_service;
    void* fallback_user;
    Nba97GameCameraStartupProgress progress;
    int result;
    size_t invocations;
} Nba97GameCameraStartupTickBinding;

/* Typed service adapter for the existing match tick's call at 0x80068C2C.
 * Earlier/later tick services go to fallback_service. A successful camera
 * return produces NBA97_BODY_OK; any exact owner refusal remains in result and
 * returns NBA97_GAME_CAMERA_STARTUP_TICK_CHILD_INCOMPLETE to the tick. */
int nba97_game_camera_startup_from_match_tick(void*,
    const Nba97MatchTickCall*, Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif

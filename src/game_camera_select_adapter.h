#ifndef NBA97_GAME_CAMERA_SELECT_ADAPTER_H
#define NBA97_GAME_CAMERA_SELECT_ADAPTER_H

#include "recovered/game_camera_select.h"
#include "recovered/game_camera_startup.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCameraSelectStartupBinding {
    size_t operation_budget;
    Nba97GameCameraSelectIo io;
    void* user;
    Nba97GameCameraSelectAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameCameraSelectProgress progress;
    Nba97GameCameraSelectRegisters entry_registers;
    int result;
    size_t invocations;
    uint32_t caller_pc;
} Nba97GameCameraSelectStartupBinding;

/* Full-GPR adapter for camera startup's source calls at 0x800796B8/E4.
 * Partial child state is copied back even when the child refuses or stops. */
int nba97_game_camera_select_from_camera_startup(void*,
    const Nba97GameTextMemory*, const Nba97GameCameraStartupEvent*,
    Nba97GameCameraStartupRegisters*);

#ifdef __cplusplus
}
#endif
#endif

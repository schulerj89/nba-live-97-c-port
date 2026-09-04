#ifndef NBA97_GAME_GLOBAL_POINTER_SAVE_H
#define NBA97_GAME_GLOBAL_POINTER_SAVE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameGlobalPointerSaveContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    uint32_t global_pointer;
} Nba97GameGlobalPointerSaveContext;

typedef struct Nba97GameGlobalPointerSaveProgress {
    size_t operations;
    size_t accesses;
    size_t stores;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stored_global_pointer;
    uint8_t completed;
} Nba97GameGlobalPointerSaveProgress;

/* Original GAMEONLY subroutine 0x800A4830..0x800A4843 (5 instructions),
 * called from 0x800299AC immediately after the static-initializer gate.
 * It stores the live gp register at 0x800D6E2C and returns. It has no child
 * call, return value, display operation or gameplay-visible effect.
 * Returns NBA97_TEXT_*. */
int nba97_game_global_pointer_save(Nba97GameGlobalPointerSaveContext*,
    Nba97GameGlobalPointerSaveProgress*);

#ifdef __cplusplus
}
#endif
#endif

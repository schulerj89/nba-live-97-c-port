#ifndef NBA97_GAME_STATIC_INITIALIZERS_H
#define NBA97_GAME_STATIC_INITIALIZERS_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameStaticInitializersContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[2]; /* Incoming s0 and s1. */
} Nba97GameStaticInitializersContext;

typedef struct Nba97GameStaticInitializersProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t initialization_flag;
    uint32_t restored_return_address;
    uint32_t restored_register[2];
    uint8_t already_initialized;
    uint8_t initialized;
    uint8_t completed;
} Nba97GameStaticInitializersProgress;

/* Original GAMEONLY subroutine 0x800948D0..0x8009493F (28 instructions).
 * It is the immediate 0x800299A4 callee. The frozen source has a constructor
 * table base of 0x80015000 and a compile-time count of zero, making the JALR at
 * 0x80094918 unreachable. The cold path writes one to 0x800C4B14; the warm
 * path only preserves the live stack/register frame. No constructor callback,
 * platform effect or gameplay behavior is synthesized. Returns NBA97_TEXT_*. */
int nba97_game_static_initializers(Nba97GameStaticInitializersContext*,
    Nba97GameStaticInitializersProgress*);

#ifdef __cplusplus
}
#endif
#endif

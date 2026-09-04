#ifndef NBA97_GAME_RESET_CALLBACK_H
#define NBA97_GAME_RESET_CALLBACK_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameResetCallbackValue {
    uint32_t word;
    uint8_t known;
} Nba97GameResetCallbackValue;

typedef struct Nba97GameResetCallbackEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint8_t argument_count;
} Nba97GameResetCallbackEvent;

/* This is the indirect child selected by the original dispatch table. The
 * callback may mutate mapped bytes/knownness synchronously, including the
 * saved return-address slot, but not the memory metadata or context. Return 1
 * only after carrying out the represented child boundary. */
typedef int (*Nba97GameResetCallbackIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameResetCallbackEvent*,
    Nba97GameResetCallbackValue*);

typedef struct Nba97GameResetCallbackContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed child calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    Nba97GameResetCallbackIo io;
    void* user;
} Nba97GameResetCallbackContext;

typedef struct Nba97GameResetCallbackProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t dispatch_table;
    uint32_t dispatch_target;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_return_address;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameResetCallbackProgress;

/* Original GAMEONLY subroutine 0x800985DC..0x8009860B (12 instructions),
 * called at 0x80029A10 immediately after SetIntrMask(0). This is the public
 * PsyQ ResetCallback dispatch wrapper: 0x800985E0 reads the table pointer at
 * 0x800C54C8, 0x800985EC reads its +0x0C slot, and 0x800985F4 invokes that
 * target indirectly. In the retail image the initial table is 0x800C54B0 and
 * this slot is 0x80098714. The 0x18-byte frame's saved return address is read
 * live after the child returns; the child's v0 is preserved as the result.
 *
 * Only the wrapper belongs to this owner. The reset implementation selected
 * by the table remains an explicit required callback. This compatibility code
 * therefore does not reset native OS callbacks, input, audio, or rendering.
 * Returns NBA97_TEXT_* and exposes every wrapper-owned access and child call. */
int nba97_game_reset_callback(Nba97GameResetCallbackContext*,
    Nba97GameResetCallbackProgress*);

#ifdef __cplusplus
}
#endif
#endif

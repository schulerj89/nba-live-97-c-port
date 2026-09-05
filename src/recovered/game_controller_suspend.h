#ifndef NBA97_GAME_CONTROLLER_SUSPEND_H
#define NBA97_GAME_CONTROLLER_SUSPEND_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameControllerSuspendEventKind {
    NBA97_GAME_CONTROLLER_SUSPEND_SHUTDOWN = 1
};

typedef struct Nba97GameControllerSuspendValue {
    uint32_t word;
    uint8_t known;
} Nba97GameControllerSuspendValue;

typedef struct Nba97GameControllerSuspendEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameControllerSuspendEvent;

/* Controller shutdown 0x80091224 remains an explicit synchronous service
 * boundary. Returning 1 acknowledges that call. Its v0 is deliberately
 * discarded by the owner, but the callback may mutate mapped bytes and
 * knownness, including the saved ra slot; it must not change memory metadata
 * or the context itself. */
typedef int (*Nba97GameControllerSuspendIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameControllerSuspendEvent*,
    Nba97GameControllerSuspendValue*);

typedef struct Nba97GameControllerSuspendContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus admitted child calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    Nba97GameControllerSuspendIo io;
    void* user;
} Nba97GameControllerSuspendContext;

typedef struct Nba97GameControllerSuspendProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t initial_suspend_flag;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_return_address;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t shutdown_called;
    uint8_t input_suspended;
    uint8_t completed;
} Nba97GameControllerSuspendProgress;

/* Original GAMEONLY controller-suspend wrapper 0x8008F19C..0x8008F1D3
 * (14 instructions), called by main at 0x80029B74 after game-clock shutdown.
 * It reads flag 0x800C4A70 before allocating its frame. If that word is zero,
 * controller shutdown 0x80091224 runs once, v0 is replaced with one, and the
 * flag is set to one. Any nonzero word skips both operations and is returned
 * verbatim rather than normalized. The branch-delay ra spill always occurs,
 * and the epilogue reloads that word from live mapped memory.
 *
 * This native compatibility owner preserves PS1 state and call ordering only;
 * it does not stop or detach host keyboard/gamepad input.
 *
 * Source bytes SHA-256:
 * 40a13c532487813e5aee2bb9caf333e1c69ddbb581cef01b9ae24ea103e10570. */
int nba97_game_controller_suspend(Nba97GameControllerSuspendContext*,
    Nba97GameControllerSuspendProgress*);

#ifdef __cplusplus
}
#endif
#endif

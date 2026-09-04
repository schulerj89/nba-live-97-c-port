#ifndef NBA97_GAME_CONTROLLER_RESUME_H
#define NBA97_GAME_CONTROLLER_RESUME_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameControllerResumeEventKind {
    NBA97_GAME_CONTROLLER_RESUME_INITIALIZE = 1,
    NBA97_GAME_CONTROLLER_RESUME_CLOCK = 2
};

typedef struct Nba97GameControllerResumeValue {
    uint32_t word;
    uint8_t known;
} Nba97GameControllerResumeValue;

typedef struct Nba97GameControllerResumeEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameControllerResumeEvent;

/* The initializer and clock read remain explicit synchronous boundaries. The
 * callback may mutate mapped bytes/knownness, including the saved ra slot,
 * but not memory metadata or the context. Return 1 only after implementing the
 * requested boundary. The initializer's return is discarded; the clock value
 * may be unknown because the original stores it without branching on it. */
typedef int (*Nba97GameControllerResumeIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameControllerResumeEvent*,
    Nba97GameControllerResumeValue*);

typedef struct Nba97GameControllerResumeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed child calls. */
    uint32_t pad_mode;
    uint32_t stack_pointer;
    uint32_t return_address;
    Nba97GameControllerResumeIo io;
    void* user;
} Nba97GameControllerResumeContext;

typedef struct Nba97GameControllerResumeProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t requested_pad_mode;
    uint32_t initial_suspend_flag;
    uint32_t clock_snapshot;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_return_address;
    uint32_t return_v0;
    uint8_t clock_snapshot_known;
    uint8_t return_v0_known;
    uint8_t input_reinitialized;
    uint8_t completed;
} Nba97GameControllerResumeProgress;

/* Original GAMEONLY subroutine 0x8008F1D4..0x8008F223 (20 instructions).
 * Startup calls it at 0x80029A18 and 0x80029A30 with a0=8. It always writes
 * the pad-numbering mode to 0x800D7A48. If the suspend flag at 0x800C4A70 is
 * nonzero, it calls controller initializer 0x80091184 (whose descendants use
 * PsyQ InitPAD/StartPAD), clears that flag, reads clock 0x800A5810, and stores
 * the snapshot at 0x800C4A74. The retail image initializes the flag to one, so
 * the first startup call reinitializes input while the second only reasserts
 * mode 8. Nearby 0x8008F19C performs the inverse suspend operation.
 *
 * This compatibility owner changes mapped PS1 input state only. It does not
 * start, stop, or poll native host input devices and has no direct pixel
 * effect. Returns NBA97_TEXT_* and exposes every owned access and child call. */
int nba97_game_controller_resume(Nba97GameControllerResumeContext*,
    Nba97GameControllerResumeProgress*);

#ifdef __cplusplus
}
#endif
#endif

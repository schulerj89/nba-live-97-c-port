#ifndef NBA97_GAME_PRESENTATION_WAIT_H
#define NBA97_GAME_PRESENTATION_WAIT_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GamePresentationWaitEventKind {
    NBA97_GAME_PRESENTATION_WAIT_SERVICE = 1
};

typedef struct Nba97GamePresentationWaitValue {
    uint32_t word;
    uint8_t known;
} Nba97GamePresentationWaitValue;

typedef struct Nba97GamePresentationWaitEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GamePresentationWaitEvent;

/* The GAMEONLY 0x800A9CC0 synchronization service remains one explicit,
 * synchronous child boundary. Returning 1 means that boundary really
 * completed; a successful no-op is not valid. The callback may mutate mapped
 * bytes and knownness, including this wrapper's saved return-address word.
 * Its live v0 is retained even though known callers use this routine as void. */
typedef int (*Nba97GamePresentationWaitIo)(void*,
    const Nba97GameTextMemory*, const Nba97GamePresentationWaitEvent*,
    Nba97GamePresentationWaitValue*);

typedef struct Nba97GamePresentationWaitContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed child calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t global_pointer;
    Nba97GamePresentationWaitIo io;
    void* user;
} Nba97GamePresentationWaitContext;

typedef struct Nba97GamePresentationWaitProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t service_entry;
    uint32_t restored_return_address;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GamePresentationWaitProgress;

/* Original GAMEONLY presentation-wait wrapper 0x80029BDC..0x80029BFB
 * (8 instructions), first reached by startup at call PC 0x80029A64 and then
 * reused by both twenty-iteration FELOAD delay loops at 0x80029B20/B50. It
 * saves ra, calls synchronization service 0x800A9CC0 with no arguments,
 * reloads the live saved word, and returns the child's incidental v0.
 *
 * The child coordinates DrawSync and the 0x800A450C VBlank ISR. This owner
 * deliberately does not replace that source boundary with host sleep, native
 * renderer cadence, a timeout, or a fabricated frame. A callback refusal and
 * a non-returning source wait therefore cannot become native success. */
int nba97_game_presentation_wait(Nba97GamePresentationWaitContext*,
    Nba97GamePresentationWaitProgress*);

#ifdef __cplusplus
}
#endif
#endif

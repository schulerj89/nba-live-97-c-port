#ifndef NBA97_GAME_VBLANK_SHUTDOWN_H
#define NBA97_GAME_VBLANK_SHUTDOWN_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameVblankShutdownEventKind {
    NBA97_GAME_VBLANK_SHUTDOWN_INTERRUPT_CALLBACK = 1
};

typedef struct Nba97GameVblankShutdownValue {
    uint32_t word;
    uint8_t known;
} Nba97GameVblankShutdownValue;

typedef struct Nba97GameVblankShutdownEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[2];
    uint32_t stack_pointer;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameVblankShutdownEvent;

/* PsyQ InterruptCallback 0x8009860C remains one explicit synchronous service
 * boundary (channel-zero table slot 0x800C54D0). Returning 1 acknowledges
 * completion; value is the old callback left live in v0. The callback may
 * mutate mapped stack bytes and knownness. */
typedef int (*Nba97GameVblankShutdownIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameVblankShutdownEvent*,
    Nba97GameVblankShutdownValue*);

typedef struct Nba97GameVblankShutdownContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus admitted child attempts. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    Nba97GameVblankShutdownIo io;
    void* user;
} Nba97GameVblankShutdownContext;

typedef struct Nba97GameVblankShutdownProgress {
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
    uint32_t incoming_frame_pointer;
    uint32_t interrupt_callback_entry;
    uint32_t interrupt_number;
    uint32_t replacement_callback;
    uint32_t restored_return_address;
    uint32_t restored_frame_pointer;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameVblankShutdownProgress;

/* Original GAMEONLY VBlank shutdown wrapper 0x800A44D4..0x800A450B
 * (14 instructions), reached only by main at call PC 0x80029B64 after the
 * second twenty-frame post-FELOAD wait. It calls PsyQ InterruptCallback(0,
 * NULL) at 0x8009860C to remove handler 0x800A450C, then reloads ra and s8
 * from live mapped stack.
 *
 * The source neither enters a critical section nor checks/normalizes the old
 * callback left in v0. That raw value and knownness remain observable here,
 * and a child-side rewrite of either saved stack word affects the epilogue.
 * No Windows interrupt registration or host timing is performed.
 *
 * Source bytes SHA-256:
 * d30124f93b39486830bd850d0f764977363aebcc9919f7546bf0c1917be5a54c. */
int nba97_game_vblank_shutdown(Nba97GameVblankShutdownContext*,
    Nba97GameVblankShutdownProgress*);

#ifdef __cplusplus
}
#endif
#endif

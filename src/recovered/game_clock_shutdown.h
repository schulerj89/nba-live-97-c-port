#ifndef NBA97_GAME_CLOCK_SHUTDOWN_H
#define NBA97_GAME_CLOCK_SHUTDOWN_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameClockShutdownEventKind {
    NBA97_GAME_CLOCK_SHUTDOWN_INTERRUPT_CALLBACK = 1
};

typedef struct Nba97GameClockShutdownValue {
    uint32_t word;
    uint8_t known;
} Nba97GameClockShutdownValue;

typedef struct Nba97GameClockShutdownEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[2];
    uint32_t stack_pointer;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameClockShutdownEvent;

/* PsyQ InterruptCallback 0x8009860C remains one explicit synchronous service
 * boundary (interrupt-channel-six table slot 0x800C54E8). Returning 1
 * acknowledges completion; value is the old callback left live in v0. The
 * callback may mutate mapped stack bytes and knownness. */
typedef int (*Nba97GameClockShutdownIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameClockShutdownEvent*,
    Nba97GameClockShutdownValue*);

typedef struct Nba97GameClockShutdownContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus admitted child attempts. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    Nba97GameClockShutdownIo io;
    void* user;
} Nba97GameClockShutdownContext;

typedef struct Nba97GameClockShutdownProgress {
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
} Nba97GameClockShutdownProgress;

/* Original GAMEONLY game-clock shutdown wrapper 0x8009167C..0x800916B3
 * (14 instructions), directly invoked by main at call PC 0x80029B6C after
 * VBlank shutdown. Its address is also registered by the clock initializer.
 * It calls PsyQ InterruptCallback(6,NULL) at 0x8009860C to remove game-clock
 * IRQ6 handler 0x800916B4, then reloads ra and s8 from live mapped stack.
 *
 * The source neither enters a critical section nor checks/normalizes the old
 * callback left in v0. That raw value and knownness remain observable here,
 * and a child-side rewrite of either saved stack word affects the epilogue.
 * No Windows interrupt registration or host timing is performed.
 *
 * Source bytes SHA-256:
 * 0724e7dd8a73dd92dde6a9128d2435f60888f950b29d1bf83f6d8e29f259c5dd. */
int nba97_game_clock_shutdown(Nba97GameClockShutdownContext*,
    Nba97GameClockShutdownProgress*);

#ifdef __cplusplus
}
#endif
#endif

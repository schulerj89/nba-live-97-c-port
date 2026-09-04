#ifndef NBA97_GAME_CLOCK_INITIALIZE_H
#define NBA97_GAME_CLOCK_INITIALIZE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameClockInitializeResult {
    NBA97_GAME_CLOCK_DIVIDE_TRAP = -6
};

typedef struct Nba97GameClockInitializeValue {
    uint32_t word;
    uint8_t known;
} Nba97GameClockInitializeValue;

typedef struct Nba97GameClockInitializeEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[3];
    uint32_t stack_pointer;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t argument_count;
} Nba97GameClockInitializeEvent;

/* One event is one synchronous original callee. The callback must perform
 * that boundary, including any mapped side effects, before returning 1. It
 * may mutate mapped bytes/knownness and this owner's live stack frame. */
typedef int (*Nba97GameClockInitializeIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameClockInitializeEvent*,
    Nba97GameClockInitializeValue*);

typedef struct Nba97GameClockInitializeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus acknowledged calls. */
    uint32_t requested_rate;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    Nba97GameClockInitializeIo io;
    void* user;
} Nba97GameClockInitializeContext;

typedef struct Nba97GameClockInitializeProgress {
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
    uint32_t restored_return_address;
    uint32_t restored_frame_pointer;
    uint32_t incoming_rate;
    uint32_t live_rate_divisor;
    uint32_t clock_base;
    uint32_t timer_target;
    uint32_t effective_rate;
    uint32_t interrupt_handler;
    uint32_t shutdown_handler;
    uint32_t root_counter_spec;
    uint32_t root_counter_mode;
    uint32_t set_rcnt_return;
    uint32_t start_rcnt_return;
    uint32_t return_v0;
    uint8_t set_rcnt_return_known;
    uint8_t start_rcnt_return_known;
    uint8_t return_v0_known;
    uint8_t initialization_guard_before;
    uint8_t callback_slots_cleared;
    uint8_t initialized_once;
    uint8_t trap_code;
    uint8_t completed;
} Nba97GameClockInitializeProgress;

/* Original GAMEONLY game-clock initializer 0x800914D8..0x8009167B
 * (105 instructions), called with 120 at main PC 0x80029A4C. On its cold
 * path it clears eight callback words at 0x800D6DEC, installs IRQ6 handler
 * 0x800916B4, and registers shutdown handler 0x8009167C. Every call computes
 * a Timer 2 target as signed 4233600/rate, publishes the quantized effective
 * rate as signed 4233600/target, starts counter 0xF2000002, then resets the
 * game-clock counters through 0x800A5880.
 *
 * Compatibility intentionally retains the source divide BREAKs. Rate zero
 * traps at 0x800915B8; a first quotient of zero is stored before the second
 * division traps at 0x80091600. Those paths remain prefix-committing and do
 * not call ExitCriticalSection. The saved argument is reloaded after cold
 * callbacks, all pre-final raw child returns are ignored, and no native OS
 * interrupt or host timer cadence is invented here. Returns NBA97_TEXT_* or
 * NBA97_GAME_CLOCK_DIVIDE_TRAP with exact mapped owner effects. */
int nba97_game_clock_initialize(Nba97GameClockInitializeContext*,
    Nba97GameClockInitializeProgress*);

#ifdef __cplusplus
}
#endif
#endif

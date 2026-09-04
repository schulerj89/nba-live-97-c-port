#ifndef NBA97_GAME_CLOCK_DELTA_H
#define NBA97_GAME_CLOCK_DELTA_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameClockDeltaEventKind {
    NBA97_GAME_CLOCK_DELTA_READ_CLOCK = 1
};

typedef struct Nba97GameClockDeltaValue {
    uint32_t word;
    uint8_t known;
} Nba97GameClockDeltaValue;

typedef struct Nba97GameClockDeltaEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameClockDeltaEvent;

/* The 0x800A5810 clock read remains one explicit synchronous boundary. The
 * callback may mutate mapped bytes/knownness, including this owner's live
 * stack frame. Its returned value may be unknown because the original stores
 * and returns it without testing availability. */
typedef int (*Nba97GameClockDeltaIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameClockDeltaEvent*, Nba97GameClockDeltaValue*);

typedef struct Nba97GameClockDeltaContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed child calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register_s0;
    uint32_t global_pointer;
    Nba97GameClockDeltaIo io;
    void* user;
} Nba97GameClockDeltaContext;

typedef struct Nba97GameClockDeltaProgress {
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
    uint32_t snapshot_address;
    uint32_t previous_snapshot;
    uint32_t sampled_clock;
    uint32_t return_v0;
    uint32_t restored_return_address;
    uint32_t restored_saved_register_s0;
    uint8_t sampled_clock_known;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameClockDeltaProgress;

/* Original GAMEONLY clock-delta sampler 0x800A584C..0x800A587F
 * (13 instructions), reached during startup at call PC 0x80029A5C and reused
 * by gameplay timing callers. It loads the prior sample from gp+0x164 (retail
 * 0x800D7B2C), calls clock leaf 0x800A5810, replaces that sample, and returns
 * new-old using the source's raw 32-bit SUBU result.
 *
 * Compatibility deliberately does not clamp backward time, reinterpret the
 * delta as signed, or replace wraparound with host wall-clock arithmetic. The
 * old sample is captured before the child call and the new sample is committed
 * even when its knownness is unavailable. This owner does not create timer
 * cadence or render a pixel. Returns NBA97_TEXT_* with exact prefix effects. */
int nba97_game_clock_delta(Nba97GameClockDeltaContext*,
    Nba97GameClockDeltaProgress*);

#ifdef __cplusplus
}
#endif
#endif

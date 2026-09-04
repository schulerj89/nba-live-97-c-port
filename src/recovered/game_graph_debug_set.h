#ifndef NBA97_GAME_GRAPH_DEBUG_SET_H
#define NBA97_GAME_GRAPH_DEBUG_SET_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameGraphDebugSetValue {
    uint32_t word;
    uint8_t known;
} Nba97GameGraphDebugSetValue;

typedef struct Nba97GameGraphDebugSetEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[4];
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register_s0; /* Live s0: the previous debug byte. */
    uint8_t saved_register_s0_known;
    uint8_t argument_count;
} Nba97GameGraphDebugSetEvent;

/* Represents the original synchronous diagnostic-function boundary. The
 * callback may mutate mapped bytes and knownness, including the saved stack
 * words, but not memory metadata or the context. Return 1 only after carrying
 * out the boundary. The source ignores the diagnostic function's return. */
typedef int (*Nba97GameGraphDebugSetIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameGraphDebugSetEvent*);

typedef struct Nba97GameGraphDebugSetContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus acknowledged calls. */
    uint32_t level;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register_s0;
    Nba97GameGraphDebugSetIo io;
    void* user;
} Nba97GameGraphDebugSetContext;

typedef struct Nba97GameGraphDebugSetProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t requested_level;
    uint32_t diagnostic_callback;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_return_address;
    uint32_t restored_saved_register_s0;
    uint32_t return_v0;
    uint8_t previous_level;
    uint8_t previous_level_known;
    uint8_t published_level;
    uint8_t graph_type;
    uint8_t graph_reverse;
    uint8_t return_v0_known;
    uint8_t diagnostic_called;
    uint8_t completed;
} Nba97GameGraphDebugSetProgress;

/* Original GAMEONLY PsyQ SetGraphDebug subroutine 0x800992C4..0x8009932F
 * (27 instructions), called with level 0 at main call PC 0x80029A28. It reads
 * the old byte at 0x800C55C2, stores the low byte of a0 there, and returns the
 * old byte. A nonzero stored byte invokes the live function pointer at
 * 0x800C55BC with format string 0x80028250, the new level, graph type from
 * 0x800C55C0, and reverse flag from 0x800C55C3.
 *
 * Source compatibility intentionally keeps full-argument-to-byte aliasing and
 * the unguarded indirect diagnostic call. These source-era hazards are not
 * clamped or repaired. This owner only changes mapped PS1 bookkeeping and
 * emits an explicit diagnostic boundary; it does not change native logging,
 * rendering, or GPU validation policy. Returns NBA97_TEXT_* with exact mapped
 * effects, while return_v0 carries the previous source byte. */
int nba97_game_graph_debug_set(Nba97GameGraphDebugSetContext*,
    Nba97GameGraphDebugSetProgress*);

#ifdef __cplusplus
}
#endif
#endif

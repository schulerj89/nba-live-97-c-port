#ifndef NBA97_GAME_VBLANK_INITIALIZE_H
#define NBA97_GAME_VBLANK_INITIALIZE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameVblankInitializeValue {
    uint32_t word;
    uint8_t known;
} Nba97GameVblankInitializeValue;

typedef struct Nba97GameVblankInitializeEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[3];
    uint32_t stack_pointer;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t argument_count;
} Nba97GameVblankInitializeEvent;

/* One event is one synchronous original callee. The callback must perform
 * that boundary (including mapped side effects) before returning 1. It may
 * mutate mapped bytes and knownness, including this owner's saved stack
 * words. A callee's raw v0 is returned through value; most are deliberately
 * ignored by the retail owner. */
typedef int (*Nba97GameVblankInitializeIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameVblankInitializeEvent*,
    Nba97GameVblankInitializeValue*);

typedef struct Nba97GameVblankInitializeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus acknowledged calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t frame_pointer;
    uint32_t global_pointer;
    Nba97GameVblankInitializeIo io;
    void* user;
} Nba97GameVblankInitializeContext;

typedef struct Nba97GameVblankInitializeProgress {
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
    uint32_t interrupt_handler;
    uint32_t root_counter_spec;
    uint32_t root_counter_target;
    uint32_t root_counter_mode;
    uint32_t set_rcnt_return;
    uint32_t start_rcnt_return;
    uint32_t return_v0;
    uint8_t set_rcnt_return_known;
    uint8_t start_rcnt_return_known;
    uint8_t return_v0_known;
    uint8_t callback_slots_cleared;
    uint8_t completed;
} Nba97GameVblankInitializeProgress;

/* Original GAMEONLY VBlank-service initializer 0x800A43E8..0x800A44D3
 * (59 instructions), called at main PC 0x80029A38. It saves gp through
 * 0x800A4830, clears eight callback words at 0x800D6E0C, waits for DrawSync,
 * enters a critical section, installs ISR 0x800A450C on interrupt channel 0,
 * requests root counter 0xF2000003 with target 1/mode 0x1000, starts it, exits
 * the critical section, and calls 0x800A3E48 to reset frame counters.
 *
 * Compatibility intentionally keeps the retail counter-3 mismatch: PsyQ
 * SetRCnt rejects low-half index 3, while StartRCnt still ORs its VBlank mask
 * before also returning false. This owner ignores both raw returns, as well as
 * every other child return except the v0 naturally left by the final call. No
 * failure repair, rollback, native OS interrupt, or host frame cadence is
 * invented here. Returns NBA97_TEXT_* with exact mapped owner effects. */
int nba97_game_vblank_initialize(Nba97GameVblankInitializeContext*,
    Nba97GameVblankInitializeProgress*);

#ifdef __cplusplus
}
#endif
#endif

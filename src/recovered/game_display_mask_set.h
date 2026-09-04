#ifndef NBA97_GAME_DISPLAY_MASK_SET_H
#define NBA97_GAME_DISPLAY_MASK_SET_H

#include "game_text_objects.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameDisplayMaskSetEventKind {
    NBA97_GAME_DISPLAY_MASK_DIAGNOSTIC = 1,
    NBA97_GAME_DISPLAY_MASK_CLEAR_ENVIRONMENTS = 2,
    NBA97_GAME_DISPLAY_MASK_GPU_CONTROL = 3
};

typedef struct Nba97GameDisplayMaskSetValue {
    uint32_t word;
    uint8_t known;
} Nba97GameDisplayMaskSetValue;

typedef struct Nba97GameDisplayMaskSetEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[3];
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[2]; /* Live s0 and s1 at this boundary. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameDisplayMaskSetEvent;

/* Represents one synchronous source callee. The callback may mutate mapped
 * bytes and knownness, including the saved o32 frame and live driver table.
 * Return 1 only after carrying out the boundary. The diagnostic and byte-fill
 * return values are ignored by the source; the GPU-control return remains v0. */
typedef int (*Nba97GameDisplayMaskSetIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameDisplayMaskSetEvent*,
    Nba97GameDisplayMaskSetValue*);

typedef struct Nba97GameDisplayMaskSetContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus acknowledged calls. */
    uint32_t mask;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[2]; /* Incoming s0 and s1. */
    Nba97GameDisplayMaskSetIo io;
    void* user;
} Nba97GameDisplayMaskSetContext;

typedef struct Nba97GameDisplayMaskSetProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t requested_mask;
    uint32_t debug_callback;
    uint32_t driver_table;
    uint32_t dispatch_target;
    uint32_t gpu_control_word;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_return_address;
    uint32_t restored_saved_register[2];
    uint32_t return_v0;
    uint8_t debug_level;
    uint8_t return_v0_known;
    uint8_t diagnostic_called;
    uint8_t environment_cache_clear_called;
    uint8_t display_enabled;
    uint8_t completed;
} Nba97GameDisplayMaskSetProgress;

/* Original GAMEONLY PsyQ SetDispMask subroutine 0x80099458..0x800994F3
 * (39 instructions), called with one at GAMEONLY main call PC 0x80029AB4.
 * It optionally reports through 0x800C55BC, clears 20 bytes at 0x800C562C
 * only when disabling, then calls live driver table 0x800C55B8 slot +0x10.
 * The retail table resolves that slot to 0x8009B16C.
 *
 * Source compatibility intentionally preserves full-word zero/nonzero input
 * testing, the GP1(03h) active-low control bit (nonzero -> 0x03000000), the
 * disable-only pre-clear, callback-before-live-table-load ordering, unguarded
 * indirect targets, raw child v0, and live o32 epilogue reloads. This owner
 * emits explicit service boundaries; it does not silently toggle the native
 * renderer or repair any original behavior. Returns NBA97_TEXT_* status. */
int nba97_game_display_mask_set(Nba97GameDisplayMaskSetContext*,
    Nba97GameDisplayMaskSetProgress*);

#ifdef __cplusplus
}
#endif
#endif

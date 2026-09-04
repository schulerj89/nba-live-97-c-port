#ifndef NBA97_GAME_DIRECTORY_CACHE_CONFIGURE_H
#define NBA97_GAME_DIRECTORY_CACHE_CONFIGURE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameDirectoryCacheConfigureContext {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    uint32_t cache_address;
    uint32_t entry_capacity;
    uint32_t stack_pointer;
    uint32_t frame_pointer;
} Nba97GameDirectoryCacheConfigureContext;

typedef struct Nba97GameDirectoryCacheConfigureProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t cache_address;
    uint32_t entry_capacity;
    uint32_t published_cache_address;
    uint32_t published_entry_capacity;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_frame_pointer;
    uint32_t return_v0;
    uint8_t completed;
} Nba97GameDirectoryCacheConfigureProgress;

/* Original GAMEONLY subroutine 0x80092C7C..0x80092CBB (16 instructions),
 * called at 0x800299F8 with preallocated cache 0x8001000C and capacity 0x2C3.
 * It publishes the capacity at 0x800C4AB8 and the cache address at 0x801046A0.
 * Consumers 0x80092CBC, 0x80092D74 and 0x80092F40 treat the cache as 0x14-byte
 * directory records. The original stack argument spills, reloads and frame-
 * pointer restore remain observable and execute in source order.
 *
 * This compatibility owner configures mapped PS1 directory-cache state only.
 * It does not allocate, clear or populate the table, and it does not configure
 * the native host filesystem or asset cache. Returns NBA97_TEXT_*; return_v0
 * records the otherwise unused cache-address value left in v0 at return. */
int nba97_game_directory_cache_configure(
    Nba97GameDirectoryCacheConfigureContext*,
    Nba97GameDirectoryCacheConfigureProgress*);

#ifdef __cplusplus
}
#endif
#endif

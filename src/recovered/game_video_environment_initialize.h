#ifndef NBA97_GAME_VIDEO_ENVIRONMENT_INITIALIZE_H
#define NBA97_GAME_VIDEO_ENVIRONMENT_INITIALIZE_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameVideoEnvironmentInitializeEventKind {
    NBA97_GAME_VIDEO_SET_DEF_DISP_ENV = 1,
    NBA97_GAME_VIDEO_SET_DEF_DRAW_ENV,
    NBA97_GAME_VIDEO_PUT_DISP_ENV,
    NBA97_GAME_VIDEO_PUT_DRAW_ENV,
    NBA97_GAME_VIDEO_DRAW_SYNC
};

typedef struct Nba97GameVideoEnvironmentInitializeValue {
    uint32_t word;
    uint8_t known;
} Nba97GameVideoEnvironmentInitializeValue;

typedef struct Nba97GameVideoEnvironmentInitializeEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[5];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t saved_register[6]; /* Live s0..s5 at the call boundary. */
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameVideoEnvironmentInitializeEvent;

/* Each callback performs one complete synchronous PsyQ boundary. SetDef*
 * callbacks populate the mapped environment record, Put* callbacks install
 * it in the graphics backend, and DrawSync(0) must finish pending work.
 * Returning 1 for a successful no-op is not valid. A child may mutate mapped
 * bytes/knownness, including this owner's live saved-register stack words. */
typedef int (*Nba97GameVideoEnvironmentInitializeIo)(void*,
    const Nba97GameTextMemory*,
    const Nba97GameVideoEnvironmentInitializeEvent*,
    Nba97GameVideoEnvironmentInitializeValue*);

typedef struct Nba97GameVideoEnvironmentInitializeContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus acknowledged calls. */
    uint32_t background_mode;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[6]; /* Incoming s0..s5. */
    uint32_t global_pointer;
    Nba97GameVideoEnvironmentInitializeIo io;
    void* user;
} Nba97GameVideoEnvironmentInitializeContext;

typedef struct Nba97GameVideoEnvironmentInitializeProgress {
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
    uint32_t requested_background_mode;
    uint32_t display_environment[2];
    uint32_t draw_environment[2];
    uint32_t restored_return_address;
    uint32_t restored_saved_register[6];
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t background_byte;
    uint8_t direct_control_bytes_written;
    uint8_t completed;
} Nba97GameVideoEnvironmentInitializeProgress;

/* Original GAMEONLY double-buffer environment initializer
 * 0x80029F20..0x8002A097 (94 instructions), first reached at main call PC
 * 0x80029A6C with background_mode=0. It defines two 512x240 display/draw
 * pairs on opposite 256-line VRAM pages, applies four Put* calls, executes
 * DrawSync(0), and clears buffer selector 0x8001EDE8.
 *
 * Retail quirks are intentional: background_mode is truncated by `sb`; dtd
 * and isbg are also written in two adjacent DRAWENV records that SetDefDrawEnv
 * never initialized; RGB is cleared only in the two initialized records; both
 * pairs are installed so pair 1 remains active while the selector is reset to
 * zero; and no untouched record bytes are sanitized. Returns NBA97_TEXT_*. */
int nba97_game_video_environment_initialize(
    Nba97GameVideoEnvironmentInitializeContext*,
    Nba97GameVideoEnvironmentInitializeProgress*);

#ifdef __cplusplus
}
#endif
#endif

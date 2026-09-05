#ifndef NBA97_GAME_LOADING_SCREEN_H
#define NBA97_GAME_LOADING_SCREEN_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameLoadingScreenEventKind {
    NBA97_GAME_LOADING_SCREEN_LOAD_RESOURCE = 1,
    NBA97_GAME_LOADING_SCREEN_FIND_IMAGE = 2,
    NBA97_GAME_LOADING_SCREEN_DRAW_SYNC = 3,
    NBA97_GAME_LOADING_SCREEN_UPLOAD_IMAGE = 4,
    NBA97_GAME_LOADING_SCREEN_RELEASE_RESOURCE = 5
};

typedef struct Nba97GameLoadingScreenValue {
    uint32_t word;
    uint8_t known;
} Nba97GameLoadingScreenValue;

typedef struct Nba97GameLoadingScreenEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[5];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t saved_register[2]; /* Current s0 and s1. */
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
    uint8_t saved_register_known[2];
} Nba97GameLoadingScreenEvent;

/* Each child is a mandatory synchronous source boundary. The callback returns
 * 1 only after carrying it out and may mutate mapped stack bytes/knownness.
 * LOAD must supply a known v0 because source control flow consumes it. An
 * unknown FIND_IMAGE result remains live across the following DrawSync, then
 * returns NBA97_TEXT_UNKNOWN at its first pointer-argument use. Other raw
 * returns may stay unknown. */
typedef int (*Nba97GameLoadingScreenIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameLoadingScreenEvent*, Nba97GameLoadingScreenValue*);

typedef struct Nba97GameLoadingScreenContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed child calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[2]; /* Incoming s0 and s1. */
    uint32_t global_pointer;
    Nba97GameLoadingScreenIo io;
    void* user;
} Nba97GameLoadingScreenContext;

typedef struct Nba97GameLoadingScreenProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t load_calls;
    size_t lookup_calls;
    size_t draw_sync_calls;
    size_t upload_calls;
    size_t release_calls;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t restored_return_address;
    uint32_t restored_saved_register[2];
    uint32_t loaded_resource;
    uint32_t resolved_image;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t resource_loaded;
    uint8_t image_lookup_completed;
    uint8_t resolved_image_known;
    uint8_t skipped_for_null_resource;
    uint8_t completed;
} Nba97GameLoadingScreenProgress;

/* Original GAMEONLY routine 0x80029E58..0x80029F1F (50 instructions),
 * reached from main at call PC 0x80029AE4. It loads "zloadscr.psh", resolves
 * entry "LdS1", and brackets three 0x800946B8 image uploads with four
 * DrawSync(0) calls. The upload coordinates are exactly (0,0), (0,256), and
 * (512,0), after which the loaded resource is released.
 *
 * Compatibility retains source behavior: a zero archive handle silently
 * skips lookup, drawing, synchronization and release, but a zero image result
 * is not checked and is still passed to all three uploads. Child raw returns
 * remain live, and the o32 epilogue reloads ra/s1/s0 from mutable stack bytes.
 * This owner does not invent archive data, VRAM pixels, GPU completion or heap
 * effects; those remain mandatory callback work. Returns NBA97_TEXT_*.
 * Ghidra instruction SHA-256:
 * a7cd09cf9222d55787b6188292a434ef2d3645f61fc8cbe214251ac39827bf7e. */
int nba97_game_loading_screen(Nba97GameLoadingScreenContext*,
    Nba97GameLoadingScreenProgress*);

#ifdef __cplusplus
}
#endif
#endif

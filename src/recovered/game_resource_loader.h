#ifndef NBA97_GAME_RESOURCE_LOADER_H
#define NBA97_GAME_RESOURCE_LOADER_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameResourceLoaderEventKind {
    NBA97_GAME_RESOURCE_LOADER_ATTEMPT = 1
};

typedef struct Nba97GameResourceLoaderValue {
    uint32_t word;
    uint8_t known;
} Nba97GameResourceLoaderValue;

typedef struct Nba97GameResourceLoaderEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[2];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t saved_register[2]; /* Current s0=filename and s1=flags. */
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
    uint8_t saved_register_known[2];
} Nba97GameResourceLoaderEvent;

/* The callback is the actual synchronous 0x800941C8 load attempt. Returning
 * 1 means the attempt completed; a known zero result is valid and causes the
 * source loop to retry with the same filename and flags. Unknown v0 cannot be
 * used by the source branch and produces NBA97_TEXT_UNKNOWN. The callback may
 * mutate mapped stack bytes/knownness, but not region metadata. */
typedef int (*Nba97GameResourceLoaderIo)(void*,
    const Nba97GameTextMemory*, const Nba97GameResourceLoaderEvent*,
    Nba97GameResourceLoaderValue*);

typedef struct Nba97GameResourceLoaderContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed attempts. */
    uint32_t filename;
    uint32_t flags;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[2]; /* Incoming s0 and s1. */
    uint32_t global_pointer;
    Nba97GameResourceLoaderIo io;
    void* user;
} Nba97GameResourceLoaderContext;

typedef struct Nba97GameResourceLoaderProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    size_t load_attempts;
    size_t null_results;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t filename;
    uint32_t flags;
    uint32_t restored_return_address;
    uint32_t restored_saved_register[2];
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameResourceLoaderProgress;

/* Original GAMEONLY routine 0x80029BFC..0x80029C3F (17 instructions).
 * Natural startup reaches it from 0x80029E70 for zloadscr.psh and from
 * 0x80029AFC for feload.bin; many gameplay resource owners reuse it.
 *
 * It calls 0x800941C8(filename, flags) until v0 is nonzero. Persistent null
 * results therefore hang in the original with no timeout, backoff, input
 * polling, or failure return. The native operation budget only reports that
 * non-returning behavior as NBA97_TEXT_LIMIT; it never changes it into success.
 * Arguments stay cached in s0/s1 across retries, successful v0 remains live,
 * and the epilogue reloads ra/s1/s0 from mutable stack bytes.
 *
 * Source bytes SHA-256:
 * 9534c90429813e90d899fe455f4d83c249eb738b1bc06b93be4470dd0486f9dc. */
int nba97_game_resource_loader(Nba97GameResourceLoaderContext*,
    Nba97GameResourceLoaderProgress*);

#ifdef __cplusplus
}
#endif
#endif

#ifndef NBA97_GAME_CD_SYNC_H
#define NBA97_GAME_CD_SYNC_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameCdSyncEventKind {
    NBA97_GAME_CD_SYNC_SERVICE = 1
};

typedef struct Nba97GameCdSyncValue {
    uint32_t word;
    uint8_t known;
} Nba97GameCdSyncValue;

typedef struct Nba97GameCdSyncEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[2];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameCdSyncEvent;

/* The internal GAMEONLY 0x8009E740 CD_sync implementation remains one exact,
 * synchronous service boundary. Returning 1 acknowledges that it completed;
 * value is its live v0 result. The callback may mutate mapped bytes and
 * knownness, including this wrapper's saved return-address word. */
typedef int (*Nba97GameCdSyncIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameCdSyncEvent*, Nba97GameCdSyncValue*);

typedef struct Nba97GameCdSyncContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus admitted child attempts. */
    uint32_t mode;
    uint32_t result_buffer;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t global_pointer;
    Nba97GameCdSyncIo io;
    void* user;
} Nba97GameCdSyncContext;

typedef struct Nba97GameCdSyncProgress {
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
    uint32_t mode;
    uint32_t result_buffer;
    uint32_t service_entry;
    uint32_t restored_return_address;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t completed;
} Nba97GameCdSyncProgress;

/* Original PsyQ-style CdSync wrapper at GAMEONLY 0x8009DBA0..0x8009DBBF
 * (8 instructions). Main reaches it at call PC 0x80029B34 as CdSync(0, 0),
 * after the first twenty post-FELOAD presentation waits. Other source callers
 * are 0x80092028, 0x80092164 and 0x80092274.
 *
 * The wrapper forwards a0/a1 unchanged to 0x8009E740, returns that child's
 * incidental v0 unchanged (including unknownness), and reloads ra from live
 * mapped stack after the child. It adds no timeout, host polling, return-code
 * normalization, result-buffer validation, or CD/device behavior.
 *
 * Source bytes SHA-256:
 * 3950cb563b219b3b5b59d41cd74547b23be952e3f494769fc8d77fe186380db3. */
int nba97_game_cd_sync(Nba97GameCdSyncContext*, Nba97GameCdSyncProgress*);

#ifdef __cplusplus
}
#endif
#endif

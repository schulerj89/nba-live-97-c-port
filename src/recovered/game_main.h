#ifndef NBA97_GAME_MAIN_H
#define NBA97_GAME_MAIN_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameMainEventKind {
    NBA97_GAME_MAIN_DIRECT_CALL = 1,
    NBA97_GAME_MAIN_INDIRECT_CALL = 2
};

enum Nba97GameMainCalleeOutcome {
    NBA97_GAME_MAIN_CALLEE_RETURNED = 0,
    NBA97_GAME_MAIN_CALLEE_TRANSFERRED = 1,
    NBA97_GAME_MAIN_CALLEE_UNSET = 255
};

typedef struct Nba97GameMainValue {
    uint32_t word;
    uint8_t known;
} Nba97GameMainValue;

typedef struct Nba97GameMainEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[3];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t saved_register[3]; /* s0, s1, s2 at the call boundary. */
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameMainEvent;

/* A direct callee must report RETURNED. The final indirect FELOAD entry may
 * report RETURNED or TRANSFERRED. A callback must return 1 only after carrying
 * out the represented boundary; this owner never supplies successful no-ops.
 * The callback may mutate mapped bytes/knownness synchronously. It must return
 * a known value for 0x80029BFC and 0x80090D60 because their v0 values are used. */
typedef int (*Nba97GameMainIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameMainEvent*, Nba97GameMainValue*,
    enum Nba97GameMainCalleeOutcome*);

typedef struct Nba97GameMainContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Counts mapped accesses plus acknowledged calls. */
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[3]; /* Incoming s0, s1, s2. */
    uint32_t global_pointer;
    Nba97GameMainIo io;
    void* user;
} Nba97GameMainContext;

typedef struct Nba97GameMainProgress {
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
    uint32_t saved_register[3];
    uint32_t restored_return_address;
    uint32_t loaded_image;
    uint32_t loaded_image_size;
    uint32_t indirect_entry;
    uint8_t reached_match_orchestration;
    uint8_t loaded_feload;
    uint8_t transferred;
    uint8_t completed;
} Nba97GameMainProgress;

/* Original GAMEONLY subroutine 0x80029994..0x80029BCB (142 instructions).
 * This is the source-order startup owner: it initializes external services,
 * calls match orchestration 0x8002D8D4, selects "cdrom:", loads "feload.bin",
 * copies it to 0x801E0000 and calls the entry word stored there. External platform,
 * GPU, audio, CD, loader and gameplay effects remain mandatory callbacks.
 * Returns NBA97_TEXT_*; completed is set for an indirect transfer or a normal
 * return after the live source epilogue restores ra/s0/s1/s2 from the stack. */
int nba97_game_main(Nba97GameMainContext*, Nba97GameMainProgress*);

#ifdef __cplusplus
}
#endif
#endif

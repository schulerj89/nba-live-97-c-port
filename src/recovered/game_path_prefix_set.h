#ifndef NBA97_GAME_PATH_PREFIX_SET_H
#define NBA97_GAME_PATH_PREFIX_SET_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GamePathPrefixSetEventKind {
    NBA97_GAME_PATH_PREFIX_COPY = 1,
    NBA97_GAME_PATH_PREFIX_LENGTH = 2
};

typedef struct Nba97GamePathPrefixSetValue {
    uint32_t word;
    uint8_t known;
} Nba97GamePathPrefixSetValue;

typedef struct Nba97GamePathPrefixSetEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[2];
    uint32_t stack_pointer;
    uint32_t global_pointer;
    uint32_t return_address;
    uint8_t kind;
    uint8_t argument_count;
} Nba97GamePathPrefixSetEvent;

/* COPY is the original BIOS A0/19 strcpy boundary and must copy through the
 * terminating byte. LENGTH is BIOS A0/1B strlen and must return the copied
 * byte count. Callbacks may mutate mapped bytes/knownness synchronously and
 * return 1 only after carrying out the represented boundary. */
typedef int (*Nba97GamePathPrefixSetIo)(void*,
    const Nba97GameTextMemory*, const Nba97GamePathPrefixSetEvent*,
    Nba97GamePathPrefixSetValue*);

typedef struct Nba97GamePathPrefixSetContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus completed callbacks. */
    uint32_t source_address;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register_s0;
    uint32_t global_pointer;
    Nba97GamePathPrefixSetIo io;
    void* user;
} Nba97GamePathPrefixSetContext;

typedef struct Nba97GamePathPrefixSetProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t source_address;
    uint32_t destination_address;
    uint32_t copied_length;
    uint32_t final_length;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t restored_return_address;
    uint32_t restored_register_s0;
    uint32_t return_v0;
    uint8_t return_v0_known;
    uint8_t separator_appended;
    uint8_t completed;
} Nba97GamePathPrefixSetProgress;

/* Original GAMEONLY subroutine 0x800A35D8..0x800A364B (29 instructions),
 * called at 0x800299E8 with source string 0x800247E4 ("cdrom:"). It copies
 * the string into 0x800D6DAC, measures it, and appends the two bytes at
 * gp+0x44/45 ("\\" and NUL for the original gp) unless the path is empty or
 * already ends in '\\' or ':'. The startup "cdrom:" input therefore needs no
 * appended separator. Stack spills, live restores and source-ordered partial
 * effects are retained. Returns NBA97_TEXT_*; return_v0 records the otherwise
 * unused source register value at return.
 *
 * This compatibility owner models PS1 startup state only. It does not select
 * host asset paths or participate in the native asset loader. Recovered
 * GAMEONLY composition tests and the game-entry diagnostic invoke it against
 * mapped PS1 RAM to retain original call order and observable CPU effects. */
int nba97_game_path_prefix_set(Nba97GamePathPrefixSetContext*,
    Nba97GamePathPrefixSetProgress*);

#ifdef __cplusplus
}
#endif
#endif

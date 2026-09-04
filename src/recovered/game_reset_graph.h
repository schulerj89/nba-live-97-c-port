#ifndef NBA97_GAME_RESET_GRAPH_H
#define NBA97_GAME_RESET_GRAPH_H

#include "game_text_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameResetGraphEventKind {
    NBA97_GAME_RESET_GRAPH_DIRECT_CALL = 1,
    NBA97_GAME_RESET_GRAPH_INDIRECT_CALL = 2
};

typedef struct Nba97GameResetGraphValue {
    uint32_t word;
    uint8_t known;
} Nba97GameResetGraphValue;

typedef struct Nba97GameResetGraphEvent {
    uint32_t pc;
    uint32_t entry;
    uint32_t argument[3];
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[2]; /* Live s0 and s1 at the call boundary. */
    uint8_t kind;
    uint8_t argument_count;
} Nba97GameResetGraphEvent;

/* A callback represents one synchronous source callee. It may mutate mapped
 * bytes and their knownness, including this owner's saved stack words. Return
 * 1 only after carrying out the represented boundary; successful no-ops do
 * not satisfy memset, ResetCallback, BIOS A0:49, or GPU-reset calls. */
typedef int (*Nba97GameResetGraphIo)(void*, const Nba97GameTextMemory*,
    const Nba97GameResetGraphEvent*, Nba97GameResetGraphValue*);

typedef struct Nba97GameResetGraphContext {
    Nba97GameTextMemory memory;
    size_t operation_budget; /* Mapped accesses plus acknowledged calls. */
    uint32_t mode;
    uint32_t stack_pointer;
    uint32_t return_address;
    uint32_t saved_register[2]; /* Incoming s0 and s1. */
    Nba97GameResetGraphIo io;
    void* user;
} Nba97GameResetGraphContext;

typedef struct Nba97GameResetGraphProgress {
    size_t operations;
    size_t accesses;
    size_t reads;
    size_t stores;
    size_t callbacks_completed;
    uint32_t stopped_pc;
    uint32_t stopped_address;
    uint32_t stopped_entry;
    uint32_t requested_mode;
    uint32_t frame_stack_pointer;
    uint32_t stack_pointer;
    uint32_t driver_table;
    uint32_t driver_reset_target;
    uint32_t debug_callback;
    uint32_t restored_return_address;
    uint32_t restored_saved_register[2];
    uint32_t return_v0;
    uint16_t display_width;
    uint16_t display_height;
    uint8_t masked_mode;
    uint8_t reset_type;
    uint8_t display_width_known;
    uint8_t display_height_known;
    uint8_t return_v0_known;
    uint8_t initialized;
    uint8_t debug_reported;
    uint8_t completed;
} Nba97GameResetGraphProgress;

/* Original GAMEONLY PsyQ ResetGraph subroutine 0x80099058..0x800991AF
 * (86 instructions), called with mode 3 at main call PC 0x80029A20.
 * The initialization path calls printf at 0x8009CB2C, three byte-fill calls
 * at 0x8009BD78, ResetCallback at 0x800985DC, BIOS A0:49 through 0x8009BDA4,
 * and the low-level GPU reset at 0x8009B878. The noninitializing modes retain
 * the original debug and driver-table indirect dispatches.
 *
 * Source compatibility intentionally keeps the original three-bit mode mask,
 * low-byte truncation of the reset result, unchecked reset-type table index,
 * live pointer reloads, and unguarded indirect calls. Those include unsafe
 * source-era behaviors; this native translation does not clamp or repair them.
 * Platform GPU/BIOS work remains explicit through io and is not applied to the
 * native renderer implicitly. Returns NBA97_TEXT_* with exact mapped effects. */
int nba97_game_reset_graph(Nba97GameResetGraphContext*,
    Nba97GameResetGraphProgress*);

#ifdef __cplusplus
}
#endif
#endif

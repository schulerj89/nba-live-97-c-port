#ifndef NBA97_GAME_OVERLAY_ENTRY_H
#define NBA97_GAME_OVERLAY_ENTRY_H
#include "game_text_objects.h"
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameOverlayEntryEventKind {
    NBA97_GAME_OVERLAY_BIOS_A0_39_INIT_HEAP=1,
    NBA97_GAME_OVERLAY_MAIN_29994
};
enum Nba97GameOverlayEntryCalleeOutcome {
    NBA97_GAME_OVERLAY_CALLEE_RETURNED=0,
    NBA97_GAME_OVERLAY_CALLEE_TRANSFERRED=1,
    NBA97_GAME_OVERLAY_CALLEE_UNSET=255
};
enum Nba97GameOverlayEntryResult {
    NBA97_GAME_OVERLAY_ENTRY_BREAK_TRAP=-6,
    NBA97_GAME_OVERLAY_ENTRY_ARITHMETIC_TRAP=-7
};
typedef struct Nba97GameOverlayEntryEvent {
    uint32_t pc,entry,argument[2];
    uint32_t stack_pointer,frame_pointer,global_pointer,return_address;
    uint8_t kind,argument_count;
} Nba97GameOverlayEntryEvent;
/* INIT_HEAP must perform BIOS A0/39 and report RETURNED. MAIN must either
 * execute and report RETURNED, or take ownership of the non-returning launch
 * and report TRANSFERRED. Returning success without an outcome is invalid.
 * Callbacks may mutate retained bytes/knownness synchronously. */
typedef int (*Nba97GameOverlayEntryIo)(void*,const Nba97GameTextMemory*,
    const Nba97GameOverlayEntryEvent*,enum Nba97GameOverlayEntryCalleeOutcome*);
typedef struct Nba97GameOverlayEntryContext {
    Nba97GameTextMemory memory;
    size_t access_budget;
    uint32_t incoming_return_address;
    Nba97GameOverlayEntryIo io;
    void* user;
} Nba97GameOverlayEntryContext;
typedef struct Nba97GameOverlayEntryProgress {
    size_t accesses,stores,words_cleared,callbacks_completed;
    uint32_t stopped_pc,stopped_address,stopped_entry;
    uint32_t stack_pointer,frame_pointer,global_pointer;
    uint32_t heap_base,heap_size,saved_return_address,restored_return_address;
    uint8_t entered_main,transferred,trapped,completed;
} Nba97GameOverlayEntryProgress;

/* Original GAMEONLY subroutine 0x80094828..0x800948CF (42 instructions).
 * Clears exactly [0x800D7BB8,0x8010B61C), derives the source stack and BIOS
 * heap values, calls 0x80098554, then transfers to 0x80029994. If gameplay
 * main returns, the original executes BREAK 1 at 0x800948CC. No BIOS, main,
 * loader, frontend handoff, or playable-game behavior is synthesized here.
 * Returns NBA97_TEXT_* or the two GAME_OVERLAY_ENTRY trap results above. */
int nba97_game_overlay_entry(Nba97GameOverlayEntryContext*,
    Nba97GameOverlayEntryProgress*);
#ifdef __cplusplus
}
#endif
#endif

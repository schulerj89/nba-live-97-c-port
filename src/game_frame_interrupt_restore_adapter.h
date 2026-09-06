#ifndef NBA97_GAME_FRAME_INTERRUPT_RESTORE_ADAPTER_H
#define NBA97_GAME_FRAME_INTERRUPT_RESTORE_ADAPTER_H

#include "recovered/game_frame_interrupt_restore.h"
#include "recovered/game_match_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameFrameInterruptRestoreCallIndex {
    NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_4909C = 0,
    NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_491D8,
    NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_4926C,
    NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_492C0,
    NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_COUNT
};

typedef struct Nba97GameFrameInterruptRestoreBinding {
    /* Persistent CP0 Status is the only carried machine state. The narrow
     * parent proves a0=args[0], zero, and ra=call.pc+8; every other register
     * and HI/LO is independently unknown at each invocation. */
    Nba97GameFrameInterruptRestoreWord cp0_status;
    size_t operation_budget;
    Nba97GameFrameInterruptRestoreJournal* journal;
    size_t journal_capacity;
    size_t invocations;
    size_t completions;
    size_t fallback_callbacks_completed;
    size_t call_count[NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_COUNT];
    /* Latest completed or stopped invocation at each exact call PC. */
    Nba97MatchFrameCall event[
        NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_COUNT];
    Nba97GameFrameInterruptRestoreProgress progress[
        NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_COUNT];
    int result[NBA97_GAME_FRAME_INTERRUPT_RESTORE_CALL_COUNT];
} Nba97GameFrameInterruptRestoreBinding;

/* Bind one exact 0x8004900C match-frame call. The frame has already required
 * its captured Status result to be known before placing it in args[0]. */
int nba97_game_frame_interrupt_restore_from_match_frame(
    void*, const Nba97MatchFrameCall*, Nba97GamePeriodValue*);

/* Execute the existing 0x80049018 frame owner. Only its four restore PCs are
 * intercepted; memory and every other service are forwarded unchanged. */
int nba97_game_match_frame_with_interrupt_restore(
    const Nba97MatchFrameContext*, Nba97GameFrameInterruptRestoreBinding*,
    Nba97MatchFrameProgress*);

#ifdef __cplusplus
}
#endif
#endif

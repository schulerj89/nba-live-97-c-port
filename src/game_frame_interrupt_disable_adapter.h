#ifndef NBA97_GAME_FRAME_INTERRUPT_DISABLE_ADAPTER_H
#define NBA97_GAME_FRAME_INTERRUPT_DISABLE_ADAPTER_H

#include "recovered/game_frame_interrupt_disable.h"
#include "recovered/game_match_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GameFrameInterruptDisableCallIndex {
    NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_49070 = 0,
    NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_491C8,
    NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_4920C,
    NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_4927C,
    NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_COUNT
};

typedef struct Nba97GameFrameInterruptDisableBinding {
    /* CP0 Status persists through every bound call. Only zero and the
     * source-proven ra=call.pc+8 are known at entry; all other GPRs and HI/LO
     * are deliberately unknown because the narrow parent exposes none. */
    Nba97GameFrameInterruptDisableWord cp0_status;
    size_t operation_budget;
    Nba97GameFrameInterruptDisableJournal* journal;
    size_t journal_capacity;
    size_t invocations;
    size_t completions;
    size_t fallback_callbacks_completed;
    size_t call_count[NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_COUNT];
    /* Latest completed or stopped invocation at each exact call PC. */
    Nba97MatchFrameCall event[
        NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_COUNT];
    Nba97GameFrameInterruptDisableProgress progress[
        NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_COUNT];
    int result[NBA97_GAME_FRAME_INTERRUPT_DISABLE_CALL_COUNT];
} Nba97GameFrameInterruptDisableBinding;

/* Bind one of the four exact 0x80048FF4 match-frame call events. This is also
 * the callback used by the natural-parent wrapper. */
int nba97_game_frame_interrupt_disable_from_match_frame(
    void*, const Nba97MatchFrameCall*, Nba97GamePeriodValue*);

/* Execute the complete existing 0x80049018 frame owner. Only its four disable
 * events are intercepted; memory access and every other typed service,
 * including unresolved 0x8004900C restore, are forwarded unchanged. */
int nba97_game_match_frame_with_interrupt_disable(
    const Nba97MatchFrameContext*, Nba97GameFrameInterruptDisableBinding*,
    Nba97MatchFrameProgress*);

#ifdef __cplusplus
}
#endif
#endif

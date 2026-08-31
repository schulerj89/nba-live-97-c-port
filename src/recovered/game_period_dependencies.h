#ifndef NBA97_GAME_PERIOD_DEPENDENCIES_H
#define NBA97_GAME_PERIOD_DEPENDENCIES_H
#include "game_period.h"
#include "game_player_initialization.h"
#ifdef __cplusplus
extern "C" {
#endif

enum Nba97GamePeriodDependencyResult {
    NBA97_PERIOD_DEPENDENCY_OK=1, NBA97_PERIOD_DEPENDENCY_ARGUMENT=0,
    NBA97_PERIOD_DEPENDENCY_UNRESOLVED=-1, NBA97_PERIOD_DEPENDENCY_REFERENCE=-2
};
/* Every event is an original ordered write. An unknown value is zero metadata,
 * not permission to store zero. Field names depend on the owning operation. */
typedef struct Nba97GamePeriodDependencyWrite {
    uint16_t value;
    uint8_t field,record,known;
} Nba97GamePeriodDependencyWrite;

enum Nba97GameRenderSortField { NBA97_RENDER_SORT_TABLE=0, NBA97_RENDER_SORT_INDEX06=1 };
typedef struct Nba97GameRenderSortState {
    Nba97GamePeriodReference render_table[11]; /* Current FDCC0 owned references. */
    Nba97GamePeriodValue x[11],index06[11]; /* Raw word08, halfword06. */
} Nba97GameRenderSortState;
typedef struct Nba97GameRenderSortEffects {
    Nba97GameRenderSortState state;
    Nba97GamePeriodDependencyWrite write[220]; /* Max55 insertion moves x4 writes. */
    uint16_t count;
} Nba97GameRenderSortEffects;
/* Complete60EF8: stable signed-X insertion sort. Only moved entities receive
 * index06 writes; sorted entries retain stale/unknown indices. Aliases persist. */
int nba97_game_period_sort_render(Nba97GameRenderSortEffects* out,
                                  const Nba97GameRenderSortState* input);

enum Nba97GamePeriodResetField {
    NBA97_PERIOD_RESET_PHASE_FDB90=0, NBA97_PERIOD_RESET_FDBD4,
    NBA97_PERIOD_RESET_FDBD6, NBA97_PERIOD_RESET_FDBD0, NBA97_PERIOD_RESET_FDBD2
};
typedef struct Nba97GamePeriodResetEffects {
    Nba97GamePeriodValue phase;
    uint16_t field[4]; /* FDBD4,D6,D0,D2, always written in this order. */
    Nba97GamePeriodDependencyWrite write[5];
    uint8_t count;
} Nba97GamePeriodResetEffects;
/* Complete5828C and actual58260: signed phase<128 clears it, then four resets.
 * No restoration of the old phase. Input must be a raw16 phase value. */
int nba97_game_period_reset_phase(Nba97GamePeriodResetEffects* out,
                                  const Nba97GamePeriodValue* phase);

enum Nba97GamePeriodMotionOperation {
    NBA97_PERIOD_MOTION_BOTH_56B78=0,
    NBA97_PERIOD_MOTION_PRIMARY_5699C=1,
    NBA97_PERIOD_MOTION_SECONDARY_56AA4=2
};
typedef struct Nba97GamePeriodMotionInput {
    Nba97GameAnimationState previous;
    uint32_t request; /* Exact a1 word, not narrowed to the stored clip halfword. */
    uint32_t header_index; /* Actual directory index request&0x3fffffff. */
    Nba97GameMotionHeaderView motion[2]; /* Primary1EC98 / secondary170C8. */
    uint8_t operation;
} Nba97GamePeriodMotionInput;
typedef struct Nba97GamePeriodMotionEffects {
    Nba97GameAnimationState state;
    uint16_t written;
    Nba97GamePeriodDependencyWrite write[24];
    uint8_t count,secondary_called,primary_called;
} Nba97GamePeriodMotionEffects;
/* Complete conditional56B78 and actual56AA4/5699C, or either standalone setter.
 * Source lock/clip/flag short circuits are retained. Actual headers need resolve
 * only on reached setter paths. Native owned directory range0..83 guards the
 * SLL-indexed source access; high request bits still affect full-word equality
 * and signed request<21. Do not narrow the request to header_index or uint16.
 * Upper channel runs first. Primary mode2 synchronization also requires exact
 * full-word equality with secondary clip. It intentionally bypasses frame-count
 * clamping. Existing low flag bits may rewind otherwise in-range frame/timing.
 * Unknown retained/copied/status-cleared state remains explicitly unknown.
 * A needed unknown branch input returnsUNRESOLVED, never a guessed value.
 * ReturnOK can include unknown written fields: apply their provenance, not zero.
 * All dependency APIs leave output unchanged on failure and support input/output
 * byte overlap. They have no runtime memory, asset loader, callbacks or I/O. */
int nba97_game_period_switch_motion(Nba97GamePeriodMotionEffects* out,
                                    const Nba97GamePeriodMotionInput* input);
#ifdef __cplusplus
}
#endif
#endif

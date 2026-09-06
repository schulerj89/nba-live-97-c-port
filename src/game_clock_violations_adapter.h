#ifndef NBA97_GAME_CLOCK_VIOLATIONS_ADAPTER_H
#define NBA97_GAME_CLOCK_VIOLATIONS_ADAPTER_H

#include "recovered/game_clock_violations.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameClockViolationsBinding {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    Nba97GameClockViolationsMachine entry_machine;
    uint8_t entry_machine_ready;
    Nba97GameClockViolationsIo io;
    void* user;
    Nba97GameClockViolationsAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameClockViolationsProgress progress;
    int result;
    size_t invocations;
} Nba97GameClockViolationsBinding;

/* Bind only match tick's actual 0x80068D64 call. The older tick service API
 * has no GPR/SP/HI/LO channel, so an independently established entry machine
 * is mandatory; a0 and JAL ra are checked against the natural event. */
int nba97_game_clock_violations_from_match_tick(
    void*, const Nba97MatchTickCall*, Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif

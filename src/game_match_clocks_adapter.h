#ifndef NBA97_GAME_MATCH_CLOCKS_ADAPTER_H
#define NBA97_GAME_MATCH_CLOCKS_ADAPTER_H

#include "recovered/game_match_clocks.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameMatchClocksBinding {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    Nba97GameMatchClocksMachine entry_machine;
    uint8_t entry_machine_ready;
    Nba97GameMatchClocksIo io;
    void* user;
    Nba97GameMatchClocksAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameMatchClocksProgress progress;
    int result;
    size_t invocations;
} Nba97GameMatchClocksBinding;

/* Compose the complete clocks owner only at match tick's actual 0x80068D40
 * or 0x80068D58 event. The legacy tick callback omits GPR/SP/HI/LO state, so
 * callers must provide the independently established source-entry machine;
 * a0 and JAL ra are checked against the natural event before execution. */
int nba97_game_match_clocks_from_match_tick(
    void*, const Nba97MatchTickCall*, Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif

#ifndef NBA97_GAME_CONTACT_DISPATCH_ADAPTER_H
#define NBA97_GAME_CONTACT_DISPATCH_ADAPTER_H

#include "recovered/game_contact_dispatch.h"
#include "game_ball_contact_gate_adapter.h"
#include "recovered/game_match_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameContactDispatchBinding {
    Nba97GameTextMemory memory;
    size_t operation_budget;
    Nba97GameContactDispatchMachine entry_machine;
    uint8_t entry_machine_ready;
    Nba97GameContactDispatchIo io;
    void* user;
    Nba97GameContactDispatchAccess* access_journal;
    size_t access_journal_capacity;
    Nba97GameContactDispatchProgress progress;
    int result;
    size_t invocations;
} Nba97GameContactDispatchBinding;

typedef struct Nba97GameContactDispatchChildren {
    size_t ball_gate_operation_budget;
    /* Optional complete AI -> AH production composition, otherwise explicit
     * full-machine child callback below. The pointed binding must outlive run. */
    Nba97GameBallContactGateBinding* contact_binding;
    Nba97GameBallContactGateIo child_800602CC;
    Nba97GameContactDispatchIo child_8005FAA8;
    void* user;
    Nba97GameBallContactGateAccess* ball_gate_access_journal;
    size_t ball_gate_access_journal_capacity;
    Nba97GameBallContactGateProgress ball_gate_progress;
    int ball_gate_result;
    size_t ball_gate_invocations;
    size_t child_8005FAA8_invocations;
} Nba97GameContactDispatchChildren;

/* Production child mux: retain 0x8005FAA8 as an explicit typed dependency
 * and execute the complete 0x80060E8C owner for both evidenced AI call PCs.
 * The latter's 0x800602CC child remains explicitly configured. */
int nba97_game_contact_dispatch_compose_children(void*,
    const Nba97GameTextMemory*, const Nba97GameContactDispatchEvent*,
    Nba97GameContactDispatchMachine*);

/* Bind only the natural match-tick event at 0x80068E08. The legacy tick
 * service omits GPR/SP/HI/LO state, so an independently proven entry machine
 * is mandatory and its JAL-produced ra is checked before dispatch. */
int nba97_game_contact_dispatch_from_match_tick(
    void*, const Nba97MatchTickCall*, Nba97GamePeriodValue*);

#ifdef __cplusplus
}
#endif
#endif

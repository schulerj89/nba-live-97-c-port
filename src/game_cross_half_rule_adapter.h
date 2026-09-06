#ifndef NBA97_GAME_CROSS_HALF_RULE_ADAPTER_H
#define NBA97_GAME_CROSS_HALF_RULE_ADAPTER_H

#include "recovered/game_cross_half_rule.h"
#include "recovered/game_match_tick.h"
#include "recovered/game_rule_delay.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameCrossHalfRuleBinding {
  Nba97GameTextMemory memory;
  size_t operation_budget;
  Nba97GameCrossHalfRuleMachine entry_machine;
  uint8_t entry_machine_ready;
  Nba97GameCrossHalfRuleIo io;
  void *user;
  Nba97GameCrossHalfRuleAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameCrossHalfRuleProgress progress;
  Nba97GameRuleDelayProgress rule_delay_progress;
  Nba97MatchTickCall event;
  Nba97GameCrossHalfRuleEvent rule_delay_event;
  int result;
  int rule_delay_result;
  size_t invocations;
  size_t completions;
  size_t rule_delay_invocations;
  size_t fallback_invocations;
  size_t fallback_completions;
} Nba97GameCrossHalfRuleBinding;

/* Compose exactly the source-proven 0x800295C8 event through its complete
 * full-machine owner. All other typed children are forwarded to binding.io. */
int nba97_game_cross_half_rule_compose_child(
    void *, const Nba97GameTextMemory *, const Nba97GameCrossHalfRuleEvent *,
    Nba97GameCrossHalfRuleMachine *);

/* Bind only match tick's natural 0x80068E30 call. The legacy tick service
 * exposes no GPR/SP/HI/LO state, so callers must supply an independently
 * proven full entry machine and mark it ready. No return value is consumed. */
int nba97_game_cross_half_rule_from_match_tick(void *,
                                               const Nba97MatchTickCall *,
                                               Nba97GamePeriodValue *);

#ifdef __cplusplus
}
#endif
#endif

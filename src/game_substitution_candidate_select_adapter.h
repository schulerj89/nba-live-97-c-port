#ifndef NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_ADAPTER_H
#define NBA97_GAME_SUBSTITUTION_CANDIDATE_SELECT_ADAPTER_H

#include "recovered/game_substitution_candidate_select.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Nba97GameSubstitutionCandidateSelectStrategyBinding {
  size_t operation_budget;
  Nba97GameSubstitutionCandidateSelectIo io;
  void *user;
  Nba97GameSubstitutionCandidateSelectAccess *access_journal;
  size_t access_journal_capacity;
  Nba97GameSubstitutionCandidateSelectProgress progress;
  Nba97GameTeamStrategyApplyEvent event;
  int result;
  size_t invocations;
  size_t completions;
  size_t fallback_invocations;
  Nba97GameTeamStrategyApplyIo fallback;
  void *fallback_user;
} Nba97GameSubstitutionCandidateSelectStrategyBinding;

void nba97_game_substitution_candidate_select_strategy_binding_init(
    Nba97GameSubstitutionCandidateSelectStrategyBinding *, size_t,
    Nba97GameSubstitutionCandidateSelectIo, void *,
    Nba97GameSubstitutionCandidateSelectAccess *, size_t,
    Nba97GameTeamStrategyApplyIo, void *);

int nba97_game_substitution_candidate_select_from_strategy(
    void *, const Nba97GameTextMemory *, const Nba97GameTeamStrategyApplyEvent *,
    Nba97GameTeamStrategyApplyMachine *);

#ifdef __cplusplus
}
#endif
#endif

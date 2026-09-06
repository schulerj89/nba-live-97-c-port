#pragma once
#include "recovered/game_team_strategy_apply.h"
#include <string>
#include <vector>
namespace nba97 { struct GameSubstitutionCandidateSelectCapture { std::string receipt; bool dispatch(const Nba97GameTextMemory*,const Nba97GameTeamStrategyApplyEvent*,Nba97GameTeamStrategyApplyMachine*); }; }

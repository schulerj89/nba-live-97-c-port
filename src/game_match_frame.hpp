#pragma once
#include "game_player_frame.hpp"
#include "game_net.hpp"
#include "recovered/game_match_frame.h"
namespace nba97 {
// One shared retained memory/projection owner for the complete49018 sequencing.
// Caller supplies the actual missing pose/camera/court/label/platform services;
// there are no successful fallback calls or synthesized entry state.
class GameMatchFrame {
public:
    explicit GameMatchFrame(GamePlayerFrame& frame):frame_(frame){}
    Nba97MatchFrameIo io{};
    void* user{};
    std::size_t child_operation_budget{1000000};
    Nba97GamePeriodValue average_scale4{}; // Actual ZSF4, separate from ZSF3.
    Nba97PlayerFrameProgress pass_progress{};
    Nba97GameNetProgress net_progress{};
    std::uint32_t last_native_entry{};
    int run(std::size_t operation_budget,Nba97MatchFrameProgress&);
private:
    GamePlayerFrame& frame_;
    Nba97PlayerFrameContext memory_{};
    static int access(void*,std::uint32_t,std::uint32_t,unsigned,unsigned,Nba97PlayerFrameValue*);
    static int call(void*,const Nba97MatchFrameCall*,Nba97GamePeriodValue*);
};
}

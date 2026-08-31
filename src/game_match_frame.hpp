#pragma once
#include "game_player_frame.hpp"
#include "game_player_marker_update.hpp"
#include "game_net.hpp"
#include "game_court_frame_compose.hpp"
#include "recovered/game_player_label_frame.h"
#include "recovered/game_pose_frame.h"
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
    Nba97PlayerMarkerIo marker_io{};
    void* marker_user{};
    std::size_t child_operation_budget{1000000};
    Nba97GamePeriodValue average_scale4{}; // Actual ZSF4, separate from ZSF3.
    Nba97GamePeriodValue leading_bits{}; // Independent retained GTE LZCR state.
    Nba97PlayerFrameProgress pass_progress{};
    Nba97PlayerFrameProgress pose_progress{};
    Nba97PlayerFrameProgress label_progress{};
    Nba97PlayerMarkerProgress marker_progress{};
    Nba97GameNetProgress net_progress{};
    Nba97CourtProgress court_progress{};
    std::uint32_t last_native_entry{};
    int run(std::size_t operation_budget,Nba97MatchFrameProgress&);
private:
    GamePlayerFrame& frame_;
    Nba97PlayerFrameContext memory_{};
    static int access(void*,std::uint32_t,std::uint32_t,unsigned,unsigned,Nba97PlayerFrameValue*);
    static int call(void*,const Nba97MatchFrameCall*,Nba97GamePeriodValue*);
};
}

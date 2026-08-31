#pragma once
#include "game_player_root.hpp"
#include "recovered/game_player_projection.h"
namespace nba97 {
// Shares the exact root/part geometry state, with the additional two vertex
// slots and AVSZ3/OTZ values consumed by525AC. Assign root from the preceding
// GamePlayerRootGeometry, then copy it back after success OR refusal. This
// keeps rotation, translation, V0, IR/MAC, FLAG and all projection FIFOs live.
// Court composition uses the same public values; average_scale3 is independent
// of the court's ZSF4. No control value is initialized by a native default.
struct GamePlayerProjectionGeometry {
    GamePlayerRootGeometry root;
    std::array<Nba97GamePeriodValue,4> extra_vertex{}; // V1XY,Z,V2XY,Z
    Nba97GamePeriodValue average_scale3{},order_depth{};
    int apply(const Nba97PlayerMathRequest&,Nba97GamePeriodValue&);
    static int callback(void*,const Nba97PlayerMathRequest*,Nba97GamePeriodValue*);
};
}

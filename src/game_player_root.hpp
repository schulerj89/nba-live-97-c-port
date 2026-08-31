#pragma once
#include "recovered/game_player_root.h"
#include "game_player_geometry.hpp"
#include <array>
namespace nba97 {
/* The vector member is the SAME retained subset exported to55368. Projection
 * controls/FIFOs are explicit, with no preview-camera or reset defaults. Only
 * the actual RTPS command consumes projection controls; earlier MVMVA operations
 * neither require nor manufacture them. Other geometry registers are untouched. */
struct GamePlayerRootGeometry {
    GamePlayerGeometry vector;
    Nba97GamePeriodValue offset_x{},offset_y{},distance{},depth_cue_a{},depth_cue_b{};
    std::array<Nba97GamePeriodValue,3> screen{};
    std::array<Nba97GamePeriodValue,4> depth{};
    Nba97GamePeriodValue mac0{},ir0{};
    int apply(const Nba97PlayerMathRequest&,Nba97GamePeriodValue&);
    static int callback(void*,const Nba97PlayerMathRequest*,Nba97GamePeriodValue*);
};
}

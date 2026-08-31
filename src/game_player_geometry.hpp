#pragma once
#include "recovered/game_player_geometry.h"
#include <array>
namespace nba97 {
/* Explicit touched subset of retained geometry state. Import/export these
 * fields when sharing a device with court geometry; all other controls/FIFOs,
 * IR0,MAC0 and LZCR are untouched. Unknown fields are canonical{0,0}. */
struct GamePlayerGeometry {
    std::array<Nba97GamePeriodValue,5> rotation{};
    std::array<Nba97GamePeriodValue,3> translation{},ir{},mac{};
    std::array<Nba97GamePeriodValue,2> vertex{};
    Nba97GamePeriodValue flags{};
    int apply(const Nba97PlayerMathRequest&,Nba97GamePeriodValue&);
    static int callback(void*,const Nba97PlayerMathRequest*,Nba97GamePeriodValue*);
};
}

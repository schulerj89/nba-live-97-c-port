#pragma once
#include "game_net.hpp"
#include "game_court_geometry.hpp"
#include <cstddef>

namespace nba97 {
// Compose the already recovered4AC68 court owner with the checked original-
// address memory used by GamePlayerFrame. Geometry is the same retained
// GameNetGeometry used by the preceding4B1A4 pass: callers import/export it
// even when run refuses, because original math prefixes remain observable.
// LZCR is independent retained GTE data-register state and is intentionally
// not inferred from FLAG. This object owns no resources or platform service.
class GameCourtFrameCompose {
public:
    Nba97PlayerFrameContext memory{};
    GameNetGeometry geometry;
    Nba97GamePeriodValue leading_bits{};
    int run(std::size_t operation_budget,Nba97CourtProgress&);
private:
    int bridge_status_{NBA97_BODY_OK};
    static int access(void*,std::uint32_t,std::uint32_t,unsigned,int,Nba97CourtValue*);
    static int math(void*,const Nba97CourtMathRequest*,Nba97CourtValue*);
};
}

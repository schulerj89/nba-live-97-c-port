#pragma once
#include <cstddef>
#include <cstdint>
#include "game_player_projection.hpp"
#include "recovered/game_net.h"
namespace nba97 {
// Net projection shares all player/court matrices and FIFOs. ZSF4 is a separate
// explicit retained control; it is not inferred from ZSF3 or a default camera.
struct GameNetGeometry {
    GamePlayerProjectionGeometry player;
    Nba97GamePeriodValue average_scale4{};
    int apply(const Nba97PlayerMathRequest&,Nba97GamePeriodValue&);
    static int callback(void*,const Nba97PlayerMathRequest*,Nba97GamePeriodValue*);
};
// Borrow an existing checked FrameContext (for example bindContext output).
// Only its memory callback/user are used. No ownership, mappings, allocations
// or callbacks are invented. Import geometry before a pass and export it after
// success OR refusal; copying this adapter does not clone borrowed memory.
class GameNet {
public:
    Nba97PlayerFrameContext memory{};
    GameNetGeometry geometry;
    int frame(std::size_t,Nba97GameNetProgress&);
    int initialize(std::size_t,Nba97GameNetProgress&);
    int draw(std::size_t,Nba97GameNetProgress&);
    int decode(std::uint32_t,std::uint32_t,std::size_t,Nba97GamePeriodValue&,Nba97GameNetProgress&);
private:
    Nba97PlayerFrameContext context(std::size_t);
    static int access(void*,std::uint32_t,std::uint32_t,unsigned,unsigned,Nba97PlayerFrameValue*);
    static int math(void*,const Nba97PlayerMathRequest*,Nba97GamePeriodValue*);
};
}

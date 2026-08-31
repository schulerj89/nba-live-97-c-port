#pragma once
#include <cstddef>
#include "recovered/game_player_marker_update.h"
namespace nba97 {
// Borrow the exact retained frame access callback/user. Separate synchronous
// IO owns the full five image-placement arguments and actual upload/sync work.
// This adapter creates no memory, geometry, image, palette or device state.
class GamePlayerMarkerUpdate {
public:
    Nba97PlayerFrameContext memory{};
    Nba97PlayerMarkerIo io{};
    void* user{};
    int run(std::size_t operation_budget,Nba97PlayerMarkerProgress&);
private:
    static int access(void*,uint32_t,uint32_t,unsigned,unsigned,Nba97PlayerFrameValue*);
    static int call(void*,const Nba97PlayerMarkerCall*,Nba97GamePeriodValue*);
};
}

#ifndef NBA97_GAME_TIPOFF_ANNOUNCEMENT_CAPTURE_H
#define NBA97_GAME_TIPOFF_ANNOUNCEMENT_CAPTURE_H
#include "game_tipoff_announcement_adapter.h"
#include <string>
namespace nba97 {
struct GameTipoffAnnouncementCapture {
    std::string receipt;
    int dispatch(const Nba97GameTextMemory*,const Nba97GameFirstPeriodStartupEvent*,
        Nba97GameFirstPeriodStartupRegisters*,unsigned mode);
};
}
#endif

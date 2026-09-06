#ifndef NBA97_GAME_MATCH_SERVICE_PUBLISH_CAPTURE_H
#define NBA97_GAME_MATCH_SERVICE_PUBLISH_CAPTURE_H
#include "game_match_service_publish_adapter.h"
#include "game_period_expiry_adapter.h"
#include <string>
namespace nba97 {
struct GameMatchServicePublishCapture {
    std::string receipt;
    Nba97GameMatchServicePublishProgress progress{};
    int dispatch(const Nba97GameTextMemory*,const Nba97MatchTickCall*,const Nba97GamePeriodExpiryProgress*);
};
}
#endif

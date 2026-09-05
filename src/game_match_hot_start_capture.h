#ifndef NBA97_GAME_MATCH_HOT_START_CAPTURE_H
#define NBA97_GAME_MATCH_HOT_START_CAPTURE_H
#include <string>
namespace nba97 {
// Explicit synthetic full-register input to the production tick adapter.
// The legacy tick API cannot provide a live guest stack/register context.
std::string captureGameMatchHotStart();
}
#endif

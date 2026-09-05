#ifndef NBA97_GAME_MATCH_INITIALIZE_CAPTURE_H
#define NBA97_GAME_MATCH_INITIALIZE_CAPTURE_H
#include "recovered/game_match_session.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace nba97 {
struct GameMatchInitializeCapture {
    std::vector<std::uint16_t> before = std::vector<std::uint16_t>(512u*240u);
    std::vector<std::uint16_t> after = std::vector<std::uint16_t>(512u*240u);
    std::string receipt;
    bool dispatch(const Nba97GameTextMemory*,const Nba97GameMatchSessionEvent*,
                  Nba97GameMatchSessionValue*);
    void writeReceipt(const std::filesystem::path&) const;
};
}
#endif

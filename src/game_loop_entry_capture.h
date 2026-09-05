#ifndef NBA97_GAME_LOOP_ENTRY_CAPTURE_H
#define NBA97_GAME_LOOP_ENTRY_CAPTURE_H
#include "recovered/game_match_session.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace nba97 {
struct GameLoopEntryCapture {
    std::vector<std::uint16_t> before = std::vector<std::uint16_t>(512u*240u);
    std::vector<std::uint16_t> after = std::vector<std::uint16_t>(512u*240u);
    std::string receipt;
    // Returns evidence acceptance, never source-wrapper completion.
    bool probe(const Nba97GameTextMemory*,const Nba97GameMatchSessionEvent*);
    void writeReceipt(const std::filesystem::path&) const;
};
}
#endif

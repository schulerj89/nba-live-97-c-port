#ifndef NBA97_FELOAD_ENTRY_CAPTURE_H
#define NBA97_FELOAD_ENTRY_CAPTURE_H

#include "recovered/game_main.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nba97 {
/* Offline native capture adapter. The caller supplies retained scanout before
 * and after dispatch. Synthetic child services are identified in the receipt;
 * this is never a replacement for the unresolved FELOAD loader main. */
struct FeloadEntryCapture {
    std::vector<std::uint16_t> before = std::vector<std::uint16_t>(512u*240u);
    std::vector<std::uint16_t> after = std::vector<std::uint16_t>(512u*240u);
    std::string receipt;
    bool dispatch(const Nba97GameTextMemory*, const Nba97GameMainEvent*,
                  Nba97GameMainValue*, Nba97GameMainCalleeOutcome*);
    void writeReceipt(const std::filesystem::path&) const;
};
}
#endif

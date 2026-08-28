#pragma once
#include "frontend_help.hpp"

namespace nba97 {
// Exact private 800AFE06 no-facts descriptor; no game text in source.
FrontendHelpDescriptor parsePlayerNotice(const std::vector<std::uint8_t>&);
FrontendHelpDescriptor loadPlayerNotice(const std::filesystem::path&);
// Malformed/missing IDX is not evidence that a player has no facts.
bool playerHasCoolFacts(const std::vector<std::uint8_t>& index, std::uint16_t player);
}

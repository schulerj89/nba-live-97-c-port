#pragma once
#include "game_render_backend.hpp"
#include "recovered/game_player_labels.h"
#include "recovered/game_text_objects.h"
#include <string>

namespace nba97 {
struct GameTextBinding {
    // Explicit proven source address of this retained view, required for the
    // original encoded object/packet links. Never derive it from a host pointer.
    std::uint32_t sourceAddress = 0;
    Nba97GameRenderBuffer view{};
};
struct GamePlayerLabelResult {
    int result = NBA97_RENDER_ARGUMENT;
    int textResult = NBA97_TEXT_ARGUMENT;
    unsigned completed = 0;
    Nba97GameTextProgress textProgress{};
    std::string detail;
};
// Actual35A44 ->30758/30D18/99960 using the same retained allocations. Required
// SDK callback performs the actual packet-clear/diagnostic boundary or refuses.
// No default DMA success, source address, font, object pool, or visible label.
// This restricted bridge requires known legacy label buffers because35A44 has
// no per-byte knownness channel. Text resources retain their knownness.
// Mutates a candidate in place: completed source prefixes survive refusal.
// For atomic publication clone all memory, rebind every borrowed input view,
// and stage external callback state too. This function does not do that clone.
GamePlayerLabelResult runGamePlayerLabels(GameRenderMemory&, Nba97GamePlayerLabels&,
    const std::vector<GameTextBinding>&, Nba97GameTextIo, void*, std::size_t stepBudget = 100000);
}

#pragma once
#include "gameplay_mocap.hpp"
#include "recovered/game_player_initialization.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace nba97 {
using GameplayFormation=std::array<std::array<std::int16_t,3>,5>;
class GameplaySetup;
using GameplaySetupResource=std::shared_ptr<const GameplaySetup>;

// One immutable generation owns the motion bytes/index and period table values.
// It supplies actual source data to recovered owners, not a playable scene.
class GameplaySetup final {
public:
    const GameplayFormation& formation(unsigned index) const;
    std::uint32_t duration(bool overtime,std::uint8_t raw_option) const noexcept;
    Nba97GameMotionHeaderView motionView(unsigned channel,unsigned slot) const;
    const GameplayMocapResource& mocap() const noexcept {return mocap_;}
private:
    GameplaySetup()=default;
    GameplayMocapResource mocap_;
    std::array<GameplayFormation,2> formations_{};
    std::array<std::array<std::uint32_t,256>,2> durations_{};
    friend GameplaySetupResource decodeGameplaySetup(const std::vector<std::uint8_t>&,GameplayMocapResource);
};

// Pack format: NBA97PER,version1,payload2112,CRC32(payload),64 formation bytes
// followed by two256-word source lookup windows. Original bytes stay private.
// New resource publication is atomic: a thrown failure cannot alter old owners.
GameplaySetupResource decodeGameplaySetup(const std::vector<std::uint8_t>& period_pack,
                                          GameplayMocapResource mocap);
GameplaySetupResource loadGameplaySetup(const std::filesystem::path& private_folder);
}

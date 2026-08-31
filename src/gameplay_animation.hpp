#pragma once
#include "gameplay_setup.hpp"
#include "recovered/game_animation_advance.h"

namespace nba97 {
class GameplayAnimation;
using GameplayAnimationResource=std::shared_ptr<const GameplayAnimation>;

// Retains the setup/motion generation and all seven original lookup windows.
// The borrowed C views are valid only while this immutable owner is retained.
class GameplayAnimation final {
public:
    GameplayAnimation(const GameplayAnimation&)=delete;
    GameplayAnimation& operator=(const GameplayAnimation&)=delete;
    GameplayAnimation(GameplayAnimation&&)=delete;
    GameplayAnimation& operator=(GameplayAnimation&&)=delete;
    const GameplaySetupResource& setup() const noexcept {return setup_;}
    const Nba97GameAnimationResources& view() const noexcept {return view_;}
private:
    GameplayAnimation()=default;
    GameplaySetupResource setup_;
    std::vector<std::uint16_t> words_;
    Nba97GameAnimationResources view_{};
    friend GameplayAnimationResource decodeGameplayAnimation(const std::vector<std::uint8_t>&,GameplaySetupResource);
};

// NBA97ANI, version1, payload0x30084, CRC32, then actual GAME bytes from
// A850C through D858F. These are owned data, never executable native code.
// Includes adjacent source reads for every possible initial unsigned-halfword
// index and later signed-halfword index; ordinary maps have only22 entries.
GameplayAnimationResource decodeGameplayAnimation(const std::vector<std::uint8_t>&,
                                                  GameplaySetupResource);
GameplayAnimationResource loadGameplayAnimation(const std::filesystem::path& private_pack,
                                                GameplaySetupResource);
}

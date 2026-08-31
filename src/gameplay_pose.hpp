#pragma once
#include "gameplay_mocap.hpp"
#include "recovered/game_pose_request.h"
#include <array>

namespace nba97 {

class GameplayPoseResources;
using GameplayPoseResource = std::shared_ptr<const GameplayPoseResources>;

// Immutable original-byte ownership: the foot prefix generation retains the
// exact normalized motion resource used to build it. Reload by replacing the
// shared owner; existing consumers keep all three resources alive together.
class GameplayPoseResources final {
public:
    const GameplayMocapResource& mocap() const noexcept { return mocap_; }
    const std::vector<std::uint8_t>& footBytes() const noexcept { return foot_; }
    const std::vector<std::uint8_t>& trigBytes() const noexcept { return trig_; }
    const std::array<std::uint16_t,84>& prefixes() const noexcept { return prefixes_; }
    std::uint32_t footRows() const noexcept { return rows_; }
    Nba97GamePose sample(const Nba97GamePosePacket& packet) const;
    Nba97GameFootOffset footOffset(const Nba97GameFootInput& input) const;
    // Read-only57B18 callback adapter; unknown entity inputs/resource errors
    // return false without manufacturing an offset or throwing across C.
    // Context points to a retained GameplayPoseResource variable, not .get().
    static int resolveFoot(void* resource, unsigned physical_entity,
        const Nba97GamePoseEntity* entity, unsigned leg, Nba97GameFootOffset* out) noexcept;
private:
    GameplayPoseResources(GameplayMocapResource mocap,
        std::vector<std::uint8_t> foot, std::vector<std::uint8_t> trig);
    GameplayMocapResource mocap_;
    std::vector<std::uint8_t> foot_, trig_;
    std::array<std::uint16_t,84> prefixes_{};
    std::uint32_t rows_=0;
    friend GameplayPoseResource decode_gameplay_pose_resources(GameplayMocapResource,
        std::vector<std::uint8_t>,std::vector<std::uint8_t>);
};
// ZHOTS raw bytes plus exactly1028 bytes from GAME D6E30 (257 signedLE words).
// Foot file must cover the normalized maximum-count prefix span; extra bytes
// stay owned, so original in-file out-of-count accesses are not silently fixed.
GameplayPoseResource decode_gameplay_pose_resources(GameplayMocapResource mocap,
    std::vector<std::uint8_t> foot, std::vector<std::uint8_t> trig);
GameplayPoseResource load_gameplay_pose_resources(GameplayMocapResource mocap,
    const std::filesystem::path& foot, const std::filesystem::path& trig);

} // namespace nba97

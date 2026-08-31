#include "gameplay_pose.hpp"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace nba97 {
namespace {
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    if (!input) throw std::runtime_error("missing gameplay pose resource: "+path.string());
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    if (input.bad()) throw std::runtime_error("failed reading gameplay pose resource: "+path.string());
    return bytes;
}
}
GameplayPoseResources::GameplayPoseResources(GameplayMocapResource mocap,
    std::vector<std::uint8_t> foot,std::vector<std::uint8_t> trig)
    :mocap_(std::move(mocap)),foot_(std::move(foot)),trig_(std::move(trig)) {
    if (!mocap_) throw std::invalid_argument("gameplay pose requires owned motion bytes");
    if (nba97_game_foot_prefixes(&mocap_->index(),prefixes_.data(),&rows_)!=NBA97_GAME_POSE_OK)
        throw std::runtime_error("invalid gameplay foot prefix source");
    if (trig_.size()!=1028) throw std::runtime_error("gameplay trig requires257 original words");
    if (foot_.empty() || static_cast<std::uint64_t>(rows_)*12u>foot_.size())
        throw std::runtime_error("truncated gameplay foot resource");
}
Nba97GamePose GameplayPoseResources::sample(const Nba97GamePosePacket& packet) const {
    Nba97GamePose pose{};
    const auto& bytes=mocap_->bytes();
    if (nba97_game_pose_sample(bytes.data(),bytes.size(),&mocap_->index(),&packet,&pose)!=NBA97_GAME_POSE_OK)
        throw std::out_of_range("gameplay pose request reads unavailable motion bytes");
    return pose;
}
Nba97GameFootOffset GameplayPoseResources::footOffset(const Nba97GameFootInput& input) const {
    Nba97GameFootOffset out{};
    if (nba97_game_foot_offset(foot_.data(),foot_.size(),prefixes_.data(),trig_.data(),trig_.size(),&input,&out)!=NBA97_GAME_POSE_OK)
        throw std::out_of_range("gameplay foot request reads unavailable resource bytes");
    return out;
}
int GameplayPoseResources::resolveFoot(void* resource, unsigned physical_entity,
    const Nba97GamePoseEntity* entity, unsigned leg, Nba97GameFootOffset* out) noexcept {
    if (!resource || !entity || !out || physical_entity>=10) return 0;
    constexpr std::uint32_t required=(1u<<NBA97_POSE_4A)|(1u<<NBA97_POSE_54)|
        (1u<<NBA97_POSE_C6)|(1u<<NBA97_POSE_9A)|(1u<<NBA97_POSE_A8);
    if ((entity->half_known&required)!=required || !(entity->word_known&(1u<<NBA97_POSE_10))) return 0;
    const auto& owner=*static_cast<const GameplayPoseResource*>(resource);
    if (!owner) return 0;
    const auto& self=*owner;
    const auto angle=entity->half[NBA97_POSE_A8];
    Nba97GameFootInput input{};
    input.clip4a=entity->half[NBA97_POSE_4A]; input.frame54=entity->half[NBA97_POSE_54];
    input.scale_c6=entity->half[NBA97_POSE_C6]; input.conversion9a=entity->half[NBA97_POSE_9A];
    input.angle_a8=static_cast<std::int16_t>(angle<0x8000u ? static_cast<int>(angle) : static_cast<int>(angle)-65536);
    input.height10=entity->word[NBA97_POSE_10]; input.leg=leg;
    return nba97_game_foot_offset(self.foot_.data(),self.foot_.size(),self.prefixes_.data(),
        self.trig_.data(),self.trig_.size(),&input,out)==NBA97_GAME_POSE_OK;
}
GameplayPoseResource decode_gameplay_pose_resources(GameplayMocapResource mocap,
    std::vector<std::uint8_t> foot,std::vector<std::uint8_t> trig) {
    return GameplayPoseResource(new GameplayPoseResources(std::move(mocap),std::move(foot),std::move(trig)));
}
GameplayPoseResource load_gameplay_pose_resources(GameplayMocapResource mocap,
    const std::filesystem::path& foot,const std::filesystem::path& trig) {
    auto foot_bytes=read(foot); auto trig_bytes=read(trig);
    return decode_gameplay_pose_resources(std::move(mocap),std::move(foot_bytes),std::move(trig_bytes));
}
} // namespace nba97

#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace nba97 {
struct PlayerPortraitSlice {
    std::uint32_t physical_record, bytes, offset;
    const std::uint8_t* data; // valid while its immutable archive remains alive
};
// Native resident ownership for F9418/Z1PORT, separate from F84C8/Z1COOL.
// The index is accepted by the original whole-file checksum decision; archive
// extents are bounded here, and each requested slice is checksum-tested before
// PNG decode. No CD delay, allocation identity or GPU lifetime is synthesized.
class PlayerPortraitArchive final {
public:
    static std::shared_ptr<const PlayerPortraitArchive> load(
        const std::filesystem::path& index, const std::filesystem::path& archive);
    static std::shared_ptr<const PlayerPortraitArchive> fromBytes(
        std::vector<std::uint8_t> index, std::vector<std::uint8_t> archive);
    std::uint32_t count() const noexcept { return count_; } // excludes reserved0
    std::uint32_t physicalRecord(std::int32_t logical_player) const;
    PlayerPortraitSlice slice(std::uint32_t physical_record) const;
    bool checksumAccepted(std::uint32_t physical_record) const;
    // UI-thread completion effect only; call after native generation checks.
    int acceptChecksum(std::uint32_t physical_record, std::uint32_t* selection_blocked) const;
    std::size_t indexBytes() const noexcept { return index_.size(); }
    std::size_t archiveBytes() const noexcept { return archive_.size(); }
private:
    PlayerPortraitArchive(std::vector<std::uint8_t>, std::vector<std::uint8_t>);
    const std::vector<std::uint8_t> index_, archive_;
    const std::uint32_t count_;
};
} // namespace nba97

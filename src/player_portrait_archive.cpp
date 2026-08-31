#include "player_portrait_archive.hpp"
#include "recovered/frontend_resource.h"
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace nba97 {
namespace {
std::uint32_t word(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
        (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("cannot open original portrait resource: " + path.string());
    const auto length = file.tellg();
    if (length < 0 || static_cast<std::uint64_t>(length) > INT32_MAX)
        throw std::runtime_error("original portrait resource size outside supported domain: " + path.string());
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    file.seekg(0);
    if (!data.empty() && !file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
        throw std::runtime_error("cannot read original portrait resource: " + path.string());
    return data;
}
std::vector<std::uint8_t> validatedIndex(std::vector<std::uint8_t> data) {
    if (data.size() > INT32_MAX) throw std::runtime_error("Z1PORT.IDX exceeds source length domain");
    Nba97ResourceValidation receipt{};
    // Retail D9B40 is0: a missing outer trailer is allowed, but a present bad
    // one is rejected. Do not silently impose a stricter original file format.
    if (nba97_resource_validate_file(data.data(), static_cast<std::uint32_t>(data.size()), 0, &receipt) != 1)
        throw std::runtime_error("Z1PORT.IDX original whole-file checksum rejected");
    data.resize(receipt.payload_bytes); // source shrink removes12, not14 bytes
    if (data.size() < 12) throw std::runtime_error("Z1PORT.IDX missing reserved portrait record");
    const auto count = word(data.data());
    if (count > INT32_MAX || std::uint64_t(count) + 1 > (data.size() - 4) / 8)
        throw std::runtime_error("Z1PORT.IDX reserved-inclusive table truncated or invalid");
    return data;
}
} // namespace

PlayerPortraitArchive::PlayerPortraitArchive(std::vector<std::uint8_t> index,
    std::vector<std::uint8_t> archive)
    : index_(validatedIndex(std::move(index))), archive_(std::move(archive)), count_(word(index_.data())) {
    if (archive_.size() > INT32_MAX) throw std::runtime_error("Z1PORT.BIG exceeds supported resource domain");
    for (std::uint32_t physical = 0; physical <= count_; ++physical) (void)slice(physical);
}
std::shared_ptr<const PlayerPortraitArchive> PlayerPortraitArchive::fromBytes(
    std::vector<std::uint8_t> index, std::vector<std::uint8_t> archive) {
    return std::shared_ptr<const PlayerPortraitArchive>(
        new PlayerPortraitArchive(std::move(index), std::move(archive)));
}
std::shared_ptr<const PlayerPortraitArchive> PlayerPortraitArchive::load(
    const std::filesystem::path& index, const std::filesystem::path& archive) {
    auto index_bytes = read(index);
    auto archive_bytes = read(archive);
    return fromBytes(std::move(index_bytes), std::move(archive_bytes));
}
std::uint32_t PlayerPortraitArchive::physicalRecord(std::int32_t logical_player) const {
    // Source310D8 compares signed values; malformed negative IDs are explicitly
    // outside this native span domain, not silently remapped to fallback0.
    if (logical_player < 0) throw std::runtime_error("negative original portrait player ID");
    const auto logical = static_cast<std::uint32_t>(logical_player);
    return logical < count_ ? logical + 1 : 0;
}
PlayerPortraitSlice PlayerPortraitArchive::slice(std::uint32_t physical) const {
    if (physical > count_) throw std::runtime_error("portrait physical record outside Z1PORT.IDX");
    const auto* entry = index_.data() + 4 + std::size_t(physical) * 8;
    const auto bytes = word(entry), offset = word(entry + 4);
    if (bytes < 2 || bytes > INT32_MAX || std::uint64_t(offset) + bytes > archive_.size())
        throw std::runtime_error("portrait slice missing checksum or outside Z1PORT.BIG");
    return {physical, bytes, offset, archive_.data() + offset};
}
bool PlayerPortraitArchive::checksumAccepted(std::uint32_t physical) const {
    const auto raw = slice(physical);
    std::uint16_t checksum = 0;
    if (!nba97_resource_crc16(raw.data, raw.bytes - 2, &checksum))
        throw std::runtime_error("portrait checksum length outside supported source domain");
    const auto stored = std::uint32_t(raw.data[raw.bytes - 2]) |
        (std::uint32_t(raw.data[raw.bytes - 1]) << 8);
    return checksum == stored;
}
int PlayerPortraitArchive::acceptChecksum(std::uint32_t physical, std::uint32_t* blocked) const {
    const auto raw = slice(physical);
    return nba97_portrait_checksum_accept(raw.data, raw.bytes, blocked);
}
} // namespace nba97

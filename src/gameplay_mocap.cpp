#include "gameplay_mocap.hpp"
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace nba97 {

GameplayMocap::GameplayMocap(std::vector<std::uint8_t> bytes, const Nba97GameMocapIndex& index)
    : bytes_(std::move(bytes)), index_(index) {}

const Nba97GameMocapHeader* GameplayMocap::header(std::size_t channel, std::size_t slot) const {
    if (channel >= NBA97_GAME_MOCAP_CHANNELS || slot >= NBA97_GAME_MOCAP_SLOTS)
        throw std::out_of_range("gameplay mocap channel/slot");
    const auto at = index_.reference[channel][slot];
    return at == NBA97_GAME_MOCAP_NONE ? nullptr : &index_.header[at];
}

GameplayMocapResource decode_gameplay_mocap(std::vector<std::uint8_t> bytes) {
    Nba97GameMocapIndex index{};
    const auto result = nba97_game_mocap_index(bytes.data(), bytes.size(), &index);
    if (result != NBA97_GAME_MOCAP_OK)
        throw std::runtime_error("invalid raw gameplay mocap, guard " + std::to_string(result));
    return GameplayMocapResource(new GameplayMocap(std::move(bytes), index));
}

GameplayMocapResource load_gameplay_mocap(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("missing gameplay mocap: " + path.string());
    const auto length = input.tellg();
    if (length < 8 || static_cast<std::uintmax_t>(length) > UINT32_MAX ||
        static_cast<std::uintmax_t>(length) > std::numeric_limits<std::size_t>::max() ||
        static_cast<std::uintmax_t>(length) > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::runtime_error("unsupported gameplay mocap file size");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("gameplay mocap read failed or file size changed");
    return decode_gameplay_mocap(std::move(bytes));
}

} // namespace nba97

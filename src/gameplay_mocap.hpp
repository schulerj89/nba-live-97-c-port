#pragma once
#include "recovered/gameplay_mocap.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace nba97 {

class GameplayMocap;
using GameplayMocapResource = std::shared_ptr<const GameplayMocap>;

// Each resource is immutable and owns both original bytes and its cached index.
// Copies of the shared owner retain a generation across scene replacement. Any
// borrowed references below require retaining that owner; no naked PS1 pointers.
class GameplayMocap final {
public:
    const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
    const Nba97GameMocapIndex& index() const noexcept { return index_; }
    // A null entry returns nullptr. An invalid channel/slot throws out_of_range.
    const Nba97GameMocapHeader* header(std::size_t channel, std::size_t slot) const;
private:
    GameplayMocap(std::vector<std::uint8_t> bytes, const Nba97GameMocapIndex& index);
    std::vector<std::uint8_t> bytes_;
    Nba97GameMocapIndex index_;
    friend GameplayMocapResource decode_gameplay_mocap(std::vector<std::uint8_t> bytes);
};

// Raw disk encoding only. Failures throw before publishing a resource, so
// `current = load_gameplay_mocap(path)` retains current if loading fails.
GameplayMocapResource decode_gameplay_mocap(std::vector<std::uint8_t> bytes);
GameplayMocapResource load_gameplay_mocap(const std::filesystem::path& path);

} // namespace nba97

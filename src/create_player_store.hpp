#pragma once

#include "recovered/create_player.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nba97 {

enum class CreatedPlayerLoadStatus { NewStore, Loaded, RecoveredBackup };

class CreatedPlayerStore final {
public:
    CreatedPlayerLoadStatus load(const std::filesystem::path& path,
                                 Nba97CreatedPlayerCatalog& catalog);
    bool save(const Nba97CreatedPlayerCatalog& catalog);
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    bool readFile(const std::filesystem::path&, Nba97CreatedPlayerCatalog&,
                  std::uint64_t&, std::string& error) const;
    void writeAtomically(const std::vector<std::uint8_t>& bytes) const;
    std::vector<std::uint8_t> serialize(const Nba97CreatedPlayerCatalog&,
                                        std::uint64_t generation) const;

    std::filesystem::path path_;
    Nba97CreatedPlayerCatalog accepted_{};
    std::uint64_t generation_ = 0;
    bool loaded_ = false;
};

} // namespace nba97

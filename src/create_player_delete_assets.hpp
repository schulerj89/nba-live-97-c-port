#pragma once

#include "frontend_help.hpp"
#include "recovered/roster_reset.h"

#include <filesystem>
#include <map>

namespace nba97 {

class CreatePlayerDeleteAssets final {
public:
    explicit CreatePlayerDeleteAssets(const std::filesystem::path& root);
    explicit CreatePlayerDeleteAssets(const std::vector<std::uint8_t>& bytes);

    [[nodiscard]] Nba97HelpRect rect(std::uint32_t address) const;
    void draw(PshImage&, std::uint32_t address, const std::string& team,
              const Nba97ResetPrompt&) const;

private:
    struct Dialog {
        Nba97HelpRect rect{};
        std::vector<std::string> body;
        std::vector<std::string> choices;
    };
    std::map<std::uint32_t, Dialog> dialogs_;
    PshFont font_;
};

} // namespace nba97

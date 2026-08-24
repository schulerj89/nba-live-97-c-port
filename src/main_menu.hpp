#pragma once

#include "psh_font.hpp"
#include "psh_image.hpp"
#include "frontend_settings.hpp"
#include "roster_database.hpp"
#include "user_profiles.hpp"

#include <cstdint>
#include <array>
#include <string>
#include <unordered_map>

namespace nba97 {

enum class MenuRow : std::uint8_t { GameOptions, FrontendButtons };

class MainMenu final {
public:
    void reset() noexcept;
    void setActiveUserProfiles(int count) noexcept;
    bool moveHorizontal(int direction) noexcept;
    bool moveVertical(int direction) noexcept;
    bool hover(int psx_x, int psx_y) noexcept;

    [[nodiscard]] MenuRow row() const noexcept { return row_; }
    [[nodiscard]] int selection() const noexcept;
    [[nodiscard]] const char* selectedLabel() const noexcept;
    [[nodiscard]] bool buttonEnabled(int index) const noexcept;

private:
    MenuRow row_ = MenuRow::GameOptions;
    int option_ = 0;
    int button_ = 0;
    int active_user_profiles_ = 0;
};

class RecoveredBottomMenu final {
public:
    void open(FrontendPage page) noexcept;
    void setSelected(int selected) noexcept;
    bool move(int horizontal, int vertical) noexcept;
    bool hover(int psx_x, int psx_y) noexcept;

    [[nodiscard]] FrontendPage page() const noexcept { return page_; }
    [[nodiscard]] int selected() const noexcept { return selected_; }
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] const char* selectedLabel() const noexcept;

private:
    FrontendPage page_ = FrontendPage::Rosters;
    int selected_ = 0;
};

enum class RosterViewMode : std::uint8_t { TeamRoster, PlayerCard };

class RosterViewer final {
public:
    void open(const RosterDatabase& database) noexcept;
    bool move(int horizontal, int vertical, const RosterDatabase& database) noexcept;
    bool hover(int psx_x, int psx_y, const RosterDatabase& database) noexcept;
    void activate(const RosterDatabase& database) noexcept;
    bool back() noexcept;

    [[nodiscard]] RosterViewMode mode() const noexcept { return mode_; }
    [[nodiscard]] std::size_t teamIndex() const noexcept { return team_index_; }
    [[nodiscard]] std::size_t playerIndex() const noexcept { return player_index_; }
    [[nodiscard]] int detailPage() const noexcept { return detail_page_; }
    [[nodiscard]] const TeamRecord* selectedTeam(const RosterDatabase& database) const noexcept;
    [[nodiscard]] const PlayerRecord* selectedPlayer(const RosterDatabase& database) const noexcept;

private:
    void clamp(const RosterDatabase& database) noexcept;
    RosterViewMode mode_ = RosterViewMode::TeamRoster;
    std::size_t team_index_ = 0;
    std::size_t player_index_ = 0;
    int detail_page_ = 0;
};

using MenuSpritePack = std::unordered_map<std::string, PshImage>;
using MenuCardPack = std::array<PshImage, 4>;

PshImage renderGameSetupMenu(const MainMenu& menu, const PshImage& title_source,
                             const PshFont& font, const MenuSpritePack& sprites,
                             const MenuCardPack& cards,
                             std::uint32_t elapsed_ms);

PshImage renderSettingsMenu(const SettingsMenu& menu,
                            const FrontendSettings& settings,
                            const PshFont& font,
                            const MenuSpritePack& sprites,
                            std::uint32_t elapsed_ms);

PshImage renderRecoveredBottomMenu(const RecoveredBottomMenu& menu,
                                   const PshFont& font,
                                   const MenuSpritePack& zset1,
                                   const MenuSpritePack& zset4,
                                   const MenuSpritePack& zset7,
                                   std::uint32_t elapsed_ms);

PshImage renderUserProfileSetup(const UserProfileMenu& menu,
                                const UserProfileStore& store,
                                const PshFont& font,
                                const MenuSpritePack& sprites,
                                std::uint32_t elapsed_ms);

PshImage renderRosterViewer(const RosterViewer& viewer,
                            const RosterDatabase& database,
                            const PshFont& font,
                            const MenuSpritePack& sprites,
                            std::uint32_t elapsed_ms);

} // namespace nba97

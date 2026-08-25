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
    void setRosterCapabilities(bool roster_modified, bool injuries_present) noexcept;
    bool move(int horizontal, int vertical) noexcept;
    bool hover(int psx_x, int psx_y) noexcept;

    [[nodiscard]] FrontendPage page() const noexcept { return page_; }
    [[nodiscard]] int selected() const noexcept { return selected_; }
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] const char* selectedLabel() const noexcept;
    [[nodiscard]] bool enabled(int index) const noexcept;

private:
    FrontendPage page_ = FrontendPage::Rosters;
    int selected_ = 0;
    bool roster_modified_ = false;
    bool injuries_present_ = false;
};

enum class RosterViewMode : std::uint8_t { TeamRoster, PlayerCard };

class RosterViewer final {
public:
    void open(const RosterDatabase& database) noexcept;
    bool move(int horizontal, int vertical, const RosterDatabase& database,
              std::uint32_t elapsed_ms = 0) noexcept;
    bool cycleCategory(int direction) noexcept;
    bool cycleDisplay(int direction) noexcept;
    bool scanTeam(int direction, const RosterDatabase& database,
                  std::uint32_t elapsed_ms = 0) noexcept;
    bool hover(int psx_x, int psx_y, const RosterDatabase& database) noexcept;
    void activate(const RosterDatabase& database) noexcept;
    void returnToRoster() noexcept;
    void toggleHelp() noexcept { help_visible_ = !help_visible_; }
    void dismissHelp() noexcept { help_visible_ = false; }
    void commit() noexcept;
    void cancel() noexcept;

    [[nodiscard]] RosterViewMode mode() const noexcept { return mode_; }
    [[nodiscard]] bool helpVisible() const noexcept { return help_visible_; }
    [[nodiscard]] std::size_t teamIndex() const noexcept { return team_index_; }
    [[nodiscard]] std::size_t playerIndex() const noexcept { return player_index_; }
    [[nodiscard]] std::size_t firstVisiblePlayer() const noexcept { return first_visible_player_; }
    [[nodiscard]] std::size_t paletteFromTeamIndex() const noexcept {
        return palette_from_team_index_;
    }
    [[nodiscard]] std::uint32_t paletteTransitionStartMs() const noexcept {
        return palette_transition_start_ms_;
    }
    [[nodiscard]] std::size_t scrollFromFirstPlayer() const noexcept {
        return scroll_from_first_player_;
    }
    [[nodiscard]] std::uint32_t scrollTransitionStartMs() const noexcept {
        return scroll_transition_start_ms_;
    }
    [[nodiscard]] std::size_t firstVisiblePlayerStat() const noexcept {
        return first_visible_player_stat_;
    }
    [[nodiscard]] std::size_t playerStatCount() const noexcept;
    [[nodiscard]] int category() const noexcept { return category_; }
    [[nodiscard]] int displayIndex() const noexcept {
        return display_by_category_[static_cast<std::size_t>(category_)];
    }
    [[nodiscard]] const TeamRecord* selectedTeam(const RosterDatabase& database) const noexcept;
    [[nodiscard]] const PlayerRecord* selectedPlayer(const RosterDatabase& database) const noexcept;

private:
    void clamp(const RosterDatabase& database) noexcept;
    RosterViewMode mode_ = RosterViewMode::TeamRoster;
    std::size_t team_index_ = 0;
    std::size_t player_index_ = 0;
    std::size_t first_visible_player_ = 0;
    std::size_t palette_from_team_index_ = 0;
    std::uint32_t palette_transition_start_ms_ = 0;
    std::size_t scroll_from_first_player_ = 0;
    std::uint32_t scroll_transition_start_ms_ = 0;
    int category_ = 2;
    std::array<int, 6> display_by_category_{0, 15, 32, 32, 44, 44};
    std::size_t entry_team_index_ = 0;
    std::size_t entry_player_index_ = 0;
    std::size_t entry_first_visible_player_ = 0;
    std::size_t first_visible_player_stat_ = 0;
    bool help_visible_ = false;
};

using MenuSpritePack = std::unordered_map<std::string, PshImage>;
using MenuCardPack = std::array<PshImage, 4>;
using RosterCardPack = std::array<PshImage, 8>;

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
                                   const RosterCardPack& roster_cards,
                                   std::uint32_t elapsed_ms,
                                   bool selected_overlay_visible = true);

PshImage renderUserProfileSetup(const UserProfileMenu& menu,
                                const UserProfileStore& store,
                                const PshFont& font,
                                const MenuSpritePack& sprites,
                                std::uint32_t elapsed_ms);

PshImage renderRosterViewer(const RosterViewer& viewer,
                            const RosterDatabase& database,
                            const PshFont& font,
                            const MenuSpritePack& sprites,
                            std::uint32_t elapsed_ms,
                            const PshImage* player_portrait = nullptr,
                            bool cool_facts_available = false,
                            const PshFont* control_font = nullptr,
                            int stat_flash_direction = 0,
                            bool cool_fact_playing = false);

} // namespace nba97

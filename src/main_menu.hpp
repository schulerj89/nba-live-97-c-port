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

struct RosterViewerConstructionContext {
    // Original global +0x78. Values 1/2 select recovered layers 4/5 only
    // while the active-roster byte at +0x2FC3 is set.
    int special_stat_layer = 0;
    bool special_roster_active = false;
    // FUN_800590B8 copies param_2 into both viewer object-state bytes.
    bool object_state_flag = false;
    // FUN_80059610 omits layer 4 while frontend roster byte +0x2F61 is set.
    bool layer_four_restricted = false;
};

struct RosterStatLayerChangeState {
    int input_mask = 0;
    int previous_layer = 2;
    int current_layer = 2;
    bool skipped_layer_four = false;
    int sound_slot = 0;
    int descriptor_table = 2;
    int descriptor_last_index = 23;
    bool descriptor_extent_changed = false;
    bool primary_animation_reset = false;
    int primary_refresh_count = 0;
    bool secondary_layout = false;
    bool secondary_animation_reset = false;
    int secondary_refresh_count = 0;
    int saved_controller_page = 0;
    bool controller_page_zeroed_for_rebuild = false;
    bool layout_rebuilt = false;
};

struct RosterPlayerCycleContext {
    // DAT_80022088+0x0F chooses which mirrored frontend descriptor page is
    // active. Page zero uses header objects 0x18..0x1A; all others 0x1E..0x20.
    std::uint8_t active_page = 0;
    bool special_roster_descriptor = false;
    // Frontend word +0x708 == 1 blocks cycling descriptor 29.
    bool special_cycle_locked = false;
    int visible_row_start = 0;
    int visible_row_count = 6;
    int layout_id = 0x24;
};

struct RosterPlayerCycleState {
    int input_mask = 0;
    std::size_t previous_slot = 0;
    std::size_t current_slot = 0;
    std::size_t roster_count = 0;
    bool wrapped = false;
    bool blocked_special_roster = false;
    bool input_latch_cleared = false;
    std::uint16_t resolved_player_id = 0xffffu;
    bool global_player_id_updated = false;
    int transition_frames = 0;
    int descriptor_page = 0;
    int header_refresh_start = 0;
    int header_refresh_count = 0;
    int visible_row_refresh_start = 0;
    int visible_row_refresh_count = 0;
    bool cool_fact_stopped = false;
    bool player_card_refreshed = false;
    bool stat_scroll_preserved = false;
};

struct RosterViewerControllerBinding {
    int slot = 0;
    std::uint32_t flags = 0;
    int object_id = 0;
    int value = 0;
    bool bound = false;
};

struct RosterViewerPersistentState {
    std::int16_t player_index = 0;
    std::int16_t first_visible_player = 0;
    std::int16_t team_index = 0;
};

struct RosterViewerRunContext {
    RosterViewerPersistentState saved{};
    int special_stat_layer = 0;
    bool special_roster_active = false;
    std::uint8_t active_team_index = 0;
    bool layer_four_restricted = false;
};

class RosterViewer final {
public:
    void open(const RosterDatabase& database) noexcept;
    void construct(const RosterDatabase& database,
                   RosterViewerConstructionContext context) noexcept;
    void runViewer(const RosterDatabase& database,
                   RosterViewerRunContext context) noexcept;
    bool move(int horizontal, int vertical, const RosterDatabase& database,
              std::uint32_t elapsed_ms = 0) noexcept;
    bool cyclePlayer(int direction, const RosterDatabase& database,
                     RosterPlayerCycleContext context = {}) noexcept;
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
    void finishRun(int result_code, int exit_status) noexcept;

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
    [[nodiscard]] int displayIndexForCategory(std::size_t category) const noexcept {
        return category < display_by_category_.size() ? display_by_category_[category] : -1;
    }
    [[nodiscard]] const RosterStatLayerChangeState& lastStatLayerChange() const noexcept {
        return last_stat_layer_change_;
    }
    [[nodiscard]] const RosterPlayerCycleState& lastPlayerCycle() const noexcept {
        return last_player_cycle_;
    }
    [[nodiscard]] int constructionLayerLimit() const noexcept {
        return construction_layer_limit_;
    }
    [[nodiscard]] int constructionDescriptorCount() const noexcept {
        return construction_descriptor_count_;
    }
    [[nodiscard]] int constructedDescriptorIndex() const noexcept {
        return constructed_descriptor_index_;
    }
    [[nodiscard]] bool constructionObjectFlagsAgree() const noexcept {
        return construction_object_flags_[0] == construction_object_flags_[1];
    }
    [[nodiscard]] bool constructionObjectStateFlag() const noexcept {
        return construction_object_flags_[0];
    }
    [[nodiscard]] bool constructionControllerBound() const noexcept {
        return construction_controller_.bound;
    }
    [[nodiscard]] int constructionControllerPhase() const noexcept {
        return construction_controller_phase_;
    }
    [[nodiscard]] const RosterViewerControllerBinding& constructionController() const noexcept {
        return construction_controller_;
    }
    [[nodiscard]] RosterViewerPersistentState savedRunState() const noexcept {
        return {static_cast<std::int16_t>(entry_player_index_),
                static_cast<std::int16_t>(entry_first_visible_player_),
                static_cast<std::int16_t>(entry_team_index_)};
    }
    [[nodiscard]] int runInputState() const noexcept { return run_input_state_; }
    [[nodiscard]] int runCancelSentinel() const noexcept { return run_cancel_sentinel_; }
    [[nodiscard]] bool runDrawCallbackBound() const noexcept {
        return run_draw_callback_bound_;
    }
    [[nodiscard]] int lastRunResult() const noexcept { return last_run_result_; }
    [[nodiscard]] int lastRunExitStatus() const noexcept { return last_run_exit_status_; }
    [[nodiscard]] const TeamRecord* selectedTeam(const RosterDatabase& database) const noexcept;
    [[nodiscard]] const PlayerRecord* selectedPlayer(const RosterDatabase& database) const noexcept;

private:
    void clamp(const RosterDatabase& database) noexcept;
    void constructViewer(const RosterDatabase& database,
                         RosterViewerConstructionContext context) noexcept;
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
    int construction_layer_limit_ = 3;
    int construction_descriptor_count_ = 28;
    int constructed_descriptor_index_ = 32;
    std::array<bool, 2> construction_object_flags_{};
    RosterViewerControllerBinding construction_controller_{};
    int construction_controller_phase_ = 0;
    bool layer_four_restricted_ = false;
    int stat_descriptor_last_index_ = 23;
    // View Player uses layout 0x23 and six visible descriptor rows. These
    // replace the raw DAT_80022088 byte fields consumed by FUN_80059610.
    int stat_layout_id_ = 0x23;
    int stat_visible_row_count_ = 6;
    int stat_controller_page_ = 0;
    RosterStatLayerChangeState last_stat_layer_change_{};
    RosterPlayerCycleState last_player_cycle_{};
    std::size_t entry_team_index_ = 0;
    std::size_t entry_player_index_ = 0;
    std::size_t entry_first_visible_player_ = 0;
    std::size_t first_visible_player_stat_ = 0;
    bool help_visible_ = false;
    int run_input_state_ = 0x10;
    int run_cancel_sentinel_ = 0x100;
    bool run_draw_callback_bound_ = false;
    int last_run_result_ = 0;
    int last_run_exit_status_ = 0;
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

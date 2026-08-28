#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace nba97 {

enum class FrontendPage : std::uint8_t {
    GameSetup,
    ProfileSetup,
    Rules,
    Options,
    Rosters,
    ViewRosters,
    Users,
    Card,
    ReorderRosters,
    TradePlayers,
    SignFreeAgent,
    ReleasePlayers
};

class FrontendSettings final {
public:
    FrontendSettings();

    bool load(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;

    [[nodiscard]] std::uint8_t rule(int index) const noexcept;
    [[nodiscard]] std::uint8_t option(int index) const noexcept;
    [[nodiscard]] std::uint8_t style() const noexcept { return style_; }
    [[nodiscard]] std::string ruleValue(int index) const;
    [[nodiscard]] std::string optionValue(int index) const;
    bool adjustRule(int index, int direction) noexcept;
    bool adjustOption(int index, int direction) noexcept;

private:
    void applyStyle(std::uint8_t style) noexcept;
    void classifyRules() noexcept;

    std::array<std::uint8_t, 14> rules_{};
    std::array<std::uint8_t, 14> custom_rules_{};
    std::array<std::uint8_t, 11> options_{};
    std::uint8_t style_ = 0;
};

class SettingsMenu final {
public:
    void open(FrontendPage page) noexcept;
    bool move(int direction) noexcept;
    bool hover(int psx_x, int psx_y) noexcept;

    [[nodiscard]] FrontendPage page() const noexcept { return page_; }
    [[nodiscard]] int selected() const noexcept { return selected_; }
    [[nodiscard]] int firstVisible() const noexcept { return first_visible_; }
    [[nodiscard]] int count() const noexcept;
    [[nodiscard]] int visibleCount() const noexcept;
    [[nodiscard]] const char* selectedLabel() const noexcept;

private:
    FrontendPage page_ = FrontendPage::GameSetup;
    int selected_ = 0;
    int first_visible_ = 0;
};

} // namespace nba97

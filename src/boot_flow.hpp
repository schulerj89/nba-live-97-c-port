#pragma once

#include <cstdint>

namespace nba97 {

enum class BootScreen : std::uint8_t {
    LoadScreen,
    LegalScreen,
    IntroVideo,
    TitleScreen,
    MainMenu,
};

class BootFlow final {
public:
    void reset() noexcept;
    [[nodiscard]] bool update(std::uint32_t delta_ms,
                              std::uint32_t transition_ms) noexcept;
    [[nodiscard]] bool completeIntro() noexcept;
    [[nodiscard]] bool enterMainMenu() noexcept;
    void requestAdvance(std::uint32_t transition_ms) noexcept;

    [[nodiscard]] BootScreen screen() const noexcept { return screen_; }
    [[nodiscard]] const char* screenName() const noexcept;

private:
    BootScreen screen_ = BootScreen::LoadScreen;
    std::uint32_t elapsed_ms_ = 0;
};

} // namespace nba97

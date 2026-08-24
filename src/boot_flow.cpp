#include "boot_flow.hpp"

namespace nba97 {

void BootFlow::reset() noexcept {
    screen_ = BootScreen::LoadScreen;
    elapsed_ms_ = 0;
}

bool BootFlow::update(std::uint32_t delta_ms,
                      std::uint32_t transition_ms) noexcept {
    elapsed_ms_ += delta_ms;
    if (elapsed_ms_ < transition_ms) return false;
    elapsed_ms_ = 0;
    if (screen_ == BootScreen::LoadScreen) screen_ = BootScreen::LegalScreen;
    else if (screen_ == BootScreen::LegalScreen) screen_ = BootScreen::IntroVideo;
    else return false;
    return true;
}

bool BootFlow::completeIntro() noexcept {
    if (screen_ != BootScreen::IntroVideo) return false;
    screen_ = BootScreen::TitleScreen;
    elapsed_ms_ = 0;
    return true;
}

bool BootFlow::enterMainMenu() noexcept {
    if (screen_ != BootScreen::TitleScreen) return false;
    screen_ = BootScreen::MainMenu;
    elapsed_ms_ = 0;
    return true;
}

void BootFlow::requestAdvance(std::uint32_t transition_ms) noexcept {
    elapsed_ms_ = transition_ms;
}

const char* BootFlow::screenName() const noexcept {
    switch (screen_) {
    case BootScreen::LoadScreen: return "load-screen";
    case BootScreen::LegalScreen: return "nba-legal-screen";
    case BootScreen::IntroVideo: return "intro-video";
    case BootScreen::TitleScreen: return "title-screen";
    case BootScreen::MainMenu: return "game-setup-menu";
    }
    return "unknown";
}

} // namespace nba97

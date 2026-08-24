#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace nba97 {

class IntroPlayer final {
public:
    enum class EventResult {
        None,
        Completed,
        UserAbort,
        Error,
    };

    static constexpr UINT kGraphEventMessage = WM_APP + 97;

    IntroPlayer();
    ~IntroPlayer();
    IntroPlayer(const IntroPlayer&) = delete;
    IntroPlayer& operator=(const IntroPlayer&) = delete;

    void start(HWND window, const std::filesystem::path& movie);
    void stop() noexcept;
    void resize() noexcept;
    void repaint(HDC device_context) noexcept;
    [[nodiscard]] EventResult handleGraphEvents() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] const std::vector<std::string>& filterNames() const noexcept;
    [[nodiscard]] long lastEventCode() const noexcept;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

} // namespace nba97

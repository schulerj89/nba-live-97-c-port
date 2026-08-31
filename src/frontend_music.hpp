#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include "ea_schl.hpp"

namespace nba97 {

class FrontendMusicPlayer final {
public:
    FrontendMusicPlayer();
    ~FrontendMusicPlayer();
    FrontendMusicPlayer(const FrontendMusicPlayer&) = delete;
    FrontendMusicPlayer& operator=(const FrontendMusicPlayer&) = delete;

    void start(const std::filesystem::path& cnk_path, std::uint8_t recovered_volume);
    void stop() noexcept;
    void setRecoveredVolume(std::uint8_t volume) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] std::string error() const;
    [[nodiscard]] unsigned underruns() const noexcept;
    [[nodiscard]] const EaSchlInfo& info() const noexcept { return info_; }
    [[nodiscard]] const std::string& decoderName() const noexcept { return decoder_name_; }

private:
    struct Stream;
    std::unique_ptr<Stream> stream_;
    EaSchlInfo info_{};
    std::string decoder_name_;
};

} // namespace nba97

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace nba97 {

struct EaSchlInfo {
    std::uint8_t version = 0, bits_per_sample = 0, channels = 0, codec = 0;
    std::uint32_t sample_rate = 0, sample_count = 0, data_blocks = 0;
};

class FrontendMusicPlayer final {
public:
    FrontendMusicPlayer() = default;
    ~FrontendMusicPlayer();
    FrontendMusicPlayer(const FrontendMusicPlayer&) = delete;
    FrontendMusicPlayer& operator=(const FrontendMusicPlayer&) = delete;

    void start(const std::filesystem::path& cnk_path, std::uint8_t recovered_volume);
    void stop() noexcept;
    void setRecoveredVolume(std::uint8_t volume) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept { return wave_out_ != nullptr; }
    [[nodiscard]] const EaSchlInfo& info() const noexcept { return info_; }
    [[nodiscard]] const std::string& decoderName() const noexcept { return decoder_name_; }

private:
    HWAVEOUT wave_out_ = nullptr;
    WAVEHDR header_{};
    DWORD previous_wave_volume_ = 0;
    bool restore_wave_volume_ = false;
    std::vector<std::int16_t> pcm_;
    EaSchlInfo info_{};
    std::string decoder_name_;
};

} // namespace nba97

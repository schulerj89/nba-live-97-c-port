#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
#include <mmsystem.h>

namespace nba97 {

struct RecoveredClipInfo {
    std::uint32_t record = 0;
    std::uint32_t sample_rate = 0;
    std::uint32_t sample_count = 0;
    std::uint32_t compressed_bytes = 0;
    std::string source;
    std::uint32_t program_volume = 127;
    std::uint32_t tone_volume = 127;
    std::uint32_t playback_volume = 127;
};

class RecoveredAudioPlayer final {
public:
    ~RecoveredAudioPlayer();
    RecoveredAudioPlayer(const RecoveredAudioPlayer&) = delete;
    RecoveredAudioPlayer& operator=(const RecoveredAudioPlayer&) = delete;
    RecoveredAudioPlayer() = default;

    RecoveredClipInfo playCursorSound(const std::filesystem::path& header,
                                      const std::filesystem::path& body,
                                      std::uint32_t sound_id);
    RecoveredClipInfo playCoolFact(const std::filesystem::path& index,
                                   const std::filesystem::path& archive,
                                   std::uint16_t player_id,
                                   std::uint32_t preferred_variant = 0xffffffffu);
    void stop() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] const RecoveredClipInfo& info() const noexcept { return info_; }

private:
    void playPcm(std::vector<std::int16_t> pcm, std::uint32_t sample_rate);
    HWAVEOUT wave_out_ = nullptr;
    WAVEHDR header_{};
    std::vector<std::int16_t> pcm_;
    RecoveredClipInfo info_{};
    std::uint32_t next_variant_ = 0;
};

} // namespace nba97

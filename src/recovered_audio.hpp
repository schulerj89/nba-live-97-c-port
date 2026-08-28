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
    std::int32_t pitch_cents = 0;
    std::uint32_t root_note = 60;
    std::uint32_t requested_note = 60;
    std::uint32_t rendered_sample_count = 0;
    bool playback_suppressed = false;
};

// One selected clip, decoded but not submitted. Ownership moves into playback;
// preparation/export never advances the gameplay selection or opens a device.
struct PreparedCoolFact {
    RecoveredClipInfo info;
    std::vector<std::int16_t> pcm;
};

class RecoveredAudioPlayer final {
public:
    ~RecoveredAudioPlayer();
    RecoveredAudioPlayer(const RecoveredAudioPlayer&) = delete;
    RecoveredAudioPlayer& operator=(const RecoveredAudioPlayer&) = delete;
    RecoveredAudioPlayer() = default;

    RecoveredClipInfo playCursorSound(const std::filesystem::path& header,
                                      const std::filesystem::path& body,
                                      std::uint32_t sound_id,
                                      std::uint8_t sfx_setting);
    RecoveredClipInfo exportCursorSound(const std::filesystem::path& header,
                                        const std::filesystem::path& body,
                                        std::uint32_t sound_id,
                                        const std::filesystem::path& output,
                                        std::uint8_t sfx_setting = 9);
    RecoveredClipInfo exportCursorSoundRaw(const std::filesystem::path& header,
                                           const std::filesystem::path& body,
                                           std::uint32_t sound_id,
                                           const std::filesystem::path& output,
                                           std::uint8_t sfx_setting = 9);
    PreparedCoolFact prepareCoolFact(const std::filesystem::path& index,
                                    const std::filesystem::path& archive,
                                    std::uint16_t player_id, std::uint32_t variant) const;
    RecoveredClipInfo startCoolFact(PreparedCoolFact prepared, std::uint8_t speech_setting);
    RecoveredClipInfo inspectCoolFact(const std::filesystem::path& index,
                                      const std::filesystem::path& archive,
                                      std::uint16_t player_id,
                                      std::uint32_t preferred_variant);
    RecoveredClipInfo exportCoolFact(const std::filesystem::path& index,
                                     const std::filesystem::path& archive,
                                     std::uint16_t player_id, std::uint32_t variant,
                                     const std::filesystem::path& output);
    // Raw export above is unchanged; this is the source-selected gain/pitch path.
    RecoveredClipInfo exportCoolFactPlayback(const std::filesystem::path& index,
                                            const std::filesystem::path& archive,
                                            std::uint16_t player_id, std::uint32_t variant,
                                            const std::filesystem::path& output,
                                            std::uint8_t speech_setting);
    void stop() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] const RecoveredClipInfo& info() const noexcept { return info_; }

private:
    RecoveredClipInfo loadCursorSound(const std::filesystem::path& header,
                                      const std::filesystem::path& body,
                                      std::uint32_t sound_id,
                                      bool play,
                                      const std::filesystem::path* output,
                                      bool apply_authored_pitch,
                                      std::uint8_t sfx_setting);
    static void applyCoolFactPlayback(PreparedCoolFact&, std::uint8_t speech_setting);
    void playPcm(std::vector<std::int16_t> pcm, std::uint32_t sample_rate);
    HWAVEOUT wave_out_ = nullptr;
    WAVEHDR header_{};
    std::vector<std::int16_t> pcm_;
    std::uint32_t wave_sample_rate_ = 0;
    RecoveredClipInfo info_{};
};

} // namespace nba97

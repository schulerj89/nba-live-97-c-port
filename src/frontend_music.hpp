#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include "ea_schl.hpp"
#include "recovered/music_routing.h"

namespace nba97 {

class FrontendMusicPlayer final {
public:
    FrontendMusicPlayer();
    ~FrontendMusicPlayer();
    FrontendMusicPlayer(const FrontendMusicPlayer&) = delete;
    FrontendMusicPlayer& operator=(const FrontendMusicPlayer&) = delete;

    void start(const std::filesystem::path& cnk_path, std::uint8_t recovered_volume);
    // UI-thread only. Caller owns the120Hz source clock and shared16-bit RNG.
    void startFrontend(const std::filesystem::path& music_directory,
                       std::uint8_t raw_option_volume, std::uint32_t source_clock);
    std::uint32_t updateFrontend(std::uint32_t source_clock, std::uint16_t& frontend_rng,
                                const Nba97MusicInputs& inputs);
    void requestSourceStop() noexcept;
    void overrideResource(unsigned index);
    void stop() noexcept;
    void setRecoveredVolume(std::uint8_t volume) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] std::string error() const;
    [[nodiscard]] unsigned underruns() const noexcept;
    [[nodiscard]] const EaSchlInfo& info() const noexcept { return info_; }
    [[nodiscard]] const std::string& decoderName() const noexcept { return decoder_name_; }
    [[nodiscard]] std::string currentResource() const;
    [[nodiscard]] std::uint32_t routingPhase() const noexcept;
    [[nodiscard]] std::uint64_t outputGeneration() const noexcept;
    [[nodiscard]] std::uint64_t sourceFrameLimit() const noexcept;

private:
    struct Output;
    struct Runtime;
    std::unique_ptr<Output> output_;
    std::unique_ptr<Runtime> runtime_;
    EaSchlInfo info_{};
    std::string decoder_name_;
    std::string failure_;
};

} // namespace nba97

#pragma once
#include "ea_schl.hpp"
#include "recovered/music_routing.h"
#include "recovered/music_stream.h"
#include "recovered/music_voice.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace nba97 {
constexpr std::uint64_t music_staging_frames = 1792;
struct MusicTrack {
    std::string filename;
    EaSchlPcm decoded;
    std::uint32_t full_blocks = 0;
    std::uint16_t partial_adpcm_bytes = 0;
};
struct MusicBank {
    std::array<std::shared_ptr<const MusicTrack>, 5> tracks;
    std::array<std::uint8_t, 16> slots{}; // indices, duplicates retained
};
std::shared_ptr<const MusicTrack> loadMusicTrack(const std::filesystem::path&);
MusicBank loadMusicBank(const std::filesystem::path& directory);

struct MusicOutputProgress {
    std::uint64_t generation = 0, completed_frames = 0;
    bool drained = false; // native driver ownership, never source FINISHED by itself
};
class MusicOutput {
public:
    virtual ~MusicOutput() = default;
    virtual void begin(std::shared_ptr<const MusicTrack>, std::uint64_t generation,
                       std::uint64_t frame_limit, unsigned gain) = 0;
    virtual void gain(unsigned) = 0;
    virtual void keyOff(std::uint64_t generation) = 0;
    virtual MusicOutputProgress progress() const = 0;
    virtual void retire() = 0;
};

// UI-thread owner. Worker/output threads never access recovered state or RNG.
class MusicPlayback final {
public:
    MusicPlayback(MusicBank bank, MusicOutput& output, std::uint8_t volume,
                  std::uint32_t source_clock);
    std::uint32_t update(std::uint32_t source_clock, std::uint16_t& frontend_rng,
                         const Nba97MusicInputs& inputs);
    void setRecoveredVolume(std::uint8_t);
    void requestSourceStop() noexcept { routing_.phase = 10; }
    void overrideResource(unsigned index); // explicit recovered caller input, zero-based
    const Nba97MusicRouting& routing() const noexcept { return routing_; }
    const Nba97MusicVoiceClock& clock() const noexcept { return clock_; }
    const Nba97MusicCompletion& completion() const noexcept { return completion_; }
    const Nba97MusicStreamDrain& drain() const noexcept { return drain_; }
    const MusicTrack* track() const noexcept { return selected_.get(); }
    std::uint64_t outputGeneration() const noexcept { return output_generation_; }
    std::uint64_t frameLimit() const noexcept { return frame_limit_; }
    int rawStreamStatus() const noexcept { return nba97_music_stream_status(stream_flags_, stream_pending_); }
private:
    static std::uint32_t routeCall(void*, Nba97MusicCall, std::uint32_t,
        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
    static std::uint32_t voiceCall(void*, Nba97MusicVoiceCall, std::uint32_t,
        std::uint32_t, std::uint32_t);
    void hardwareService();
    void slotEntry();
    void startStream();
    void beginOutput();
    void applyGain();
    MusicBank bank_;
    MusicOutput& output_;
    Nba97MusicResources resources_{};
    Nba97MusicRouting routing_{};
    Nba97MusicVoiceClock clock_{};
    std::array<Nba97MusicVoice, 24> voices_{};
    Nba97MusicCompletion completion_{};
    Nba97MusicStreamDrain drain_{};
    std::shared_ptr<const MusicTrack> selected_;
    std::uint32_t now_, previous_clock_;
    std::uint16_t slots_ = 400;
    std::uint64_t output_generation_ = 0, next_slot_ = 1, frame_limit_ = 0;
    bool output_pending_ = false, output_started_ = false, keyoff_ = false;
    std::uint8_t stream_flags_ = 0, stream_pending_ = 0; // source C6CAC/C6CAD
};
} // namespace nba97

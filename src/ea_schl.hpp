#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace nba97 {
struct EaSchlInfo {
    std::uint8_t version=0, bits_per_sample=0, channels=0, codec=0;
    std::uint32_t sample_rate=0, sample_count=0, data_blocks=0;
};
struct EaSchlPcm {
    EaSchlInfo info;
    std::vector<std::int16_t> samples; // Interleaved left/right, before music gain.
};
// Portable fixed SCHl/PATl/TMxl codec6 decoder used by the native player.
// No audio device, looping policy or original stream-controller timing here.
EaSchlPcm decodeEaSchl(const std::vector<std::uint8_t>& bytes);
EaSchlPcm loadEaSchl(const std::filesystem::path& path);
}

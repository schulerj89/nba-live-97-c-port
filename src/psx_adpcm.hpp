#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nba97 {

// Decode one continuous mono PlayStation ADPCM stream. Each 16-byte frame
// produces 28 signed 16-bit PCM samples and carries predictor history forward.
std::vector<std::int16_t> decodePsxAdpcmMono(const std::uint8_t* source,
                                             std::size_t bytes,
                                             std::size_t sample_limit = 0);

} // namespace nba97

#include "psx_adpcm.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace nba97 {

std::vector<std::int16_t> decodePsxAdpcmMono(const std::uint8_t* source,
                                             std::size_t bytes,
                                             std::size_t sample_limit) {
    static constexpr std::array<std::array<int, 2>, 5> coefficients{{
        {{0, 0}}, {{60, 0}}, {{115, -52}}, {{98, -55}}, {{122, -60}}}};
    if (!source || bytes % 16)
        throw std::runtime_error("PSX ADPCM stream is not frame aligned");
    std::vector<std::int16_t> output;
    output.reserve(bytes / 16 * 28);
    std::int32_t h1 = 0, h2 = 0;
    for (std::size_t frame = 0; frame < bytes; frame += 16) {
        const unsigned shift = source[frame] & 0x0f;
        const unsigned filter = source[frame] >> 4;
        if (filter >= coefficients.size() || shift > 12)
            throw std::runtime_error("invalid PSX ADPCM frame header");
        for (unsigned sample = 0; sample < 28; ++sample) {
            const std::uint8_t packed = source[frame + 2 + sample / 2];
            std::int32_t nibble = (sample & 1) ? (packed >> 4) : (packed & 0x0f);
            if (nibble & 8) nibble -= 16;
            std::int32_t decoded = (nibble << 12) >> shift;
            decoded += (h1 * coefficients[filter][0] +
                        h2 * coefficients[filter][1] + 32) >> 6;
            decoded = (std::max)(-32768, (std::min)(32767, decoded));
            output.push_back(static_cast<std::int16_t>(decoded));
            h2 = h1;
            h1 = decoded;
            if (sample_limit && output.size() == sample_limit) return output;
        }
    }
    return output;
}

} // namespace nba97

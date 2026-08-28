#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace nba97 {
// Native stream adapter: integer linear gain, independent of Windows session
// volume. Call only for buffers returned by the device; never edit queued PCM.
inline void fillMusicPcm(const std::vector<std::int16_t>& source, std::size_t& position,
                         std::int16_t* output, std::size_t samples, unsigned gain) {
    if(source.empty() || source.size()%2 || position>=source.size() || position%2 || samples%2 || (!output && samples))
        throw std::invalid_argument("invalid stereo music loop extent");
    gain=(std::min)(gain,127u);
    for(std::size_t i=0;i<samples;++i) {
        output[i]=static_cast<std::int16_t>(static_cast<std::int32_t>(source[position])*static_cast<int>(gain)/127);
        if(++position==source.size()) position=0;
    }
}
}

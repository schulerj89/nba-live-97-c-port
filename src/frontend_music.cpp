#include "frontend_music.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace nba97 {
namespace {
std::uint16_t u16(const std::vector<std::uint8_t>& b, std::size_t p) {
    if (p + 2 > b.size()) throw std::runtime_error("truncated SCHl u16");
    return static_cast<std::uint16_t>(b[p] | (b[p + 1] << 8));
}
std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t p) {
    if (p + 4 > b.size()) throw std::runtime_error("truncated SCHl u32");
    return static_cast<std::uint32_t>(b[p]) |
        (static_cast<std::uint32_t>(b[p + 1]) << 8) |
        (static_cast<std::uint32_t>(b[p + 2]) << 16) |
        (static_cast<std::uint32_t>(b[p + 3]) << 24);
}
bool tag(const std::vector<std::uint8_t>& b, std::size_t p, const char* value) {
    return p + 4 <= b.size() && std::memcmp(b.data() + p, value, 4) == 0;
}
std::int16_t clamp16(std::int32_t value) {
    return static_cast<std::int16_t>((std::max)(-32768, (std::min)(32767, value)));
}

void decodePsxChannel(const std::uint8_t* source, std::size_t bytes,
                      std::vector<std::int16_t>& output, std::int32_t& h1, std::int32_t& h2) {
    static constexpr std::array<std::array<int, 2>, 5> coefficients{{
        {{0, 0}}, {{60, 0}}, {{115, -52}}, {{98, -55}}, {{122, -60}}}};
    if (bytes % 16) throw std::runtime_error("PSX ADPCM channel block is not frame aligned");
    output.clear(); output.reserve(bytes / 16 * 28);
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
            decoded += (h1 * coefficients[filter][0] + h2 * coefficients[filter][1] + 32) >> 6;
            decoded = clamp16(decoded);
            output.push_back(static_cast<std::int16_t>(decoded));
            h2 = h1; h1 = decoded;
        }
    }
}

std::vector<std::int16_t> decodeSchl(const std::filesystem::path& path, EaSchlInfo& info) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing private frontend music: " + path.string());
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
    if (data.size() < 0x80 || !tag(data, 0, "SCHl") || !tag(data, 0x0c, "PATl") ||
        !tag(data, 0x44, "TMxl"))
        throw std::runtime_error("unsupported EA fixed SCHl/TMxl stream");
    const auto header_size = u32(data, 4);
    if (header_size < 0x80 || header_size > data.size())
        throw std::runtime_error("invalid SCHl header size");
    info.version = data[0x48]; info.bits_per_sample = data[0x49];
    info.channels = data[0x4a]; info.codec = data[0x4b];
    info.sample_rate = u16(data, 0x4e); info.sample_count = u32(data, 0x54);
    if (info.bits_per_sample != 16 || info.channels != 2 || info.codec != 0x06 ||
        info.sample_rate == 0 || info.sample_count == 0)
        throw std::runtime_error("SCHl is not the recovered 16-bit stereo PSX ADPCM variant");

    std::vector<std::int16_t> pcm;
    pcm.reserve(static_cast<std::size_t>(info.sample_count) * info.channels);
    std::array<std::int32_t, 2> h1{}, h2{};
    std::array<std::vector<std::int16_t>, 2> channels;
    for (std::size_t offset = header_size; offset + 8 <= data.size();) {
        const auto size = u32(data, offset + 4);
        if (size < 8 || size > data.size() - offset)
            throw std::runtime_error("invalid SCHl chunk boundary");
        if (tag(data, offset, "SCEl")) break;
        if (tag(data, offset, "SCDl")) {
            if (size < 0x10 || ((size - 0x10) % info.channels) != 0)
                throw std::runtime_error("invalid SCDl payload size");
            const std::size_t per_channel = (size - 0x10) / info.channels;
            for (std::size_t channel = 0; channel < info.channels; ++channel)
                decodePsxChannel(data.data() + offset + 0x10 + per_channel * channel,
                                 per_channel, channels[channel], h1[channel], h2[channel]);
            if (channels[0].size() != channels[1].size())
                throw std::runtime_error("SCDl channel sample mismatch");
            const auto remaining = info.sample_count - static_cast<std::uint32_t>(pcm.size() / 2);
            const std::size_t samples = (std::min<std::size_t>)(channels[0].size(), remaining);
            for (std::size_t i = 0; i < samples; ++i) {
                pcm.push_back(channels[0][i]); pcm.push_back(channels[1][i]);
            }
            ++info.data_blocks;
        }
        offset += size;
    }
    if (pcm.size() / info.channels != info.sample_count)
        throw std::runtime_error("SCHl decoded sample count does not match TMxl header");
    return pcm;
}
}

FrontendMusicPlayer::~FrontendMusicPlayer() { stop(); }

void FrontendMusicPlayer::start(const std::filesystem::path& cnk_path,
                                std::uint8_t recovered_volume) {
    stop();
    info_ = {};
    pcm_ = decodeSchl(cnk_path, info_);
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = info_.channels;
    format.nSamplesPerSec = info_.sample_rate; format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    MMRESULT result = waveOutOpen(&wave_out_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) { wave_out_ = nullptr; pcm_.clear(); throw std::runtime_error("waveOutOpen failed: " + std::to_string(result)); }
    // waveOutSetVolume may map to persistent per-device or per-application
    // state depending on the Windows audio driver. Preserve it so frontend
    // music and --self-test cannot leave the subsequent DirectShow movie mute.
    restore_wave_volume_ =
        waveOutGetVolume(wave_out_, &previous_wave_volume_) == MMSYSERR_NOERROR;
    header_ = {};
    header_.lpData = reinterpret_cast<LPSTR>(pcm_.data());
    header_.dwBufferLength = static_cast<DWORD>(pcm_.size() * sizeof(std::int16_t));
    header_.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP; header_.dwLoops = 0xffffffffu;
    result = waveOutPrepareHeader(wave_out_, &header_, sizeof(header_));
    if (result != MMSYSERR_NOERROR) { stop(); throw std::runtime_error("waveOutPrepareHeader failed: " + std::to_string(result)); }
    setRecoveredVolume(recovered_volume);
    result = waveOutWrite(wave_out_, &header_, sizeof(header_));
    if (result != MMSYSERR_NOERROR) { stop(); throw std::runtime_error("waveOutWrite failed: " + std::to_string(result)); }
    decoder_name_ = "native EA fixed SCHl/TMxl + PlayStation ADPCM -> WinMM PCM";
}

void FrontendMusicPlayer::stop() noexcept {
    if (wave_out_) {
        waveOutReset(wave_out_);
        if (header_.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(wave_out_, &header_, sizeof(header_));
        if (restore_wave_volume_)
            waveOutSetVolume(wave_out_, previous_wave_volume_);
        waveOutClose(wave_out_);
    }
    wave_out_ = nullptr; header_ = {}; previous_wave_volume_ = 0;
    restore_wave_volume_ = false; pcm_.clear(); decoder_name_.clear();
}

void FrontendMusicPlayer::setRecoveredVolume(std::uint8_t volume) noexcept {
    if (!wave_out_) return;
    const DWORD level = static_cast<DWORD>((std::min<unsigned>)(volume, 127) * 0xffffu / 127u);
    waveOutSetVolume(wave_out_, level | (level << 16));
}
} // namespace nba97

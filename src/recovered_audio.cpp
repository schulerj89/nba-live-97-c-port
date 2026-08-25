#include "recovered_audio.hpp"

#include "psx_adpcm.hpp"
#include "recovered/frontend_audio.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace nba97 {
namespace {
std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing private audio asset: " + path.string());
    return {(std::istreambuf_iterator<char>(input)), {}};
}
std::uint16_t u16(const std::vector<std::uint8_t>& b, std::size_t p) {
    if (p + 2 > b.size()) throw std::runtime_error("truncated audio u16");
    return static_cast<std::uint16_t>(b[p] | (b[p + 1] << 8));
}
std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t p) {
    if (p + 4 > b.size()) throw std::runtime_error("truncated audio u32");
    return static_cast<std::uint32_t>(b[p]) |
        (static_cast<std::uint32_t>(b[p + 1]) << 8) |
        (static_cast<std::uint32_t>(b[p + 2]) << 16) |
        (static_cast<std::uint32_t>(b[p + 3]) << 24);
}
bool tag(const std::vector<std::uint8_t>& b, std::size_t p, const char* value) {
    return p + 4 <= b.size() && std::memcmp(b.data() + p, value, 4) == 0;
}
void applyPsxGain(std::vector<std::int16_t>& pcm,
                  std::uint32_t program_volume,
                  std::uint32_t tone_volume,
                  std::uint32_t playback_volume) {
    constexpr std::int64_t kMaxVolume = 127;
    constexpr std::int64_t kDenominator = kMaxVolume * kMaxVolume * kMaxVolume;
    const std::int64_t numerator = static_cast<std::int64_t>(program_volume) *
        tone_volume * playback_volume;
    for (auto& sample : pcm) {
        const std::int64_t scaled = static_cast<std::int64_t>(sample) * numerator;
        sample = static_cast<std::int16_t>(scaled / kDenominator);
    }
}

void writePcmWav(const std::filesystem::path& path,
                 const std::vector<std::int16_t>& pcm,
                 std::uint32_t sample_rate) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write recovered WAV: " + path.string());
    const std::uint32_t data_size = static_cast<std::uint32_t>(pcm.size() * 2);
    const std::uint32_t riff_size = 36 + data_size;
    const std::uint16_t pcm_format = 1, channels = 1, bits = 16, block_align = 2;
    const std::uint32_t byte_rate = sample_rate * block_align;
    const auto write = [&output](const auto& value) {
        output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    output.write("RIFF", 4); write(riff_size); output.write("WAVEfmt ", 8);
    const std::uint32_t fmt_size = 16; write(fmt_size); write(pcm_format);
    write(channels); write(sample_rate); write(byte_rate); write(block_align);
    write(bits); output.write("data", 4); write(data_size);
    output.write(reinterpret_cast<const char*>(pcm.data()), data_size);
    if (!output) throw std::runtime_error("failed writing recovered WAV: " + path.string());
}
}

RecoveredAudioPlayer::~RecoveredAudioPlayer() { stop(); }

void RecoveredAudioPlayer::playPcm(std::vector<std::int16_t> pcm,
                                   std::uint32_t sample_rate) {
    // All ZCURSOR programs are 22.05 kHz. Reusing the WinMM device prevents
    // close/open transients from making identical repeated cursor cues sound
    // different. A new format still takes the full close/open path.
    if (wave_out_ && wave_sample_rate_ == sample_rate) {
        if (!(header_.dwFlags & WHDR_DONE)) waveOutReset(wave_out_);
        if (header_.dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(wave_out_, &header_, sizeof(header_));
        header_ = {};
        pcm_.clear();
    } else {
        stop();
    }
    pcm_ = std::move(pcm);
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = sample_rate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = sample_rate * 2;
    MMRESULT result = MMSYSERR_NOERROR;
    if (!wave_out_) {
        result = waveOutOpen(&wave_out_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            wave_out_ = nullptr;
            pcm_.clear();
            throw std::runtime_error("waveOutOpen recovered clip failed: " + std::to_string(result));
        }
        wave_sample_rate_ = sample_rate;
    }
    header_ = {};
    header_.lpData = reinterpret_cast<LPSTR>(pcm_.data());
    header_.dwBufferLength = static_cast<DWORD>(pcm_.size() * sizeof(std::int16_t));
    result = waveOutPrepareHeader(wave_out_, &header_, sizeof(header_));
    if (result != MMSYSERR_NOERROR) {
        stop();
        throw std::runtime_error("waveOutPrepareHeader recovered clip failed: " + std::to_string(result));
    }
    result = waveOutWrite(wave_out_, &header_, sizeof(header_));
    if (result != MMSYSERR_NOERROR) {
        stop();
        throw std::runtime_error("waveOutWrite recovered clip failed: " + std::to_string(result));
    }
}

RecoveredClipInfo RecoveredAudioPlayer::playCursorSound(
        const std::filesystem::path& header_path,
        const std::filesystem::path& body_path,
        std::uint32_t sound_id) {
    return loadCursorSound(header_path, body_path, sound_id, true, nullptr);
}

RecoveredClipInfo RecoveredAudioPlayer::exportCursorSound(
        const std::filesystem::path& header_path,
        const std::filesystem::path& body_path,
        std::uint32_t sound_id,
        const std::filesystem::path& output) {
    return loadCursorSound(header_path, body_path, sound_id, false, &output);
}

RecoveredClipInfo RecoveredAudioPlayer::loadCursorSound(
        const std::filesystem::path& header_path,
        const std::filesystem::path& body_path,
        std::uint32_t sound_id,
        bool play,
        const std::filesystem::path* output) {
    const auto header = readFile(header_path);
    const auto body = readFile(body_path);
    if (!tag(header, 0, "BNKl") || sound_id >= 12)
        throw std::runtime_error("unsupported ZCURSOR BNKl sound id");
    // BNKl stores a self-relative PATl pointer for each sound ID. Each PATl
    // then points to its 92-byte tone, whose serialized TMxl starts at +40.
    // Following that chain preserves the original PS1 program/tone gain.
    const std::size_t program_pointer = 12 + static_cast<std::size_t>(sound_id) * 4;
    const std::size_t patl = program_pointer + u32(header, program_pointer);
    if (patl + 16 > header.size() || !tag(header, patl, "PATl") ||
        header[patl + 7] != 1)
        throw std::runtime_error("invalid ZCURSOR PATl program");
    const std::size_t tone_pointer = patl + 12;
    const std::size_t tone = tone_pointer + u32(header, tone_pointer);
    const std::size_t tmxl = tone + 40;
    if (tone + 92 > header.size() || !tag(header, tmxl, "TMxl"))
        throw std::runtime_error("invalid ZCURSOR PATl tone mapping");
    const std::uint32_t sample_rate = u16(header, tmxl + 10);
    const std::uint32_t sample_count = u32(header, tmxl + 16);
    const std::uint32_t offset = u32(header, tmxl + 28);
    const std::uint32_t bytes = u32(header, tmxl + 36);
    if (!sample_rate || !sample_count || bytes % 16 || offset + bytes > body.size())
        throw std::runtime_error("invalid ZCURSOR TMxl range");
    const std::uint32_t program_volume = header[patl + 11];
    const std::uint32_t tone_volume = header[tone + 18];
    // FUN_8002F124 passes min(frontend SFX setting * 12, 127) to the bank
    // player. The recovered frontend initializes that setting to 9.
    const std::uint32_t playback_volume = nba97_frontend_sfx_volume(9);
    auto pcm = decodePsxAdpcmMono(body.data() + offset, bytes, sample_count);
    applyPsxGain(pcm, program_volume, tone_volume, playback_volume);
    if (output) writePcmWav(*output, pcm, sample_rate);
    if (play) playPcm(std::move(pcm), sample_rate);
    info_ = {sound_id, sample_rate, sample_count, bytes,
             header_path.filename().string() + "/" + body_path.filename().string()};
    info_.program_volume = program_volume;
    info_.tone_volume = tone_volume;
    info_.playback_volume = playback_volume;
    return info_;
}

RecoveredClipInfo RecoveredAudioPlayer::playCoolFact(
        const std::filesystem::path& index_path,
        const std::filesystem::path& archive_path,
        std::uint16_t player_id,
        std::uint32_t preferred_variant) {
    return loadCoolFact(index_path, archive_path, player_id, preferred_variant, true);
}

RecoveredClipInfo RecoveredAudioPlayer::inspectCoolFact(
        const std::filesystem::path& index_path,
        const std::filesystem::path& archive_path,
        std::uint16_t player_id,
        std::uint32_t preferred_variant) {
    return loadCoolFact(index_path, archive_path, player_id, preferred_variant, false);
}

RecoveredClipInfo RecoveredAudioPlayer::loadCoolFact(
        const std::filesystem::path& index_path,
        const std::filesystem::path& archive_path,
        std::uint16_t player_id,
        std::uint32_t preferred_variant,
        bool play) {
    const auto index = readFile(index_path);
    const std::uint32_t count = u32(index, 0);
    std::uint32_t variant = preferred_variant;
    if (variant >= 5) {
        bool found = false;
        for (std::uint32_t attempt = 0; attempt < 5; ++attempt) {
            const std::uint32_t candidate = (next_variant_ + attempt) % 5;
            const std::uint32_t record = static_cast<std::uint32_t>(player_id) * 5 + candidate;
            if (record < count && u32(index, 4 + static_cast<std::size_t>(record) * 8) != 0) {
                variant = candidate;
                next_variant_ = (candidate + 1) % 5;
                found = true;
                break;
            }
        }
        if (!found) throw std::runtime_error("player has no Cool Fact records");
    }
    const std::uint32_t record = static_cast<std::uint32_t>(player_id) * 5 + variant;
    if (record >= count) throw std::runtime_error("Cool Fact record is outside IDX");
    const std::size_t entry = 4 + static_cast<std::size_t>(record) * 8;
    const std::uint32_t bytes = u32(index, entry);
    const std::uint32_t offset = u32(index, entry + 4);
    if (!bytes) throw std::runtime_error("Cool Fact record absent");
    std::ifstream archive(archive_path, std::ios::binary);
    if (!archive) throw std::runtime_error("missing private audio asset: " + archive_path.string());
    archive.seekg(0, std::ios::end);
    const auto archive_size = archive.tellg();
    if (archive_size < 0 || static_cast<std::uint64_t>(offset) + bytes >
                              static_cast<std::uint64_t>(archive_size))
        throw std::runtime_error("Cool Fact record exceeds BIG archive");
    archive.seekg(offset, std::ios::beg);
    std::vector<std::uint8_t> clip(bytes);
    archive.read(reinterpret_cast<char*>(clip.data()), bytes);
    if (archive.gcount() != static_cast<std::streamsize>(bytes))
        throw std::runtime_error("Cool Fact record read was truncated");
    if (!tag(clip, 0, "PATl") || !tag(clip, 0x38, "TMxl") ||
        clip[0x3d] != 16 || clip[0x3e] != 1 || clip[0x3f] != 6)
        throw std::runtime_error("unsupported Cool Fact PATl/TMxl record");
    const std::uint32_t sample_rate = u16(clip, 0x42);
    const std::uint32_t sample_count = u32(clip, 0x48);
    // FUN_800314A0 passes PATl+0x74 to the SPU loader. The archive record
    // carries exactly two non-audio trailer bytes after the final ADPCM frame,
    // so derive the byte count from TMxl's authoritative sample count.
    constexpr std::size_t payload = 0x74;
    const std::size_t compressed =
        ((static_cast<std::size_t>(sample_count) + 27) / 28) * 16;
    if (!sample_rate || !sample_count || payload + compressed > clip.size() ||
        clip.size() - (payload + compressed) != 2)
        throw std::runtime_error("invalid Cool Fact PSX ADPCM payload");
    auto pcm = decodePsxAdpcmMono(clip.data() + payload, compressed, sample_count);
    if (play) playPcm(std::move(pcm), sample_rate);
    info_ = {record, sample_rate, sample_count, static_cast<std::uint32_t>(compressed),
             archive_path.filename().string() + " variant=" + std::to_string(variant)};
    return info_;
}

void RecoveredAudioPlayer::stop() noexcept {
    if (wave_out_) {
        waveOutReset(wave_out_);
        if (header_.dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(wave_out_, &header_, sizeof(header_));
        waveOutClose(wave_out_);
    }
    wave_out_ = nullptr;
    wave_sample_rate_ = 0;
    header_ = {};
    pcm_.clear();
}

bool RecoveredAudioPlayer::isPlaying() const noexcept {
    return wave_out_ && !(header_.dwFlags & WHDR_DONE);
}

} // namespace nba97

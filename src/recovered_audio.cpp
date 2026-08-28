#include "recovered_audio.hpp"

#include "psx_adpcm.hpp"
#include "cool_fact_index.hpp"
#include "recovered/frontend_audio.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>

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
std::int16_t s16(const std::vector<std::uint8_t>& b, std::size_t p) {
    return static_cast<std::int16_t>(u16(b, p));
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

std::vector<std::int16_t> applyPitchCents(const std::vector<std::int16_t>& input,
                                          std::int32_t pitch_cents) {
    if (input.empty() || pitch_cents == 0) return input;
    // FUN_80072048 applies cents to the SPU pitch register. Keep WinMM at the
    // common 22.05 kHz format and resample the PCM by the equivalent ratio so
    // switching cues cannot cause device close/open transients.
    const double ratio = std::pow(2.0, static_cast<double>(pitch_cents) / 1200.0);
    const auto output_size = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(input.size() / ratio)));
    std::vector<std::int16_t> output(output_size);
    for (std::size_t i = 0; i < output.size(); ++i) {
        const double source = static_cast<double>(i) * ratio;
        const auto source_index = static_cast<std::size_t>(source);
        const auto lower = source_index < input.size() ? source_index : input.size() - 1;
        const auto upper = lower + 1 < input.size() ? lower + 1 : input.size() - 1;
        const double fraction = source - static_cast<double>(lower);
        const double value = input[lower] + (input[upper] - input[lower]) * fraction;
        output[i] = static_cast<std::int16_t>(std::clamp(
            std::lround(value), static_cast<long>(-32768), static_cast<long>(32767)));
    }
    return output;
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
        std::uint32_t sound_id,
        std::uint8_t sfx_setting) {
    // 2F12C branches directly to return when 21D7E is zero: no bank read,
    // voice allocation or interruption of a cue already playing.
    if (!sfx_setting) {
        RecoveredClipInfo skipped;
        skipped.record = sound_id;
        skipped.playback_volume = 0;
        skipped.playback_suppressed = true;
        skipped.source = "FUN_8002F124 muted; bank/player not called";
        return skipped;
    }
    return loadCursorSound(header_path, body_path, sound_id, true, nullptr, true, sfx_setting);
}

RecoveredClipInfo RecoveredAudioPlayer::exportCursorSound(
        const std::filesystem::path& header_path,
        const std::filesystem::path& body_path,
        std::uint32_t sound_id,
        const std::filesystem::path& output,
        std::uint8_t sfx_setting) {
    return loadCursorSound(header_path, body_path, sound_id, false, &output, true, sfx_setting);
}

RecoveredClipInfo RecoveredAudioPlayer::exportCursorSoundRaw(
        const std::filesystem::path& header_path,
        const std::filesystem::path& body_path,
        std::uint32_t sound_id,
        const std::filesystem::path& output,
        std::uint8_t sfx_setting) {
    return loadCursorSound(header_path, body_path, sound_id, false, &output, false, sfx_setting);
}

RecoveredClipInfo RecoveredAudioPlayer::loadCursorSound(
        const std::filesystem::path& header_path,
        const std::filesystem::path& body_path,
        std::uint32_t sound_id,
        bool play,
        const std::filesystem::path* output,
        bool apply_authored_pitch,
        std::uint8_t sfx_setting) {
    const auto header = readFile(header_path);
    const auto body = readFile(body_path);
    if (header.size()<8 || !tag(header, 0, "BNKl"))
        throw std::runtime_error("invalid ZCURSOR BNKl header");
    // 91814: legacy bank version0 has128 slots; otherwise u16 at+6.
    // ZCURSOR has128 table slots and populated IDs1..12, not IDs0..11.
    const std::uint32_t program_count=header[4] ? u16(header,6):128;
    if (sound_id>=program_count || program_count>(header.size()-8)/4)
        throw std::runtime_error("unsupported ZCURSOR BNKl sound id");
    // FUN_80091814 indexes the BNKl table at bank + 8 + sound_id * 4. BNKl's
    // two-word header is therefore followed by the self-relative PATl pointer
    // for sound ID 0. Each PATl points to a 92-byte tone whose TMxl is at +40.
    const std::size_t program_pointer = 8 + static_cast<std::size_t>(sound_id) * 4;
    const auto relative_program=u32(header,program_pointer);
    if(!relative_program) throw std::runtime_error("unpopulated ZCURSOR BNKl sound id");
    const std::size_t patl = program_pointer + relative_program;
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
    if (!sample_rate || !sample_count || bytes % 16 || offset > body.size() || bytes > body.size()-offset)
        throw std::runtime_error("invalid ZCURSOR TMxl range");
    const std::uint32_t program_volume = header[patl + 11];
    const std::uint32_t tone_volume = header[tone + 18];
    // FUN_8009267C defaults its requested note to MIDI 60 when FUN_8009180C
    // supplies -1. It combines PATl and tone fine pitch, then subtracts 100
    // cents for every semitone between the tone root and requested note.
    constexpr std::uint32_t requested_note = 60;
    const std::uint32_t root_note = header[tone + 9];
    const std::int32_t pitch_cents = static_cast<std::int32_t>(s16(header, patl + 8)) +
        static_cast<std::int32_t>(s16(header, tone + 20)) -
        100 * (static_cast<std::int32_t>(root_note) -
               static_cast<std::int32_t>(requested_note));
    // FUN_8002F124 passes min(frontend SFX setting * 12, 127) to the bank
    // player. Runtime callers supply current Options SF/X volume; exports
    // default to the original first-boot setting (9) for stable comparisons.
    const std::uint32_t playback_volume = nba97_frontend_sfx_volume(sfx_setting);
    auto pcm = decodePsxAdpcmMono(body.data() + offset, bytes, sample_count);
    applyPsxGain(pcm, program_volume, tone_volume, playback_volume);
    if (apply_authored_pitch) pcm = applyPitchCents(pcm, pitch_cents);
    const std::uint32_t rendered_sample_count = static_cast<std::uint32_t>(pcm.size());
    if (output) writePcmWav(*output, pcm, sample_rate);
    if (play) playPcm(std::move(pcm), sample_rate);
    info_ = {sound_id, sample_rate, sample_count, bytes,
             header_path.filename().string() + "/" + body_path.filename().string()};
    info_.program_volume = program_volume;
    info_.tone_volume = tone_volume;
    info_.playback_volume = playback_volume;
    info_.pitch_cents = pitch_cents;
    info_.root_note = root_note;
    info_.requested_note = requested_note;
    info_.rendered_sample_count = rendered_sample_count;
    return info_;
}

void RecoveredAudioPlayer::applyCoolFactPlayback(PreparedCoolFact& prepared,
                                                std::uint8_t speech_setting) {
    if(prepared.pcm.empty() || !prepared.info.sample_rate ||
       prepared.pcm.size()!=prepared.info.sample_count)
        throw std::runtime_error("invalid prepared Cool Fact");
    auto& info=prepared.info;
    info.playback_volume=nba97_frontend_speech_volume(speech_setting);
    applyPsxGain(prepared.pcm,info.program_volume,info.tone_volume,info.playback_volume);
    if(info.pitch_cents) prepared.pcm=applyPitchCents(prepared.pcm,info.pitch_cents);
    info.rendered_sample_count=static_cast<std::uint32_t>(prepared.pcm.size());
}

RecoveredClipInfo RecoveredAudioPlayer::startCoolFact(PreparedCoolFact prepared,
                                                      std::uint8_t speech_setting) {
    applyCoolFactPlayback(prepared,speech_setting);
    // Unlike 2F124's SFX mute branch, 31770 submits speech even at gain0.
    playPcm(std::move(prepared.pcm),prepared.info.sample_rate);
    info_=std::move(prepared.info);
    return info_;
}

RecoveredClipInfo RecoveredAudioPlayer::inspectCoolFact(
        const std::filesystem::path& index_path,
        const std::filesystem::path& archive_path,
        std::uint16_t player_id,
        std::uint32_t preferred_variant) {
    info_=prepareCoolFact(index_path,archive_path,player_id,preferred_variant).info;
    return info_;
}

RecoveredClipInfo RecoveredAudioPlayer::exportCoolFact(
        const std::filesystem::path& index_path, const std::filesystem::path& archive_path,
        std::uint16_t player_id, std::uint32_t variant, const std::filesystem::path& output) {
    auto prepared=prepareCoolFact(index_path,archive_path,player_id,variant);
    writePcmWav(output,prepared.pcm,prepared.info.sample_rate);
    info_=std::move(prepared.info);
    return info_;
}

RecoveredClipInfo RecoveredAudioPlayer::exportCoolFactPlayback(
        const std::filesystem::path& index_path, const std::filesystem::path& archive_path,
        std::uint16_t player_id, std::uint32_t variant, const std::filesystem::path& output,
        std::uint8_t speech_setting) {
    auto prepared=prepareCoolFact(index_path,archive_path,player_id,variant);
    applyCoolFactPlayback(prepared,speech_setting);
    writePcmWav(output,prepared.pcm,prepared.info.sample_rate);
    info_=std::move(prepared.info);
    return info_;
}

PreparedCoolFact RecoveredAudioPlayer::prepareCoolFact(
        const std::filesystem::path& index_path,
        const std::filesystem::path& archive_path,
        std::uint16_t player_id,
        std::uint32_t preferred_variant) const {
    const auto index = readFile(index_path);
    const CoolFactIndexView records(index);
    // Selection belongs to the recovered View context, never a global counter
    // inside the decoder. Inspection/export cannot consume gameplay variants.
    const std::uint32_t variant = preferred_variant;
    const auto entry=records.lookup(player_id,variant);
    const auto record=entry.physical_record, bytes=entry.bytes, offset=entry.offset;
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
    if (clip.size()<0x76 || !tag(clip, 0, "PATl") || clip[7]!=1 || u32(clip,12)!=4 ||
        !tag(clip, 0x38, "TMxl") ||
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
    RecoveredClipInfo info{record, sample_rate, sample_count, static_cast<std::uint32_t>(compressed),
             archive_path.filename().string() + " player=" + std::to_string(player_id) +
             " variant=" + std::to_string(variant) + " logical=" + std::to_string(entry.logical_record) +
             " offset=" + std::to_string(offset)};
    info.program_volume=clip[11];
    info.tone_volume=clip[16+18];
    info.root_note=clip[16+9];
    info.pitch_cents=s16(clip,8)+s16(clip,16+20)-100*(static_cast<int>(info.root_note)-60);
    if(info.program_volume>127 || info.tone_volume>127)
        throw std::runtime_error("Cool Fact gain exceeds 7-bit range");
    info.rendered_sample_count=sample_count;
    return {std::move(info),std::move(pcm)};
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

#include "music_playback.hpp"
#include "sha256.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nba97 {
namespace {
constexpr const char* names[] = {"ZTMENU1.CNK", "ZTMENU2.CNK", "ZTMENU3.CNK",
    "ZTMENU4.CNK", "ZTPAUSE.CNK"};
constexpr const char* hashes[] = {
    "29bdee20fc33093b077b42d9bbe59d23558747b5f5b63ac0c574f8703144f4ff",
    "1bd8ed36abce8ea492eb606f3ff43bfbc7b9e13598fc460138a1ea1e364fad24",
    "96c5e4452cccaf59242b117edc96b2fb1c9b0b4e2be13e65c661494f81e63ab8",
    "14baee8db5592b541119f9aba898436b0fdde02f900b3041fb511c9c9cf651e7",
    "bd41a6dafc5f1bd84c7fb5c596130a5109cfbcdadb20d5357e50d98bc7c3bd5c"};
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("missing private music resource: " + path.string());
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
    if (input.bad()) throw std::runtime_error("failed reading music resource: " + path.string());
    return data;
}
void verify(const std::vector<std::uint8_t>& bytes, const char* expected) {
    Sha256 hash; hash.update(bytes.data(), bytes.size());
    constexpr char hex[] = "0123456789abcdef";
    std::string actual;
    for (auto b : hash.digest()) { actual += hex[b >> 4]; actual += hex[b & 15]; }
    if (actual != expected) throw std::runtime_error("music resource SHA256 differs from audited original");
}
unsigned resourceIndex(const std::string& name) {
    for (unsigned i = 0; i < 5; ++i) if (name == names[i]) return i;
    throw std::runtime_error("unrecognized original music resource: " + name);
}
std::uint32_t word(const std::vector<std::uint8_t>& b, std::size_t p) {
    if (p + 4 > b.size()) throw std::runtime_error("truncated music chunk word");
    return b[p] | (std::uint32_t(b[p+1]) << 8) |
        (std::uint32_t(b[p+2]) << 16) | (std::uint32_t(b[p+3]) << 24);
}
struct PlanProvider {
    const std::vector<std::uint8_t>& bytes;
    std::vector<Nba97MusicStreamBlock> blocks;
    std::size_t next = 0;
    std::uint64_t copied = 0;
    static void fetch(void* context, Nba97MusicStreamBlock* block) {
        auto& p = *static_cast<PlanProvider*>(context);
        *block = p.next < p.blocks.size() ? p.blocks[p.next++] : Nba97MusicStreamBlock{};
    }
    static void copy(void* context, std::uint32_t source, std::uint32_t dest, std::uint32_t size) {
        auto& p = *static_cast<PlanProvider*>(context);
        if (!source || std::uint64_t(source - 1) + size > p.bytes.size() ||
            std::uint64_t(dest) + size > 2048)
            throw std::runtime_error("recovered music staging span outside validated source");
        p.copied += size;
    }
};
Nba97MusicStreamDrain makeDrain(std::uint32_t blocks, std::uint16_t slots) {
    Nba97MusicStreamDrain d{};
    d.slots = slots; d.channels = 2; d.staging_size = 1024;
    d.tracked_voice = 0; d.paired_voice = 1;
    d.write_total = (std::min)(blocks, std::uint32_t(slots / 2 - 1));
    d.write_index = static_cast<std::uint16_t>(d.write_total % (slots / 2));
    d.producer_ended = static_cast<std::uint8_t>(d.write_total == blocks);
    return d;
}
void refill(Nba97MusicStreamDrain& d, std::uint32_t blocks) {
    if (d.producer_ended) return;
    if (d.write_total < blocks) {
        ++d.write_total;
        d.write_index = static_cast<std::uint16_t>(d.write_total % (d.slots / 2));
    } else d.producer_ended = 1;
}
std::uint64_t outputLimit(std::uint32_t blocks, std::uint16_t slots) {
    auto d = makeDrain(blocks, slots);
    for (std::uint32_t ordinal = 0; ordinal < blocks; ++ordinal) {
        if (nba97_music_stream_irq_stop(&d) == 1) return ordinal * music_staging_frames;
        std::uint32_t address = 0;
        if (nba97_music_stream_irq_advance(&d, &address) != 1)
            throw std::runtime_error("recovered music ring advance failed");
        refill(d, blocks);
    }
    // Original zero-wrap bug: no source key-off. Native media exhaustion
    // remains separate from FINISHED; do not silently repair the comparison.
    return blocks * music_staging_frames;
}
} // namespace

std::shared_ptr<const MusicTrack> loadMusicTrack(const std::filesystem::path& path) {
    const auto index = resourceIndex(path.filename().string());
    const auto bytes = read(path);
    verify(bytes, hashes[index]); // permits the proven fixed tone/envelope projection below
    auto track = std::make_shared<MusicTrack>();
    track->filename = names[index];
    track->decoded = decodeEaSchl(bytes);
    PlanProvider provider{bytes, {}, 0, 0};
    provider.blocks.push_back({1, word(bytes, 4), word(bytes, 0), 0});
    bool ended = false;
    for (std::size_t offset = word(bytes, 4); offset < bytes.size();) {
        const auto tag = word(bytes, offset), size = word(bytes, offset + 4);
        if (size < 8 || size > bytes.size() - offset)
            throw std::runtime_error("invalid music chunk extent");
        if (tag == 0x6c444353) { // SCDl
            if (size < 16 || word(bytes, offset + 12) != size - 16 || (size - 16) % 32)
                throw std::runtime_error("invalid music channel payload");
            provider.blocks.push_back({static_cast<std::uint32_t>(offset + 1), size - 16, tag, 0});
        } else if (tag == 0x6c454353) { // SCEl, ordinary caller keep_open == 0
            std::uint32_t tail = 0;
            nba97_music_stream_end(0, &tail);
            ended = tail == UINT32_MAX && offset + size == bytes.size();
        } else if (tag != 0x6c434353) throw std::runtime_error("unsupported music continuation chunk");
        offset += size;
    }
    if (!ended) throw std::runtime_error("music stream has no finite SCEl boundary");
    Nba97MusicStream stream{};
    stream.channels = 2; stream.staging_size = 1024;
    stream.underrun_index = stream.resume_index = stream.format_index = UINT16_MAX;
    while (!stream.producer_ended) {
        const int result = nba97_music_stream_fill(&stream, PlanProvider::fetch, PlanProvider::copy, &provider);
        if (result < 0) throw std::runtime_error("recovered music staging failed");
        if (result == 1) ++track->full_blocks;
    }
    track->partial_adpcm_bytes = static_cast<std::uint16_t>(1024 - stream.staging_remaining);
    if (!track->full_blocks || track->full_blocks * music_staging_frames > track->decoded.samples.size() / 2)
        throw std::runtime_error("music staging prefix exceeds decoded PCM");
    return track;
}

MusicBank loadMusicBank(const std::filesystem::path& directory) {
    const auto slots = read(directory / "music_slots.bin");
    verify(slots, "7e9457b330b38dbc81cc1bcfefb0141f866af79ec1eda9d397855e70bf2daf40");
    if (slots.size() != 208) throw std::runtime_error("music slot pack must contain sixteen13-byte records");
    MusicBank bank;
    for (unsigned i = 0; i < 16; ++i) {
        const auto* first = reinterpret_cast<const char*>(slots.data() + i * 13);
        const auto* end = static_cast<const char*>(std::memchr(first, 0, 13));
        if (!end) throw std::runtime_error("unterminated music resource slot");
        bank.slots[i] = static_cast<std::uint8_t>(resourceIndex(std::string(first, end)));
    }
    for (unsigned i = 0; i < 5; ++i) bank.tracks[i] = loadMusicTrack(directory / names[i]);
    return bank;
}

MusicPlayback::MusicPlayback(MusicBank bank, MusicOutput& output, std::uint8_t volume,
    std::uint32_t source_clock) : bank_(std::move(bank)), output_(output),
    now_(source_clock), previous_clock_(source_clock) {
    for (const auto& track : bank_.tracks) if (!track || !track->full_blocks)
        throw std::invalid_argument("music bank has an empty resource");
    resources_.initial = 1; resources_.pause = 5;
    for (unsigned i = 0; i < 16; ++i) {
        if (bank_.slots[i] >= 5) throw std::invalid_argument("music bank slot outside resource table");
        resources_.slots[i] = bank_.slots[i] + 1;
    }
    clock_.rate = 120; clock_.master_gain = 127;
    completion_.finished = 1; completion_.tracked_voice = 255;
    nba97_music_routing_init(&routing_, &resources_, volume, 0, routeCall, this);
}

std::uint32_t MusicPlayback::update(std::uint32_t source_clock, std::uint16_t& rng,
    const Nba97MusicInputs& inputs) {
    now_ = source_clock;
    // Caller supplies the actual source-clock domain. No hidden wall clock,
    // RNG, rate cap, or elapsed-time rewrite of the source signed deadlines.
    for (; previous_clock_ != now_; ++previous_clock_)
        if (nba97_music_voice_timer(&clock_, voices_.data(), voiceCall, this) != 1)
            throw std::runtime_error("recovered music voice timer fault");
    const bool draws = routing_.voice != UINT32_MAX && !inputs.guard_a && !inputs.guard_b &&
        !routing_.inhibited && routing_.phase == 1 && !inputs.selection_blocked &&
        !routing_.override && !inputs.pause;
    nba97_music_routing_step(&routing_, &inputs, &resources_, &rng, routeCall, this);
    return draws ? 1u : 0u; // READY is synchronously true in this resident native bank
}
void MusicPlayback::setRecoveredVolume(std::uint8_t gain) {
    nba97_music_voice_gain(&voices_[0], (std::min)(unsigned(gain), 127u));
    applyGain();
}
void MusicPlayback::overrideResource(unsigned index) {
    if (index >= bank_.tracks.size()) throw std::out_of_range("music override outside resource table");
    routing_.override = index + 1;
}
void MusicPlayback::applyGain() {
    nba97_music_voice_effective(&voices_[0], clock_.master_gain, voiceCall, this);
    output_.gain(voices_[0].effective_gain <= 127 ? voices_[0].effective_gain : 0);
}
void MusicPlayback::startStream() {
    if (!selected_) throw std::runtime_error("music stream started before resource load");
    // The five hash-verified tone headers produce these values through924B4/
    //9267C. FFFFFFFF envelope ticks are still decremented by the source service.
    voices_ = {};
    auto& voice = voices_[0];
    voice.handle = 1; voice.active = 1;
    voice.authored_gain = voice.effective_gain = 127;
    voice.ramp_current = voice.envelope_current = 127u << 16;
    voice.envelope_count = 1; voice.envelope_ticks = UINT32_MAX;
    completion_ = {}; completion_.tracked_voice = 0;
    drain_ = makeDrain(selected_->full_blocks, slots_);
    frame_limit_ = outputLimit(selected_->full_blocks, slots_);
    ++output_generation_; next_slot_ = 1;
    output_pending_ = true; output_started_ = false; keyoff_ = false;
    //6A8F8 clears both bytes, then marks available SCDl data with flags4.
    // The resident, validated bank supplies that ordinary successful path.
    stream_flags_ = 4; stream_pending_ = 0;
}
void MusicPlayback::beginOutput() {
    if (!output_pending_) return;
    output_.begin(selected_, output_generation_, frame_limit_, voices_[0].effective_gain);
    output_pending_ = false; output_started_ = true;
    slotEntry(); // initial SPU IRQ address is slot0+8; native substitutes its entry
}
void MusicPlayback::slotEntry() {
    if (keyoff_) return;
    if (nba97_music_stream_irq_stop(&drain_) == 1) {
        keyoff_ = true;
        completion_.channel_state = 1;
        output_.keyOff(output_generation_);
        return;
    }
    std::uint32_t address = 0;
    if (nba97_music_stream_irq_advance(&drain_, &address) != 1)
        throw std::runtime_error("recovered music IRQ advance failed");
    refill(drain_, selected_->full_blocks);
}
void MusicPlayback::hardwareService() {
    if (!output_started_) return;
    const auto progress = output_.progress();
    // A retired/reset generation cannot finish a later voice.
    if (progress.generation != output_generation_) return;
    while (!keyoff_ && next_slot_ <= progress.completed_frames / music_staging_frames) {
        slotEntry(); ++next_slot_;
    }
    if (keyoff_) {
        // Explicit platform substitution: WinMM drained => native level0.
        // This is not a measurement of original SPU ADSR release or prefetch.
        const int status = nba97_music_hardware_status(0, progress.drained ? 0 : 1);
        nba97_music_voice_complete(&completion_, &voices_[0], 0, status);
    }
}

std::uint32_t MusicPlayback::routeCall(void* context, Nba97MusicCall call,
    std::uint32_t a0, std::uint32_t a1, std::uint32_t, std::uint32_t, std::uint32_t) {
    auto& s = *static_cast<MusicPlayback*>(context);
    switch (call) {
    case NBA97_MUSIC_READY: return 1;
    case NBA97_MUSIC_ALLOCATE: return 1;
    case NBA97_MUSIC_LOAD:
        if (!a1 || a1 > 5) throw std::runtime_error("music routing resource token is invalid");
        s.selected_ = s.bank_.tracks[a1 - 1]; return 0;
    case NBA97_MUSIC_CLOCK: return s.now_;
    case NBA97_MUSIC_CONFIGURE: s.slots_ = static_cast<std::uint16_t>(a0); return 0;
    case NBA97_MUSIC_START_STREAM: s.startStream(); return 0;
    case NBA97_MUSIC_VOICE:
        //6ACAC: reservation bit2, enabled audio bit1, valid voice handle bit4.
        // Completion702B0 does not clear these stream flags.
        s.stream_flags_ |= 7; return 1;
    case NBA97_MUSIC_GAIN: s.setRecoveredVolume(static_cast<std::uint8_t>(a1)); return 0;
    case NBA97_MUSIC_REFILL: s.beginOutput(); return 0;
    case NBA97_MUSIC_FINISHED: return s.completion_.finished;
    case NBA97_MUSIC_FADE: nba97_music_voice_fade(&s.voices_[0], a1, UINT32_MAX); return 0;
    case NBA97_MUSIC_BUSY:
        // Source status is never0; it must not become an isPlaying boolean.
        return static_cast<std::uint32_t>(s.rawStreamStatus());
    case NBA97_MUSIC_DETACH:
        //28C28 ->6B784 clears C6CAC after releasing the CD queue.
        s.stream_flags_ = 0; return 0;
    case NBA97_MUSIC_RETIRE:
    case NBA97_MUSIC_FREE:
        s.output_.retire(); s.output_started_ = s.output_pending_ = false; return 0;
    default: return 0; // synchronous resident-bank CD/pump/notification boundaries
    }
}
std::uint32_t MusicPlayback::voiceCall(void* context, Nba97MusicVoiceCall call,
    std::uint32_t, std::uint32_t a1, std::uint32_t) {
    auto& s = *static_cast<MusicPlayback*>(context);
    switch (call) {
    case NBA97_VOICE_HARDWARE_SERVICE: s.hardwareService(); break;
    case NBA97_VOICE_STOP: s.drain_.stop_requested = 1; break;
    case NBA97_VOICE_APPLY: s.output_.gain(a1 <= 127 ? a1 : 0); break;
    case NBA97_VOICE_ENVELOPE_WORD:
    case NBA97_VOICE_GAIN_MAP:
        throw std::runtime_error("unproved music envelope/map reached by native adapter");
    default: break;
    }
    return 0;
}
} // namespace nba97

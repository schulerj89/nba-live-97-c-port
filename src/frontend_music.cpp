#include "frontend_music.hpp"
#include "music_playback.hpp"
#include "music_buffer_retirement.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace nba97 {
namespace {
constexpr std::size_t buffer_frames = 256, buffer_count = 4;
void check(MMRESULT result, const char* operation) {
    if (result != MMSYSERR_NOERROR)
        throw std::runtime_error(std::string(operation) + ": " + std::to_string(result));
}
// Every address passed to WinMM, including its event, lives in this heap owner.
// Destroy only after reset, every unprepare, and close have succeeded.
struct DeviceStorage final : MusicRetirementOps {
    HWAVEOUT device = nullptr;
    HANDLE done_event = nullptr;
    MusicBufferRetirement lease;
    std::array<WAVEHDR, buffer_count> headers{};
    std::array<std::array<std::int16_t, buffer_frames * 2>, buffer_count> buffers{};
    std::array<bool, buffer_count> queued{};
    ~DeviceStorage() { if (done_event) CloseHandle(done_event); }
    bool reset() noexcept override { return waveOutReset(device) == MMSYSERR_NOERROR; }
    bool unprepare(std::size_t i) noexcept override {
        return waveOutUnprepareHeader(device, &headers[i], sizeof(WAVEHDR)) == MMSYSERR_NOERROR;
    }
    bool close() noexcept override { return waveOutClose(device) == MMSYSERR_NOERROR; }
};
void retainUnsafeStorage(std::unique_ptr<DeviceStorage> storage) noexcept {
    // Exceptional driver failure: retain storage for process lifetime, instead
    // of freeing live headers/event. No static teardown may destroy this owner.
    // This deliberate leak is reported; normal retirement never enters here.
    auto* retained = storage.release();
    (void)retained;
}
struct WaveStream {
    std::shared_ptr<const MusicTrack> track;
    const std::uint64_t generation, frame_limit;
    std::atomic<unsigned> gain, starvation{0};
    std::atomic<std::uint64_t> completed{0};
    std::atomic<bool> playing{false}, drained{false};
    HANDLE stop_event = nullptr, ready_event = nullptr;
    std::thread worker;
    mutable std::mutex mutex;
    std::string failure;
    WaveStream(std::shared_ptr<const MusicTrack> t, std::uint64_t gen,
        std::uint64_t limit, unsigned volume) : track(std::move(t)),
        generation(gen), frame_limit(limit), gain(volume) {
        if (frame_limit > track->decoded.samples.size() / 2)
            throw std::runtime_error("music output prefix exceeds PCM storage");
        stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stop_event || !ready_event) { closeEvents(); throw std::runtime_error("music event creation failed"); }
        try { worker = std::thread([this] { run(); }); }
        catch (...) { closeEvents(); throw; }
    }
    ~WaveStream() {
        SetEvent(stop_event);
        if (worker.joinable()) worker.join();
        closeEvents();
    }
    void closeEvents() noexcept {
        if (stop_event) CloseHandle(stop_event);
        if (ready_event) CloseHandle(ready_event);
    }
    std::string error() const { std::lock_guard<std::mutex> lock(mutex); return failure; }
    void fail(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!failure.empty()) failure += "; ";
        failure += message;
    }
    void run() noexcept {
        std::unique_ptr<DeviceStorage> storage;
        try {
            storage = std::make_unique<DeviceStorage>();
            auto& s = *storage;
            s.done_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!s.done_event) throw std::runtime_error("music driver event creation failed");
            WAVEFORMATEX format{};
            format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = 2;
            format.nSamplesPerSec = track->decoded.info.sample_rate;
            format.wBitsPerSample = 16; format.nBlockAlign = 4;
            format.nAvgBytesPerSec = format.nSamplesPerSec * 4;
            check(waveOutOpen(&s.device, WAVE_MAPPER, &format,
                reinterpret_cast<DWORD_PTR>(s.done_event), 0, CALLBACK_EVENT), "music output open");
            s.lease.opened = true;
            std::uint64_t position = 0;
            std::size_t next = 0, queued = 0;
            auto submit = [&](std::size_t i) {
                if (position >= frame_limit) return;
                auto& h = s.headers[i];
                const auto frames = static_cast<std::size_t>((std::min)(frame_limit - position,
                    std::uint64_t(buffer_frames)));
                const auto volume = (std::min)(gain.load(), 127u);
                for (std::size_t n = 0; n < frames * 2; ++n)
                    s.buffers[i][n] = static_cast<std::int16_t>(
                        int(track->decoded.samples[static_cast<std::size_t>(position) * 2 + n]) *
                        static_cast<int>(volume) / 127);
                h.dwBufferLength = static_cast<DWORD>(frames * 4);
                check(waveOutWrite(s.device, &h, sizeof(h)), "music output submit");
                s.queued[i] = true; ++queued; position += frames;
            };
            for (std::size_t i = 0; i < buffer_count; ++i) {
                auto& h = s.headers[i];
                h.lpData = reinterpret_cast<LPSTR>(s.buffers[i].data());
                h.dwBufferLength = static_cast<DWORD>(sizeof(s.buffers[i]));
                check(waveOutPrepareHeader(s.device, &h, sizeof(h)), "music output prepare");
                s.lease.prepared[i] = true;
                submit(i);
            }
            playing = queued != 0; SetEvent(ready_event);
            const HANDLE events[]{stop_event, s.done_event};
            while (queued) {
                const auto wait = WaitForMultipleObjects(2, events, FALSE, 100);
                if (wait == WAIT_OBJECT_0) break;
                if (wait == WAIT_FAILED) throw std::runtime_error("music output wait failed");
                // Reset before scanning: completion during scan remains signalled.
                // Process/resubmit in FIFO order, including wrapped queues/stalls.
                ResetEvent(s.done_event);
                bool all_returned = true;
                for (std::size_t i = 0; i < buffer_count; ++i)
                    if (s.queued[i] && !(s.headers[i].dwFlags & WHDR_DONE)) all_returned = false;
                if (all_returned && position < frame_limit) ++starvation;
                for (std::size_t n = 0; n < buffer_count && s.queued[next] &&
                    (s.headers[next].dwFlags & WHDR_DONE); ++n) {
                    completed += s.headers[next].dwBufferLength / 4;
                    s.queued[next] = false; --queued;
                    submit(next); next = (next + 1) % buffer_count;
                }
            }
            // Reset returns are never counted as rendered frames: completed was
            // updated only in the normal FIFO loop before any reset call.
        } catch (const std::exception& e) { fail(e.what()); }
        catch (...) { fail("unknown music output failure"); }
        playing = false;
        if (storage) {
            if (storage->lease.release(*storage)) drained = true;
            else {
                fail("WinMM cleanup failed; outstanding device/header/event storage retained for process lifetime");
                retainUnsafeStorage(std::move(storage));
            }
        }
        SetEvent(ready_event);
    }
};
} // namespace

struct FrontendMusicPlayer::Output final : MusicOutput {
    std::unique_ptr<WaveStream> stream;
    unsigned retired_underruns = 0;
    std::string failure;
    ~Output() override { retire(); }
    void begin(std::shared_ptr<const MusicTrack> track, std::uint64_t generation,
        std::uint64_t frame_limit, unsigned volume) override {
        retire();
        if (!failure.empty()) throw std::runtime_error(failure);
        auto next = std::make_unique<WaveStream>(std::move(track), generation, frame_limit, volume);
        if (WaitForSingleObject(next->ready_event, 6000) != WAIT_OBJECT_0)
            throw std::runtime_error("music output startup timeout");
        if (!next->error().empty()) throw std::runtime_error(next->error());
        stream = std::move(next);
    }
    void gain(unsigned volume) override { if (stream) stream->gain = (std::min)(volume, 127u); }
    void keyOff(std::uint64_t generation) override {
        if (stream && stream->generation == generation) SetEvent(stream->stop_event);
    }
    MusicOutputProgress progress() const override {
        if (!stream) return {};
        return {stream->generation, stream->completed.load(), stream->drained.load()};
    }
    void retire() override {
        if (!stream) return;
        SetEvent(stream->stop_event);
        if (stream->worker.joinable()) stream->worker.join();
        retired_underruns += stream->starvation;
        const auto error = stream->error();
        if (!error.empty()) failure = error;
        stream.reset();
    }
    std::string error() const {
        if (!failure.empty()) return failure;
        return stream ? stream->error() : std::string{};
    }
};
struct FrontendMusicPlayer::Runtime {
    MusicPlayback playback;
    Runtime(MusicBank bank, MusicOutput& output, std::uint8_t volume, std::uint32_t clock)
        : playback(std::move(bank), output, volume, clock) {}
};
FrontendMusicPlayer::FrontendMusicPlayer() = default;
FrontendMusicPlayer::~FrontendMusicPlayer() { stop(); }
void FrontendMusicPlayer::start(const std::filesystem::path& path, std::uint8_t volume) {
    stop(); failure_.clear(); info_ = {};
    const auto track = loadMusicTrack(path);
    info_ = track->decoded.info;
    output_ = std::make_unique<Output>();
    // Legacy decoder smoke: finite eligible prefix, no selector/private RNG.
    // Diagnostic drain is not source FINISHED.
    output_->begin(track, 1, track->full_blocks * music_staging_frames, volume);
    decoder_name_ = "native EA SCHl/TMxl + PS ADPCM; finite diagnostic PCM, four256-frame WinMM buffers";
}
void FrontendMusicPlayer::startFrontend(const std::filesystem::path& directory,
    std::uint8_t volume, std::uint32_t clock) {
    stop(); failure_.clear(); info_ = {};
    auto bank = loadMusicBank(directory);
    info_ = bank.tracks[0]->decoded.info;
    output_ = std::make_unique<Output>();
    runtime_ = std::make_unique<Runtime>(std::move(bank), *output_, volume, clock);
    decoder_name_ = "native five-resource source routing/voice/staging; WinMM slot-entry/drain substitution, four256-frame buffers";
}
std::uint32_t FrontendMusicPlayer::updateFrontend(std::uint32_t clock, std::uint16_t& rng,
    const Nba97MusicInputs& inputs) {
    if (!runtime_ || !failure_.empty()) return 0;
    try {
        const auto draws = runtime_->playback.update(clock, rng, inputs);
        if (const auto* track = runtime_->playback.track()) info_ = track->decoded.info;
        return draws;
    } catch (const std::exception& e) {
        failure_ = e.what();
        if (output_) output_->retire();
        return 0;
    }
}
void FrontendMusicPlayer::stop() noexcept {
    runtime_.reset();
    if (output_) {
        output_->retire();
        try { if (!output_->error().empty()) failure_ = output_->error(); } catch (...) {}
    }
    output_.reset(); decoder_name_.clear();
}
void FrontendMusicPlayer::setRecoveredVolume(std::uint8_t volume) noexcept {
    if (runtime_) runtime_->playback.setRecoveredVolume(volume);
    else if (output_) output_->gain(volume);
}
void FrontendMusicPlayer::requestSourceStop() noexcept {
    if (runtime_) runtime_->playback.requestSourceStop();
}
void FrontendMusicPlayer::overrideResource(unsigned index) {
    if (runtime_) runtime_->playback.overrideResource(index);
}
bool FrontendMusicPlayer::isPlaying() const noexcept {
    return output_ && output_->stream && output_->stream->playing;
}
std::string FrontendMusicPlayer::error() const {
    return !failure_.empty() ? failure_ : output_ ? output_->error() : std::string{};
}
unsigned FrontendMusicPlayer::underruns() const noexcept {
    return output_ ? output_->retired_underruns + (output_->stream ? output_->stream->starvation.load() : 0) : 0;
}
std::string FrontendMusicPlayer::currentResource() const {
    if (runtime_ && runtime_->playback.track()) return runtime_->playback.track()->filename;
    return output_ && output_->stream ? output_->stream->track->filename : std::string{};
}
std::uint32_t FrontendMusicPlayer::routingPhase() const noexcept {
    return runtime_ ? runtime_->playback.routing().phase : UINT32_MAX;
}
std::uint64_t FrontendMusicPlayer::outputGeneration() const noexcept {
    return runtime_ ? runtime_->playback.outputGeneration() : 0;
}
std::uint64_t FrontendMusicPlayer::sourceFrameLimit() const noexcept {
    return runtime_ ? runtime_->playback.frameLimit() : 0;
}
} // namespace nba97

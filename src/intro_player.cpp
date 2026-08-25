#include "intro_player.hpp"

#include <dshow.h>
#include <d3d9.h>
#include <mmsystem.h>
#include <vmr9.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace nba97 {
namespace {

template <typename Interface>
class ComPtr final {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    [[nodiscard]] Interface* get() const noexcept { return pointer_; }
    [[nodiscard]] Interface** put() noexcept {
        reset();
        return &pointer_;
    }
    Interface* operator->() const noexcept { return pointer_; }
    explicit operator bool() const noexcept { return pointer_ != nullptr; }

    void reset() noexcept {
        if (pointer_) pointer_->Release();
        pointer_ = nullptr;
    }

private:
    Interface* pointer_ = nullptr;
};

std::runtime_error comError(const char* operation, HRESULT result) {
    std::ostringstream message;
    message << operation << " failed (HRESULT 0x" << std::hex << std::uppercase
            << static_cast<unsigned long>(result) << ')';
    return std::runtime_error(message.str());
}

void require(HRESULT result, const char* operation) {
    if (FAILED(result)) throw comError(operation, result);
}

std::uint32_t littleU32(const std::vector<std::uint8_t>& data,
                        std::size_t at) {
    if (at + 4 > data.size()) throw std::runtime_error("truncated AVI chunk");
    return static_cast<std::uint32_t>(data[at]) |
           (static_cast<std::uint32_t>(data[at + 1]) << 8) |
           (static_cast<std::uint32_t>(data[at + 2]) << 16) |
           (static_cast<std::uint32_t>(data[at + 3]) << 24);
}

bool fourcc(const std::vector<std::uint8_t>& data, std::size_t at,
            const char* value) noexcept {
    return at + 4 <= data.size() && std::memcmp(data.data() + at, value, 4) == 0;
}

void collectAviPcmChunks(const std::vector<std::uint8_t>& avi,
                         std::size_t begin, std::size_t end, bool in_movi,
                         std::vector<std::uint8_t>& pcm) {
    end = (std::min)(end, avi.size());
    for (std::size_t at = begin; at + 8 <= end;) {
        const std::uint32_t size = littleU32(avi, at + 4);
        const std::size_t payload = at + 8;
        if (size > end - payload)
            throw std::runtime_error("AVI chunk extends beyond its container");
        if ((fourcc(avi, at, "LIST") || fourcc(avi, at, "RIFF")) && size >= 4) {
            const bool child_is_movi = in_movi || fourcc(avi, payload, "movi") ||
                                       fourcc(avi, payload, "rec ");
            collectAviPcmChunks(avi, payload + 4, payload + size,
                                child_is_movi, pcm);
        } else if (in_movi && avi[at + 2] == 'w' && avi[at + 3] == 'b') {
            pcm.insert(pcm.end(), avi.begin() + static_cast<std::ptrdiff_t>(payload),
                       avi.begin() + static_cast<std::ptrdiff_t>(payload + size));
        }
        const std::size_t padded = static_cast<std::size_t>(size) + (size & 1u);
        if (padded > end - payload) break;
        at = payload + padded;
    }
}

std::vector<std::uint8_t> loadAviPcm(const std::filesystem::path& movie) {
    std::ifstream input(movie, std::ios::binary);
    if (!input) throw std::runtime_error("unable to open intro AVI PCM source");
    std::vector<std::uint8_t> avi((std::istreambuf_iterator<char>(input)), {});
    if (avi.size() < 12 || !fourcc(avi, 0, "RIFF") || !fourcc(avi, 8, "AVI "))
        throw std::runtime_error("invalid intro AVI RIFF header");
    std::vector<std::uint8_t> pcm;
    collectAviPcmChunks(avi, 12, avi.size(), false, pcm);
    if (pcm.empty() || pcm.size() % 4 != 0)
        throw std::runtime_error("intro AVI has no valid stereo PCM chunks");
    return pcm;
}

std::string narrow(const wchar_t* text) {
    if (!text || !*text) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 1) return {};
    std::string output(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, output.data(), size,
                        nullptr, nullptr);
    output.pop_back();
    return output;
}

} // namespace

class IntroPlayer::Implementation final {
public:
    void start(HWND host_window, const std::filesystem::path& movie) {
        stop();
        window = host_window;

        require(CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_IGraphBuilder,
                                 reinterpret_cast<void**>(graph.put())),
                "CoCreateInstance(FilterGraph)");

        ComPtr<IBaseFilter> renderer;
        require(CoCreateInstance(CLSID_VideoMixingRenderer9, nullptr,
                                 CLSCTX_INPROC_SERVER, IID_IBaseFilter,
                                 reinterpret_cast<void**>(renderer.put())),
                "CoCreateInstance(VMR9)");
        require(graph->AddFilter(renderer.get(), L"NBA97 in-window VMR9"),
                "IGraphBuilder::AddFilter(VMR9)");

        ComPtr<IVMRFilterConfig9> configuration;
        require(renderer->QueryInterface(IID_IVMRFilterConfig9,
                                         reinterpret_cast<void**>(configuration.put())),
                "QueryInterface(IVMRFilterConfig9)");
        require(configuration->SetRenderingMode(VMR9Mode_Windowless),
                "IVMRFilterConfig9::SetRenderingMode");
        require(renderer->QueryInterface(IID_IVMRWindowlessControl9,
                                         reinterpret_cast<void**>(video.put())),
                "QueryInterface(IVMRWindowlessControl9)");
        require(video->SetVideoClippingWindow(window),
                "IVMRWindowlessControl9::SetVideoClippingWindow");
        require(video->SetAspectRatioMode(VMR9ARMode_LetterBox),
                "IVMRWindowlessControl9::SetAspectRatioMode");
        require(video->SetBorderColor(RGB(0, 0, 0)),
                "IVMRWindowlessControl9::SetBorderColor");

        require(graph->RenderFile(std::filesystem::absolute(movie).c_str(), nullptr),
                "IGraphBuilder::RenderFile(Z0ZTITLE.avi)");
        require(graph->QueryInterface(IID_IBasicAudio,
                                      reinterpret_cast<void**>(audio.put())),
                "QueryInterface(IBasicAudio)");
        // DirectShow builds a valid DirectSound leg on this machine but can
        // still produce silence. Mute that duplicate leg and send the AVI's
        // original PCM chunks through the same proven WinMM path as frontend
        // music, starting both clocks together below.
        require(audio->put_Volume(-10000), "IBasicAudio::put_Volume");
        require(audio->put_Balance(0), "IBasicAudio::put_Balance");
        require(graph->QueryInterface(IID_IMediaControl,
                                      reinterpret_cast<void**>(control.put())),
                "QueryInterface(IMediaControl)");
        require(graph->QueryInterface(IID_IMediaEventEx,
                                      reinterpret_cast<void**>(events.put())),
                "QueryInterface(IMediaEventEx)");
        require(events->SetNotifyWindow(reinterpret_cast<OAHWND>(window),
                                        IntroPlayer::kGraphEventMessage, 0),
                "IMediaEventEx::SetNotifyWindow");
        collectFilterNames();
        startNativeAudio(movie);
        resize();
        require(control->Run(), "IMediaControl::Run");
        const MMRESULT audio_result = waveOutWrite(
            wave_out, &wave_header, sizeof(wave_header));
        if (audio_result != MMSYSERR_NOERROR) {
            stop();
            throw std::runtime_error("waveOutWrite(intro PCM) failed: " +
                                     std::to_string(audio_result));
        }
        playing = true;
        last_event_code = 0;
    }

    void stop() noexcept {
        playing = false;
        if (events) events->SetNotifyWindow(0, 0, 0);
        if (control) control->Stop();
        stopNativeAudio();
        events.reset();
        control.reset();
        audio.reset();
        video.reset();
        graph.reset();
        filters.clear();
        window = nullptr;
    }

    void startNativeAudio(const std::filesystem::path& movie) {
        pcm = loadAviPcm(movie);
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 37800;
        format.wBitsPerSample = 16;
        format.nBlockAlign = 4;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        MMRESULT result = waveOutOpen(&wave_out, WAVE_MAPPER, &format,
                                      0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            wave_out = nullptr;
            throw std::runtime_error("waveOutOpen(intro PCM) failed: " +
                                     std::to_string(result));
        }
        restore_wave_volume =
            waveOutGetVolume(wave_out, &previous_wave_volume) == MMSYSERR_NOERROR;
        waveOutSetVolume(wave_out, 0xffffffffu);
        wave_header = {};
        wave_header.lpData = reinterpret_cast<LPSTR>(pcm.data());
        wave_header.dwBufferLength = static_cast<DWORD>(pcm.size());
        result = waveOutPrepareHeader(wave_out, &wave_header, sizeof(wave_header));
        if (result != MMSYSERR_NOERROR) {
            stopNativeAudio();
            throw std::runtime_error("waveOutPrepareHeader(intro PCM) failed: " +
                                     std::to_string(result));
        }
        audio_description = "native AVI PCM -> WinMM; 37800 Hz stereo samples=" +
            std::to_string(pcm.size() / 4);
    }

    void stopNativeAudio() noexcept {
        if (wave_out) {
            waveOutReset(wave_out);
            if (wave_header.dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(wave_out, &wave_header, sizeof(wave_header));
            if (restore_wave_volume)
                waveOutSetVolume(wave_out, previous_wave_volume);
            waveOutClose(wave_out);
        }
        wave_out = nullptr;
        wave_header = {};
        previous_wave_volume = 0;
        restore_wave_volume = false;
        pcm.clear();
        audio_description.clear();
    }

    void resize() noexcept {
        if (!video || !window) return;
        RECT destination{};
        GetClientRect(window, &destination);
        video->SetVideoPosition(nullptr, &destination);
        video->DisplayModeChanged();
    }

    void repaint(HDC device_context) noexcept {
        if (video && window) video->RepaintVideo(window, device_context);
    }

    IntroPlayer::EventResult handleGraphEvents() noexcept {
        if (!events) return IntroPlayer::EventResult::None;
        IntroPlayer::EventResult result = IntroPlayer::EventResult::None;
        long event_code = 0;
        LONG_PTR parameter1 = 0;
        LONG_PTR parameter2 = 0;
        while (events->GetEvent(&event_code, &parameter1, &parameter2, 0) == S_OK) {
            events->FreeEventParams(event_code, parameter1, parameter2);
            last_event_code = event_code;
            if (event_code == EC_COMPLETE) result = IntroPlayer::EventResult::Completed;
            else if (event_code == EC_USERABORT) result = IntroPlayer::EventResult::UserAbort;
            else if (event_code == EC_ERRORABORT) result = IntroPlayer::EventResult::Error;
        }
        return result;
    }

    void collectFilterNames() {
        filters.clear();
        ComPtr<IEnumFilters> enumerator;
        if (FAILED(graph->EnumFilters(enumerator.put()))) return;
        IBaseFilter* raw_filter = nullptr;
        while (enumerator->Next(1, &raw_filter, nullptr) == S_OK) {
            FILTER_INFO info{};
            if (SUCCEEDED(raw_filter->QueryFilterInfo(&info))) {
                filters.push_back(narrow(info.achName));
                if (info.pGraph) info.pGraph->Release();
            }
            raw_filter->Release();
            raw_filter = nullptr;
        }
    }

    HWND window = nullptr;
    bool playing = false;
    long last_event_code = 0;
    std::vector<std::string> filters;
    std::string audio_description;
    std::vector<std::uint8_t> pcm;
    HWAVEOUT wave_out = nullptr;
    WAVEHDR wave_header{};
    DWORD previous_wave_volume = 0;
    bool restore_wave_volume = false;
    ComPtr<IGraphBuilder> graph;
    ComPtr<IMediaControl> control;
    ComPtr<IMediaEventEx> events;
    ComPtr<IBasicAudio> audio;
    ComPtr<IVMRWindowlessControl9> video;
};

IntroPlayer::IntroPlayer() : implementation_(std::make_unique<Implementation>()) {}
IntroPlayer::~IntroPlayer() = default;
void IntroPlayer::start(HWND window, const std::filesystem::path& movie) {
    implementation_->start(window, movie);
}
void IntroPlayer::stop() noexcept { implementation_->stop(); }
void IntroPlayer::resize() noexcept { implementation_->resize(); }
void IntroPlayer::repaint(HDC dc) noexcept { implementation_->repaint(dc); }
IntroPlayer::EventResult IntroPlayer::handleGraphEvents() noexcept {
    return implementation_->handleGraphEvents();
}
bool IntroPlayer::isPlaying() const noexcept { return implementation_->playing; }
const std::vector<std::string>& IntroPlayer::filterNames() const noexcept {
    return implementation_->filters;
}
const std::string& IntroPlayer::audioDescription() const noexcept {
    return implementation_->audio_description;
}
long IntroPlayer::lastEventCode() const noexcept {
    return implementation_->last_event_code;
}

} // namespace nba97

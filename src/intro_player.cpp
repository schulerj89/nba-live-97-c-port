#include "intro_player.hpp"

#include <dshow.h>
#include <d3d9.h>
#include <vmr9.h>

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
        resize();
        require(control->Run(), "IMediaControl::Run");
        playing = true;
        last_event_code = 0;
    }

    void stop() noexcept {
        playing = false;
        if (events) events->SetNotifyWindow(0, 0, 0);
        if (control) control->Stop();
        events.reset();
        control.reset();
        video.reset();
        graph.reset();
        filters.clear();
        window = nullptr;
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
    ComPtr<IGraphBuilder> graph;
    ComPtr<IMediaControl> control;
    ComPtr<IMediaEventEx> events;
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
long IntroPlayer::lastEventCode() const noexcept {
    return implementation_->last_event_code;
}

} // namespace nba97

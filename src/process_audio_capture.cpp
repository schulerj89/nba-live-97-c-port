#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <sdkddkver.h>
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <wrl.h>
#include "process_audio_capture.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace nba97 {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT hr,const char* operation) {
    if(FAILED(hr)) {std::ostringstream s;s<<operation<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr);throw std::runtime_error(s.str());}
}
struct Event {
    HANDLE value=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    Event() {if(!value) throw std::runtime_error("audio event creation failed");}
    ~Event() {CloseHandle(value);}
};
struct Apartment {
    Apartment(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"capture COM");}
    ~Apartment(){CoUninitialize();}
};
struct CloseProcess {void operator()(void* h) const noexcept {if(h) CloseHandle(h);}};
struct ProcessIdentity {
    std::unique_ptr<void,CloseProcess> handle;
    DWORD pid;
    std::uint64_t created=0;
    bool external;
    ProcessIdentity(DWORD id,const std::filesystem::path& expected,bool selected):pid(id),external(selected) {
        if(!pid || (external && expected.empty())) throw std::runtime_error("explicit audio target requires PID and executable");
        handle.reset(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid));
        if(!handle) throw std::runtime_error("cannot open selected audio process");
        if(WaitForSingleObject(handle.get(),0)!=WAIT_TIMEOUT) throw std::runtime_error("selected audio process has exited");
        if(external) {
            std::wstring image(32768,L'\0');DWORD size=static_cast<DWORD>(image.size());
            if(!QueryFullProcessImageNameW(handle.get(),0,image.data(),&size))
                throw std::runtime_error("cannot verify selected audio executable");
            image.resize(size);
            if(!std::filesystem::equivalent(expected,std::filesystem::path(image)))
                throw std::runtime_error("selected audio PID does not match expected executable");
        }
        FILETIME creation{},exit{},kernel{},user{};
        if(!GetProcessTimes(handle.get(),&creation,&exit,&kernel,&user))
            throw std::runtime_error("cannot identify selected audio process creation time");
        created=(std::uint64_t(creation.dwHighDateTime)<<32)|creation.dwLowDateTime;
    }
    void requireAlive() const {
        if(WaitForSingleObject(handle.get(),0)!=WAIT_TIMEOUT)
            throw std::runtime_error("selected audio process exited; recording stopped without fallback");
    }
};
// Callback owns its event and activation parameters. If activation times out,
// the outstanding COM operation cannot refer to the destroyed recorder.
class Activation final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IActivateAudioInterfaceCompletionHandler,Microsoft::WRL::FtmBase> {
public:
    Event done;
    HRESULT result=E_PENDING;
    ComPtr<IAudioClient> client;
    AUDIOCLIENT_ACTIVATION_PARAMS parameters{};
    PROPVARIANT property{};
    explicit Activation(DWORD target_pid) {
        parameters.ActivationType=AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
        parameters.ProcessLoopbackParams.TargetProcessId=target_pid;
        parameters.ProcessLoopbackParams.ProcessLoopbackMode=PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
        property.vt=VT_BLOB;property.blob.cbSize=sizeof(parameters);
        property.blob.pBlobData=reinterpret_cast<BYTE*>(&parameters);
    }
    STDMETHODIMP ActivateCompleted(IActivateAudioInterfaceAsyncOperation* operation) override {
        ComPtr<IUnknown> unknown;
        HRESULT activation=E_FAIL;
        result=operation->GetActivateResult(&activation,&unknown);
        if(SUCCEEDED(result)) result=activation;
        if(SUCCEEDED(result)) result=unknown.As(&client);
        SetEvent(done.value);return S_OK;
    }
};
void u16(std::ostream& out,std::uint16_t value) {out.put(static_cast<char>(value));out.put(static_cast<char>(value>>8));}
void u32(std::ostream& out,std::uint32_t value) {u16(out,static_cast<std::uint16_t>(value));u16(out,static_cast<std::uint16_t>(value>>16));}
void header(std::ostream& out,std::uint32_t bytes) {
    out.write("RIFF",4);u32(out,36+bytes);out.write("WAVEfmt ",8);u32(out,16);
    u16(out,1);u16(out,2);u32(out,48000);u32(out,192000);u16(out,4);u16(out,16);
    out.write("data",4);u32(out,bytes);
}
}
std::uint64_t ProcessAudioCapture::qpc100ns() {
    LARGE_INTEGER ticks{},frequency{};
    if(!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&ticks) ||
       frequency.QuadPart<=0 || frequency.QuadPart>1000000000 || ticks.QuadPart<0)
        throw std::runtime_error("invalid capture QPC clock");
    const auto t=static_cast<std::uint64_t>(ticks.QuadPart), f=static_cast<std::uint64_t>(frequency.QuadPart);
    return (t/f)*10000000+(t%f)*10000000/f;
}
struct ProcessAudioCapture::Impl {
    ProcessIdentity target;
    std::filesystem::path directory;
    std::uint64_t origin;
    Event stop, ready;
    std::thread thread;
    mutable std::mutex mutex;
    std::string failure;
    std::atomic<bool> valid{false};
    std::atomic<std::uint64_t> requested_end{0};
    bool finished=false;
    Impl(const std::filesystem::path& path,std::uint64_t start,DWORD pid,
         const std::filesystem::path& expected,bool selected):target(pid,expected,selected),directory(path),origin(start) {
        if(!std::filesystem::create_directory(directory)) throw std::runtime_error("audio recording directory must be fresh");
        thread=std::thread([this]{run();});
    }
    ~Impl(){SetEvent(stop.value);if(thread.joinable()) thread.join();}
    void fail(const std::string& detail) {std::lock_guard<std::mutex> lock(mutex);if(failure.empty()) failure=detail;}
    std::string error() const {std::lock_guard<std::mutex> lock(mutex);return failure;}
    void run() noexcept {
        std::uint64_t frames=0,packets=0,nonzero=0,discontinuities=0,timestamp_errors=0,position_gaps=0,nonzero_positions=0;
        std::uint64_t last_packet_end=0;
        std::uint64_t expected_position=0;
        std::int64_t first_qpc=-1,last_qpc=-1;
        std::ofstream wave,timeline;
        try {
            Apartment apartment;
            target.requireAlive();
            auto activation=Microsoft::WRL::Make<Activation>(target.pid);
            if(!activation) throw std::bad_alloc();
            ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
            check(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,__uuidof(IAudioClient),
                &activation->property,activation.Get(),&operation),"activate process-only loopback");
            if(WaitForSingleObject(activation->done.value,5000)!=WAIT_OBJECT_0)
                throw std::runtime_error("process audio activation timed out");
            check(activation->result,"process-only loopback result");
            const auto client=activation->client;
            WAVEFORMATEX format{};
            format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=48000;
            format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=192000;
            check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,0,0,&format,nullptr),"initialize process mix PCM16");
            Event samples;
            check(client->SetEventHandle(samples.value),"set capture event");
            ComPtr<IAudioCaptureClient> capture;
            check(client->GetService(IID_PPV_ARGS(&capture)),"get capture client");
            wave.open(directory/"mixed.wav",std::ios::binary);
            timeline.open(directory/"packets.csv",std::ios::binary);
            if(!wave || !timeline) throw std::runtime_error("cannot open mixed audio artifacts");
            header(wave,0);
            timeline<<"packet,sample_offset,frames,device_position,qpc_100ns,flags\n";
            check(client->Start(),"start process capture");
            struct StopClient {IAudioClient* client;~StopClient(){client->Stop();}} stop_client{client.Get()};
            const HANDLE events[]{stop.value,samples.value};
            // Bounded at120s/23MB. Packet copy releases WASAPI before disk I/O.
            std::vector<char> packet_bytes(48000*4);
            bool stopping=false;
            for(;;) {
                target.requireAlive();
                const auto wait=stopping?WaitForSingleObject(samples.value,100):WaitForMultipleObjects(2,events,FALSE,100);
                if(wait==WAIT_FAILED) throw std::runtime_error("audio capture wait failed");
                if(!stopping && wait==WAIT_OBJECT_0) stopping=true;
                ResetEvent(samples.value);
                UINT32 available=0;
                while(true) {
                    check(capture->GetNextPacketSize(&available),"next audio packet");
                    if(!available) break;
                    BYTE* data=nullptr;UINT32 count=0;DWORD flags=0;UINT64 position=0,qpc=0;
                    check(capture->GetBuffer(&data,&count,&flags,&position,&qpc),"get audio packet");
                    struct Release {IAudioCaptureClient* c;UINT32 n;~Release(){if(c)c->ReleaseBuffer(n);}} release{capture.Get(),count};
                    if(!count || count>48000 || frames+count>48000*120 || packets>=20000)
                        throw std::runtime_error("audio packet/session bound exceeded");
                    const auto bytes=count*4;
                    if(flags&AUDCLNT_BUFFERFLAGS_SILENT) std::fill_n(packet_bytes.data(),bytes,char{0});
                    else {
                        if(!data) throw std::runtime_error("missing nonsilent audio buffer");
                        std::copy_n(reinterpret_cast<const char*>(data),bytes,packet_bytes.data());
                    }
                    check(capture->ReleaseBuffer(count),"release audio packet");release.c=nullptr;
                    if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) ++discontinuities;
                    if(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) ++timestamp_errors;
                    if(packets && position!=expected_position) ++position_gaps;
                    if(position) ++nonzero_positions;
                    if(qpc>static_cast<UINT64>(INT64_MAX) || (last_qpc>=0 && qpc<=static_cast<UINT64>(last_qpc)))
                        throw std::runtime_error("nonmonotonic audio QPC");
                    if(first_qpc<0) first_qpc=static_cast<std::int64_t>(qpc);
                    last_qpc=static_cast<std::int64_t>(qpc);expected_position=position+count;
                    last_packet_end=qpc+static_cast<std::uint64_t>(count)*10000000/48000;
                    for(std::size_t i=0;i<bytes;i+=2)
                        if(packet_bytes[i] || packet_bytes[i+1]) ++nonzero;
                    wave.write(packet_bytes.data(),bytes);
                    timeline<<packets<<','<<frames<<','<<count<<','<<position<<','<<qpc<<','<<flags<<'\n';
                    if(!wave || !timeline) throw std::runtime_error("audio artifact write failed");
                    frames+=count;++packets;
                    if(packets==1) SetEvent(ready.value);
                }
                if(stopping) {
                    const auto end=requested_end.load();
                    if(!end || last_packet_end>=end) break;
                    if(qpc100ns()>end+3000000) throw std::runtime_error("audio tail does not cover requested stop time");
                }
                if(qpc100ns()-origin>1200000000) throw std::runtime_error("audio wall-time bound exceeded");
            }
            check(client->Stop(),"stop process capture");
            if(!packets) fail("no process audio packets captured");
            if(!requested_end.load()) fail("capture destroyed without an explicit stop boundary");
            // Observed on the Windows process-loopback virtual device: every
            // device-position field is zero, while QPC advances. Do not invent
            // a device counter or call this proof of sample continuity.
            if(discontinuities || timestamp_errors || (nonzero_positions && position_gaps))
                fail("audio packet timing/glitch flags require review");
        } catch(const std::exception& e) {fail(e.what());}
        catch(...) {fail("unknown process capture failure");}
        try {
            if(wave.is_open()) {wave.seekp(0);header(wave,static_cast<std::uint32_t>(frames*4));wave.close();if(!wave)fail("audio WAV finalization failed");}
            if(timeline.is_open()) {timeline.close();if(!timeline)fail("audio timeline finalization failed");}
            std::ofstream report(directory/"recording.json",std::ios::binary);
            report<<"{\n\"schema_version\":1,\"kind\":\"windows_process_mix\",\n"
                  <<"\"target_pid\":"<<target.pid<<",\"scope\":\""
                  <<(target.external?"include_selected_process_tree":"include_current_process_tree")<<"\",\n"
                  <<"\"target_executable_verified\":"<<(target.external?"true":"false")
                  <<",\"target_creation_filetime\":"<<target.created<<",\n"
                  <<"\"microphone\":false,\"system_fallback\":false,\"windows_autoconvert_pcm\":true,\n"
                  <<"\"sample_rate\":48000,\"channels\":2,\"bits\":16,\"qpc_origin_100ns\":"<<origin<<",\n"
                  <<"\"packets\":"<<packets<<",\"sample_frames\":"<<frames<<",\"nonzero_samples\":"<<nonzero<<",\n"
                  <<"\"discontinuities\":"<<discontinuities<<",\"timestamp_errors\":"<<timestamp_errors<<",\"position_gaps\":"<<position_gaps<<",\n"
                  <<"\"device_position_status\":\""<<(nonzero_positions?"reported":"unavailable_all_zero")<<"\",\"sample_continuity_verified\":false,\n"
                  <<"\"requested_end_qpc_100ns\":"<<requested_end.load()<<",\"last_packet_end_qpc_100ns\":"<<last_packet_end<<",\n"
                  <<"\"first_packet_qpc_100ns\":"<<first_qpc<<",\"last_packet_qpc_100ns\":"<<last_qpc<<",\n"
                  <<"\"complete\":"<<(error().empty()?"true":"false")<<",\"original_parity\":false\n}\n";
            report.close();if(!report)fail("audio summary write failed");
            if(!error().empty()) {std::ofstream detail(directory/"error.txt");detail<<error()<<'\n';}
            valid=error().empty();
        } catch(...) {fail("audio finalization exception");}
        SetEvent(ready.value);
    }
};
ProcessAudioCapture::ProcessAudioCapture(const std::filesystem::path& path,std::uint64_t origin)
    :impl_(std::make_unique<Impl>(path,origin,GetCurrentProcessId(),std::filesystem::path{},false)) {
    if(WaitForSingleObject(impl_->ready.value,6000)!=WAIT_OBJECT_0) throw std::runtime_error("audio startup timed out");
    if(!impl_->error().empty()) throw std::runtime_error(impl_->error());
}
ProcessAudioCapture::ProcessAudioCapture(const std::filesystem::path& path,std::uint64_t origin,
                                       std::uint32_t pid,const std::filesystem::path& expected)
    :impl_(std::make_unique<Impl>(path,origin,pid,expected,true)) {
    if(WaitForSingleObject(impl_->ready.value,6000)!=WAIT_OBJECT_0) throw std::runtime_error("audio startup timed out");
    if(!impl_->error().empty()) throw std::runtime_error(impl_->error());
}
ProcessAudioCapture::~ProcessAudioCapture()=default;
void ProcessAudioCapture::finish() {
    if(impl_->finished) return;
    impl_->requested_end=ProcessAudioCapture::qpc100ns();
    SetEvent(impl_->stop.value);if(impl_->thread.joinable())impl_->thread.join();impl_->finished=true;
}
bool ProcessAudioCapture::complete() const {return impl_->valid;}
std::string ProcessAudioCapture::error() const {return impl_->error();}
}

#include "frontend_music.hpp"
#include "music_pcm.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <thread>

namespace nba97 {
struct FrontendMusicPlayer::Stream {
    static constexpr std::size_t buffer_samples=2048, buffer_count=4;
    std::vector<std::int16_t> pcm;
    std::atomic<unsigned> gain, starvation{0};
    std::atomic<bool> playing{false};
    HANDLE stop_event=nullptr, ready_event=nullptr, done_event=nullptr;
    std::thread worker;
    mutable std::mutex mutex;
    std::string failure;
    Stream(std::vector<std::int16_t> data,unsigned volume,const EaSchlInfo& info):pcm(std::move(data)),gain(volume) {
        stop_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        ready_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        done_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!stop_event || !ready_event || !done_event) {closeEvents();throw std::runtime_error("music stream event creation failed");}
        try {worker=std::thread([this,info]{run(info);});}
        catch(...) {closeEvents();throw;}
    }
    ~Stream() {SetEvent(stop_event);if(worker.joinable())worker.join();closeEvents();}
    void closeEvents() noexcept {
        if(stop_event)CloseHandle(stop_event);
        if(ready_event)CloseHandle(ready_event);
        if(done_event)CloseHandle(done_event);
    }
    std::string error() const {std::lock_guard<std::mutex> lock(mutex);return failure;}
    void run(const EaSchlInfo& info) noexcept {
        HWAVEOUT device=nullptr;
        std::array<WAVEHDR,buffer_count> headers{};
        std::array<std::array<std::int16_t,buffer_samples>,buffer_count> buffers{};
        auto check=[](MMRESULT result,const char* operation) {
            if(result!=MMSYSERR_NOERROR)throw std::runtime_error(std::string(operation)+": "+std::to_string(result));
        };
        try {
            WAVEFORMATEX format{};
            format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=info.sample_rate;
            format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=info.sample_rate*4;
            check(waveOutOpen(&device,WAVE_MAPPER,&format,reinterpret_cast<DWORD_PTR>(done_event),0,CALLBACK_EVENT),"music stream open");
            std::size_t position=0,next=0;
            for(std::size_t i=0;i<buffer_count;++i) {
                auto& h=headers[i];h.lpData=reinterpret_cast<LPSTR>(buffers[i].data());h.dwBufferLength=sizeof(buffers[i]);
                check(waveOutPrepareHeader(device,&h,sizeof(h)),"music buffer prepare");
                fillMusicPcm(pcm,position,buffers[i].data(),buffer_samples,gain.load());
                check(waveOutWrite(device,&h,sizeof(h)),"music buffer submit");
            }
            playing=true;SetEvent(ready_event);
            const HANDLE events[]{stop_event,done_event};
            for(;;) {
                const auto wait=WaitForMultipleObjects(2,events,FALSE,100);
                if(wait==WAIT_OBJECT_0)break;
                if(wait==WAIT_FAILED)throw std::runtime_error("music stream wait failed");
                ResetEvent(done_event);
                if(std::all_of(headers.begin(),headers.end(),[](const WAVEHDR& h){return (h.dwFlags&WHDR_DONE)!=0;}))++starvation;
                // FIFO order matters after a scheduling stall; array-order
                // resubmission at a wrapped queue position would reorder music.
                for(std::size_t n=0;n<buffer_count && (headers[next].dwFlags&WHDR_DONE);++n) {
                    fillMusicPcm(pcm,position,buffers[next].data(),buffer_samples,gain.load());
                    check(waveOutWrite(device,&headers[next],sizeof(WAVEHDR)),"music buffer refill");
                    next=(next+1)%buffer_count;
                }
            }
        } catch(const std::exception& e) {std::lock_guard<std::mutex> lock(mutex);failure=e.what();}
        catch(...) {std::lock_guard<std::mutex> lock(mutex);failure="unknown music stream failure";}
        playing=false;
        if(device) {
            waveOutReset(device);
            for(auto& h:headers)if(h.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(device,&h,sizeof(h));
            waveOutClose(device);
        }
        SetEvent(ready_event);
    }
};

FrontendMusicPlayer::FrontendMusicPlayer()=default;
FrontendMusicPlayer::~FrontendMusicPlayer() { stop(); }

void FrontendMusicPlayer::start(const std::filesystem::path& cnk_path,
                                std::uint8_t recovered_volume) {
    stop();
    info_ = {};
    auto decoded=loadEaSchl(cnk_path);
    info_=decoded.info;
    auto stream=std::make_unique<Stream>(std::move(decoded.samples),recovered_volume,info_);
    if(WaitForSingleObject(stream->ready_event,6000)!=WAIT_OBJECT_0)throw std::runtime_error("music stream startup timeout");
    if(!stream->error().empty())throw std::runtime_error(stream->error());
    stream_=std::move(stream);
    decoder_name_ = "native EA SCHl/TMxl + PS ADPCM -> four queued PCM buffers; music-only software gain";
}

void FrontendMusicPlayer::stop() noexcept {
    stream_.reset();decoder_name_.clear();
}

void FrontendMusicPlayer::setRecoveredVolume(std::uint8_t volume) noexcept {
    if(stream_)stream_->gain=(std::min<unsigned>)(volume,127);
}
bool FrontendMusicPlayer::isPlaying() const noexcept {return stream_ && stream_->playing;}
std::string FrontendMusicPlayer::error() const {return stream_?stream_->error():std::string{};}
unsigned FrontendMusicPlayer::underruns() const noexcept {return stream_?stream_->starvation.load():0;}
} // namespace nba97

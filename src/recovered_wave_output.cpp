#include "recovered_wave_output.hpp"
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace nba97 {
namespace {
class WinMmApi final:public RecoveredWaveApi {
public:
    MMRESULT open(HWAVEOUT* h,const WAVEFORMATEX* f) noexcept override {return waveOutOpen(h,WAVE_MAPPER,f,0,0,CALLBACK_NULL);}
    MMRESULT prepare(HWAVEOUT h,WAVEHDR* p) noexcept override {return waveOutPrepareHeader(h,p,sizeof(*p));}
    MMRESULT write(HWAVEOUT h,WAVEHDR* p) noexcept override {return waveOutWrite(h,p,sizeof(*p));}
    MMRESULT reset(HWAVEOUT h) noexcept override {return waveOutReset(h);}
    MMRESULT unprepare(HWAVEOUT h,WAVEHDR* p) noexcept override {return waveOutUnprepareHeader(h,p,sizeof(*p));}
    MMRESULT close(HWAVEOUT h) noexcept override {return waveOutClose(h);}
};
const char* operationName(RecoveredWaveOperation op) noexcept {
    switch(op){
    case RecoveredWaveOperation::Open:return "waveOutOpen";
    case RecoveredWaveOperation::Prepare:return "waveOutPrepareHeader";
    case RecoveredWaveOperation::Write:return "waveOutWrite";
    case RecoveredWaveOperation::Reset:return "waveOutReset";
    case RecoveredWaveOperation::AwaitReturn:return "waiting for WHDR_DONE";
    case RecoveredWaveOperation::Unprepare:return "waveOutUnprepareHeader";
    case RecoveredWaveOperation::Close:return "waveOutClose";
    default:return "WinMM";
    }
}
[[noreturn]] void fail(RecoveredWaveFailure f){
    throw std::runtime_error(std::string(operationName(f.operation))+" recovered clip failed: "+std::to_string(f.code));
}
DWORD flags(const WAVEHDR& h) noexcept {
    // WinMM documents driver updates to these flags. Poll through volatile;
    // headers remain stable and are never modified/released while borrowed.
    return *static_cast<const volatile DWORD*>(&h.dwFlags);
}
struct ClipGeneration {
    const std::uint64_t generation;
    std::vector<std::int16_t> pcm;
    WAVEHDR header{};
    bool prepared=false,submitted=false,interrupted=false;
    ClipGeneration(std::uint64_t g,std::vector<std::int16_t> p):generation(g),pcm(std::move(p)){
        header.lpData=reinterpret_cast<LPSTR>(pcm.data());
        header.dwBufferLength=static_cast<DWORD>(pcm.size()*sizeof(std::int16_t));
    }
    RecoveredWaveProgress progress() const noexcept {
        const bool returned=submitted&&(flags(header)&WHDR_DONE)!=0;
        return {generation,submitted,returned,returned&&!interrupted,interrupted,false};
    }
};
}
struct RecoveredWaveOutput::Session {
    std::shared_ptr<RecoveredWaveApi> api;
    HWAVEOUT device=nullptr;
    std::uint32_t sample_rate=0;
    std::unique_ptr<ClipGeneration> clip;
    Session* next=nullptr;
    explicit Session(std::shared_ptr<RecoveredWaveApi> a):api(std::move(a)){}
};

RecoveredWaveOutput::RecoveredWaveOutput():RecoveredWaveOutput(std::make_shared<WinMmApi>()){}
RecoveredWaveOutput::RecoveredWaveOutput(std::shared_ptr<RecoveredWaveApi> api):api_(std::move(api)){
    if(!api_)throw std::invalid_argument("missing recovered waveform API");
}
RecoveredWaveOutput::Session*& RecoveredWaveOutput::retained() noexcept {static Session* head=nullptr;return head;}
void RecoveredWaveOutput::diagnose(RecoveredWaveFailure f) noexcept {
    char text[192];std::snprintf(text,sizeof text,"Recovered audio: %s incomplete (%u); unreleased output ownership retained.\n",operationName(f.operation),static_cast<unsigned>(f.code));
    OutputDebugStringA(text);
}
bool RecoveredWaveOutput::retire(Session& s,bool keep,RecoveredWaveFailure& failure,RecoveredWaveProgress& last) noexcept {
    if(s.clip){
        auto& c=*s.clip;
        if(c.prepared&&c.submitted&&!(flags(c.header)&WHDR_DONE)){
            c.interrupted=true;
            const auto result=s.api->reset(s.device);
            if(result!=MMSYSERR_NOERROR){failure={RecoveredWaveOperation::Reset,result};return false;}
            // Native WinMM ownership boundary: even successful reset must be
            // followed by the driver's return flag before unprepare/free.
            if(!(flags(c.header)&WHDR_DONE)){
                failure={RecoveredWaveOperation::AwaitReturn,WAVERR_STILLPLAYING};return false;
            }
        }
        const auto progress=c.progress(); // Unprepare may change driver flags.
        if(c.prepared){
            const auto result=s.api->unprepare(s.device,&c.header);
            if(result!=MMSYSERR_NOERROR){failure={RecoveredWaveOperation::Unprepare,result};return false;}
            c.prepared=false;
        }
        last=progress;last.storage_released=true;
        s.clip.reset(); // Only after the actual successful unprepare.
    }
    if(s.device&&!keep){
        const auto result=s.api->close(s.device);
        if(result!=MMSYSERR_NOERROR){failure={RecoveredWaveOperation::Close,result};return false;}
        s.device=nullptr;s.sample_rate=0;
    }
    return true;
}
RecoveredWaveOutput::~RecoveredWaveOutput(){
    if(session_&&!retire(*session_,false,failure_,last_)){
        diagnose(failure_);
        session_->next=retained();retained()=session_.release();
    }
}
std::uint64_t RecoveredWaveOutput::play(std::vector<std::int16_t> pcm,std::uint32_t rate){
    if(pcm.empty()||!rate||rate>(std::numeric_limits<DWORD>::max)()/2u||
       pcm.size()>(std::numeric_limits<DWORD>::max)()/sizeof(std::int16_t))
        throw std::invalid_argument("invalid recovered PCM output size/rate");
    if(next_generation_==(std::numeric_limits<std::uint64_t>::max)())
        throw std::overflow_error("native recovered output generation exhausted");
    collectRetained();
    failure_={};
    if(session_&&!retire(*session_,session_->sample_rate==rate,failure_,last_))fail(failure_);
    if(!session_)session_=std::make_unique<Session>(api_);
    auto& s=*session_;
    if(!s.device){
        WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_PCM;f.nChannels=1;f.nSamplesPerSec=rate;
        f.wBitsPerSample=16;f.nBlockAlign=2;f.nAvgBytesPerSec=rate*2;
        const auto result=s.api->open(&s.device,&f);
        if(result!=MMSYSERR_NOERROR){s.device=nullptr;failure_={RecoveredWaveOperation::Open,result};fail(failure_);}
        s.sample_rate=rate;
    }
    s.clip=std::make_unique<ClipGeneration>(++next_generation_,std::move(pcm));
    auto& c=*s.clip;
    auto result=s.api->prepare(s.device,&c.header);
    if(result==MMSYSERR_NOERROR){
        c.prepared=true;result=s.api->write(s.device,&c.header);
        if(result==MMSYSERR_NOERROR){c.submitted=true;return c.generation;}
        failure_={RecoveredWaveOperation::Write,result};
    }else failure_={RecoveredWaveOperation::Prepare,result};
    const auto submission_failure=failure_;
    // Preserve any cleanup failure separately as the current ownership error;
    // the thrown error still identifies the failed submission operation.
    retire(s,false,failure_,last_);fail(submission_failure);
}
void RecoveredWaveOutput::stop() noexcept {
    collectRetained();
    failure_={};
    if(session_&&!retire(*session_,false,failure_,last_))diagnose(failure_);
}
bool RecoveredWaveOutput::isPlaying() const noexcept {
    return session_&&session_->clip&&session_->clip->submitted&&!(flags(session_->clip->header)&WHDR_DONE);
}
RecoveredWaveProgress RecoveredWaveOutput::progress() const noexcept {
    return session_&&session_->clip?session_->clip->progress():last_;
}
bool RecoveredWaveOutput::isCurrentGeneration(std::uint64_t generation) const noexcept {
    return generation&&session_&&session_->clip&&session_->clip->submitted&&session_->clip->generation==generation;
}
std::size_t RecoveredWaveOutput::collectRetained() noexcept {
    Session** link=&retained();
    while(*link){
        Session* s=*link;RecoveredWaveFailure f{};RecoveredWaveProgress p{};
        if(retire(*s,false,f,p)){*link=s->next;delete s;}
        else link=&s->next;
    }
    return retainedCount();
}
std::size_t RecoveredWaveOutput::retainedCount() noexcept {
    std::size_t n=0;for(auto* s=retained();s;s=s->next)++n;return n;
}
} // namespace nba97

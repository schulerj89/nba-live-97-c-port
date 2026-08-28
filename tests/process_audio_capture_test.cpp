#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include "process_audio_capture.hpp"
#include "frontend_music.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs=std::filesystem;
void require(bool value,const char* text) {if(!value)throw std::runtime_error(text);}
class Tone {
    HWAVEOUT device=nullptr;WAVEHDR header{};std::vector<std::int16_t> pcm;
public:
    Tone(double hz,int channel):pcm(48000*2) {
        for(unsigned i=0;i<48000;++i) pcm[2*i+channel]=static_cast<std::int16_t>(1200*std::sin(6.283185307179586*hz*i/48000));
        WAVEFORMATEX format{WAVE_FORMAT_PCM,2,48000,192000,4,16,0};
        require(waveOutOpen(&device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)==MMSYSERR_NOERROR,"tone device open");
        header.lpData=reinterpret_cast<LPSTR>(pcm.data());header.dwBufferLength=static_cast<DWORD>(pcm.size()*2);
        header.dwFlags=WHDR_BEGINLOOP|WHDR_ENDLOOP;header.dwLoops=0xffffffffu;
        if(waveOutPrepareHeader(device,&header,sizeof(header))!=MMSYSERR_NOERROR) {waveOutClose(device);device=nullptr;throw std::runtime_error("tone prepare");}
        if(waveOutWrite(device,&header,sizeof(header))!=MMSYSERR_NOERROR) {stop();throw std::runtime_error("tone submit");}
    }
    void stop(){if(device){waveOutReset(device);waveOutUnprepareHeader(device,&header,sizeof(header));waveOutClose(device);device=nullptr;}}
    ~Tone(){stop();}
};
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--reference-tone") {
            Tone tone(660,0);Sleep(8000);return 0;
        }
        if(argc==3 && std::string(argv[1])=="--selected-live") {
            const auto directory=fs::absolute(argv[2]);
            const auto relative=fs::weakly_canonical(directory).lexically_relative(fs::canonical(".local"));
            require(!relative.empty() && relative!="." && !relative.is_absolute() && *relative.begin()!="..",
                    "selected live destination must be private");
            std::wstring executable(32768,L'\0');
            const auto length=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
            require(length && length<executable.size(),"selected live executable identity");executable.resize(length);
            std::wstring command=L"\""+executable+L"\" --reference-tone";
            STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
            require(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,
                    nullptr,nullptr,&startup,&process)!=FALSE,"selected live child launch");
            struct Child {PROCESS_INFORMATION p;~Child(){CloseHandle(p.hThread);CloseHandle(p.hProcess);}} child{process};
            // The selected child produces660Hz. The recorder's own two tones
            // and any already-running app audio must be absent from its PCM.
            Tone excluded_left(440,0),excluded_right(880,1);
            nba97::ProcessAudioCapture capture(directory,nba97::ProcessAudioCapture::qpc100ns(),process.dwProcessId,executable);
            Sleep(2500);capture.finish();
            std::cout<<"PROCESS AUDIO selected-live complete="<<capture.complete()<<" target_pid="<<process.dwProcessId
                     <<"; child660Hz included, recorder440/880Hz must be excluded; inspect PCM\n";
            return capture.complete()?0:2;
        }
        if(argc==4 && std::string(argv[1])=="--music-isolation") {
            // Explicit, audible diagnostic. Production music mute must not
            // silence the two independently playing low-volume test streams.
            const auto directory=fs::absolute(argv[2]);
            const auto relative=fs::weakly_canonical(directory).lexically_relative(fs::canonical(".local"));
            require(!relative.empty() && !relative.is_absolute() && *relative.begin()!="..", "isolation destination must be private");
            const auto music=fs::canonical(argv[3]);
            const auto music_relative=music.lexically_relative(fs::canonical(".local"));
            require(!music_relative.empty() && !music_relative.is_absolute() && *music_relative.begin()!="..", "music source must be private");
            const auto origin=nba97::ProcessAudioCapture::qpc100ns();
            nba97::ProcessAudioCapture capture(directory,origin);
            std::ofstream events(directory/"music-isolation.csv");
            events<<"event,qpc_100ns\n";
            auto mark=[&](const char* event){events<<event<<','<<nba97::ProcessAudioCapture::qpc100ns()<<'\n';events.flush();};
            {Tone left(440,0),right(880,1);
                mark("tones_started");Sleep(1600);
                nba97::FrontendMusicPlayer player;
                mark("music_mute_call");player.start(music,0);mark("music_muted");Sleep(1600);
                require(player.isPlaying() && player.error().empty(),"muted music stream failed instead of running");
                require(player.underruns()==0,"muted music stream starved; inspect scheduling");
                player.stop();mark("music_stopped");Sleep(1600);
            }
            mark("tones_stopped");Sleep(300);capture.finish();
            std::cout<<"PROCESS AUDIO music-isolation capture_complete="<<capture.complete()
                     <<"; inspect PCM before/during/after production music mute; capture success is NOT isolation success\n";
            return capture.complete()?0:2;
        }
        if(argc==2 && std::string(argv[1])=="--foreign-tone") {
            Tone tone(660,0);Sleep(5000);return 0;
        }
        if(argc==3 && std::string(argv[1])=="--live") {
            // Explicit live test only, never part of device-free CI. Two real
            // concurrent WinMM streams, captured through the production API.
            auto directory=fs::absolute(argv[2]);
            const auto relative=fs::weakly_canonical(directory).lexically_relative(fs::canonical(".local"));
            require(!relative.empty() && !relative.is_absolute() && *relative.begin()!="..", "live test destination must be private");
            const auto origin=nba97::ProcessAudioCapture::qpc100ns();
            nba97::ProcessAudioCapture capture(directory,origin);
            Sleep(300);
            {Tone left(440,0),right(880,1);Sleep(2200);}
            Sleep(300);capture.finish();capture.finish();
            std::cout<<"PROCESS AUDIO live complete="<<capture.complete()<<" detail="<<capture.error()<<" output="<<directory<<'\n';
            return capture.complete()?0:2;
        }
        require(argc==1,"unknown test arguments");
        const auto start=nba97::ProcessAudioCapture::qpc100ns();
        require(start>0,"QPC epoch");
        for(int i=0;i<100;++i) require(nba97::ProcessAudioCapture::qpc100ns()>=start,"QPC nonmonotonic");
        const auto existing=fs::temp_directory_path();
        bool refused=false;
        try{nba97::ProcessAudioCapture invalid(existing,start);}catch(const std::exception&){refused=true;}
        require(refused,"existing output directory not refused");
        const auto rejectTarget=[&](DWORD pid,const fs::path& expected,const char* reason) {
            bool rejected=false;
            try {nba97::ProcessAudioCapture invalid(existing,start,pid,expected);}
            catch(const std::exception& e) {rejected=std::string(e.what()).find(reason)!=std::string::npos;}
            require(rejected,"explicit target did not fail at the expected pre-capture guard");
        };
        rejectTarget(0,existing,"requires PID");
        rejectTarget(GetCurrentProcessId(),{},"requires PID");
        rejectTarget(MAXDWORD,existing,"cannot open");
        rejectTarget(GetCurrentProcessId(),existing,"does not match");
        std::wstring executable(32768,L'\0');
        const auto length=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
        require(length && length<executable.size(),"test executable identity");executable.resize(length);
        rejectTarget(GetCurrentProcessId(),executable,"directory must be fresh");
        std::cout<<"PROCESS AUDIO PASS explicit_target_zero_missing_invalid_mismatch_and_verified_identity; no device capture attempted\n";
        std::cout<<"PROCESS AUDIO PASS monotonic_clock_existing_path_refusal; no device capture attempted; live mix/isolation tested separately\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"PROCESS AUDIO FAIL "<<e.what()<<'\n';return 1;}
}

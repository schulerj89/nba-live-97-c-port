#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "process_audio_capture.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// Explicit, bounded diagnostic. Does not launch/control the emulator, alter
// volume, capture microphones, or fall back to a system-wide recording device.
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc!=5) throw std::runtime_error("usage: nba97_capture_reference_audio PID EXPECTED_NO$PSX_EXE FRESH_PRIVATE_DIRECTORY SECONDS(1..110)");
        auto number=[](const wchar_t* text) {
            const std::wstring value(text);std::size_t used=0;
            if(value.empty() || value.find_first_not_of(L"0123456789")!=std::wstring::npos)
                throw std::runtime_error("PID/duration must be positive decimal integers");
            const auto result=std::stoull(value,&used);
            if(used!=value.size()) throw std::runtime_error("invalid integer");
            return result;
        };
        const auto pid=number(argv[1]),seconds=number(argv[4]);
        if(!pid || pid>MAXDWORD || !seconds || seconds>110) throw std::runtime_error("PID/duration outside bounds");
        const auto executable=std::filesystem::canonical(argv[2]);
        if(_wcsicmp(executable.filename().c_str(),L"NO$PSX.EXE")!=0)
            throw std::runtime_error("reference recorder is restricted to the explicitly named no$psx executable");
        const auto output=std::filesystem::weakly_canonical(std::filesystem::absolute(argv[3]));
        const auto root=std::filesystem::canonical(".local");
        const auto relative=output.lexically_relative(root);
        if(relative.empty() || relative=="." || relative.is_absolute() || *relative.begin()=="..")
            throw std::runtime_error("recording destination must be a fresh child of repository .local");
        const auto origin=nba97::ProcessAudioCapture::qpc100ns();
        nba97::ProcessAudioCapture capture(output,origin,static_cast<DWORD>(pid),executable);
        std::cout<<"REFERENCE-AUDIO recording verified no$psx PID="<<pid<<" for "<<seconds
                 <<" seconds; process tree only; 48kHz stereo PCM16; no normalization or global audio changes\n"<<std::flush;
        for(unsigned elapsed=0;elapsed<seconds;++elapsed) {
            Sleep(1000);
            if(!capture.error().empty()) break;
        }
        capture.finish();
        std::cout<<"REFERENCE-AUDIO complete="<<capture.complete()<<" detail="<<capture.error()
                 <<"; capture success is not original/native sound parity\n";
        return capture.complete()?0:2;
    } catch(const std::exception& e) {std::cerr<<"REFERENCE-AUDIO refused/failed: "<<e.what()<<'\n';return 1;}
}

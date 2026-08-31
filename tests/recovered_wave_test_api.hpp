#pragma once
#include "recovered_wave_output.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace nba97_wave_test {
struct Fake final:nba97::RecoveredWaveApi {
    MMRESULT open_error=0,prepare_error=0,write_error=0,reset_error=0,unprepare_error=0,close_error=0;
    bool opened=false,queued=false,reset_returns_buffer=true,clear_flags_on_unprepare=false;
    WAVEHDR* borrowed=nullptr;
    unsigned opens=0,closes=0,resets=0,unprepares=0,writes=0;
    std::vector<std::string> calls;
    std::vector<std::uint8_t> last_unprepared;
    MMRESULT open(HWAVEOUT* h,const WAVEFORMATEX* f) noexcept override {
        calls.push_back("open");++opens;
        if(open_error)return open_error;
        if(opened||f->nChannels!=1||f->wBitsPerSample!=16||f->nBlockAlign!=2)return MMSYSERR_INVALPARAM;
        opened=true;*h=reinterpret_cast<HWAVEOUT>(std::uintptr_t(opens));return 0;
    }
    MMRESULT prepare(HWAVEOUT,WAVEHDR* p) noexcept override {
        calls.push_back("prepare");if(prepare_error)return prepare_error;
        if(borrowed||!opened||p->dwFlags)return MMSYSERR_INVALPARAM;
        borrowed=p;p->dwFlags|=WHDR_PREPARED;return 0;
    }
    MMRESULT write(HWAVEOUT,WAVEHDR* p) noexcept override {
        calls.push_back("write");++writes;if(write_error)return write_error;
        if(p!=borrowed)return WAVERR_UNPREPARED;
        queued=true;p->dwFlags|=WHDR_INQUEUE;p->dwFlags&=~WHDR_DONE;return 0;
    }
    MMRESULT reset(HWAVEOUT) noexcept override {
        calls.push_back("reset");++resets;if(reset_error)return reset_error;
        if(reset_returns_buffer)finish();return 0;
    }
    MMRESULT unprepare(HWAVEOUT,WAVEHDR* p) noexcept override {
        calls.push_back("unprepare");++unprepares;if(unprepare_error)return unprepare_error;
        if(queued)return WAVERR_STILLPLAYING;
        if(p!=borrowed)return MMSYSERR_INVALPARAM;
        last_unprepared.assign(reinterpret_cast<std::uint8_t*>(p->lpData),reinterpret_cast<std::uint8_t*>(p->lpData)+p->dwBufferLength);
        p->dwFlags=clear_flags_on_unprepare?0:p->dwFlags&~WHDR_PREPARED;borrowed=nullptr;return 0;
    }
    MMRESULT close(HWAVEOUT) noexcept override {
        calls.push_back("close");++closes;if(close_error)return close_error;
        if(queued||borrowed)return WAVERR_STILLPLAYING;opened=false;return 0;
    }
    void finish() noexcept {queued=false;if(borrowed){borrowed->dwFlags|=WHDR_DONE;borrowed->dwFlags&=~WHDR_INQUEUE;}}
};
}

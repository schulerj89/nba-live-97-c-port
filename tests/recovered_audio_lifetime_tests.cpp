#include "recovered_audio.hpp"
#include "recovered_wave_test_api.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
unsigned checks;
void check(bool ok) { ++checks; if (!ok) throw std::runtime_error("audio lifetime check " + std::to_string(checks)); }
template<class F> void throws(F f) { bool threw=false; try { f(); } catch(const std::runtime_error&) { threw=true; } check(threw); }
using Bytes=std::vector<std::uint8_t>;
using nba97_wave_test::Fake;
void put(Bytes& b,std::size_t offset,std::uint32_t value,unsigned size=4) {
    for(unsigned i=0;i<size;++i)b.at(offset+i)=static_cast<std::uint8_t>(value>>(i*8));
}
void tag(Bytes& b,std::size_t offset,const char* text) {std::copy_n(text,4,b.begin()+offset);}
void write(const std::filesystem::path& path,const Bytes& b) {
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());
    if(!out)throw std::runtime_error("cannot write audio lifetime fixture");
}
struct Temp {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("nba97-audio-lifetime-"+std::to_string(GetCurrentProcessId()));
    Temp(){check(std::filesystem::create_directory(path));}
    ~Temp(){std::error_code ec;for(const char* p:{"fixture.vh","fixture.vb","zcursor_pitch.bin","expected.wav"})std::filesystem::remove(path/p,ec);std::filesystem::remove(path,ec);}
};
void cursorFixture(const Temp& temp) {
    // Invented supported BNKl/PATl/tone/TMxl with full ranges and constant
    // envelope; all-zero pitch table keeps this lifetime test at unity pitch.
    Bytes h(136,0);tag(h,0,"BNKl");h[4]=1;put(h,6,2,2);put(h,12,4);
    constexpr unsigned p=16,t=32,m=72,e=128;
    tag(h,p,"PATl");h[p+4]=h[p+6]=h[p+7]=1;h[p+10]=64;h[p+11]=127;put(h,p+12,4);
    h[t+1]=h[t+3]=127;put(h,t+4,0xffffffff);h[t+9]=60;h[t+10]=1;h[t+11]=255;h[t+12]=64;
    h[t+15]=1;h[t+16]=64;h[t+18]=127;put(h,t+36,e-(t+36));put(h,e,0xffffffff);put(h,e+4,127);
    tag(h,m,"TMxl");h[m+5]=16;h[m+6]=1;h[m+7]=6;put(h,m+10,22050,2);put(h,m+12,2048,2);put(h,m+16,28);
    put(h,m+20,0xffffffff);put(h,m+24,0xffffffff);put(h,m+36,16);put(h,m+44,0xffffffff);put(h,m+48,0xffffffff);
    Bytes b(16,0x17);b[0]=4;b[1]=1;
    write(temp.path/"fixture.vh",h);write(temp.path/"fixture.vb",b);write(temp.path/"zcursor_pitch.bin",Bytes(256,0));
}
void cursor() {
    Temp t;cursorFixture(t);auto api=std::make_shared<Fake>();nba97::RecoveredAudioPlayer player(api);
    unsigned accepted=0;auto call=[&]{++accepted;check(api->writes==accepted-1);};
    const auto h=t.path/"fixture.vh",b=t.path/"fixture.vb";
    const auto expected=player.exportCursorSound(h,b,1,t.path/"expected.wav",9);
    check(api->opens==0&&accepted==0);
    std::ifstream in(t.path/"expected.wav",std::ios::binary);Bytes wav{std::istreambuf_iterator<char>(in),{}};
    auto first=player.playCursorSound(h,b,1,9,call);check(accepted==1&&api->writes==1&&player.isPlaying());
    check(first.sample_count==expected.sample_count&&first.pitch_register==expected.pitch_register&&first.effective_volume==expected.effective_volume);
    auto* header=api->borrowed;auto* data=header->lpData;
    check(wav.size()==44+header->dwBufferLength&&std::memcmp(wav.data()+44,data,header->dwBufferLength)==0);
    // Original accepted-cue ordering is preserved on a native device failure:
    // the caller already accepted the cue before retirement/submission failed.
    api->reset_error=MMSYSERR_ERROR;
    throws([&]{player.playCursorSound(h,b,1,9,call);});
    check(accepted==2&&api->writes==1&&api->borrowed==header&&header->lpData==data);
    check(player.info().source==first.source&&player.info().rendered_sample_count==first.rendered_sample_count&&player.isPlaying());
    const auto resets=api->resets;
    auto muted=player.playCursorSound("missing","missing",1,0,[&]{++accepted;});
    check(muted.playback_suppressed&&accepted==2&&api->resets==resets&&player.isPlaying());
    throws([&]{player.playCursorSound("missing","missing",1,9,[&]{++accepted;});});
    check(accepted==2&&api->resets==resets&&api->borrowed==header);
    api->reset_error=0;player.stop();check(!player.isPlaying()&&!api->borrowed&&!api->opened);
    api->write_error=MMSYSERR_ERROR;
    throws([&]{player.playCursorSound(h,b,1,9,[&]{++accepted;});});
    check(accepted==3&&!player.isPlaying()&&!api->borrowed&&!api->opened);
    check(player.info().source==first.source);
}
nba97::PreparedCoolFact fact(unsigned record) {
    nba97::PreparedCoolFact p;p.info.record=record;p.info.sample_rate=22050;p.info.sample_count=5;
    p.info.source="invented lifetime fact";p.pcm={32767,-32768,100,-100,0};return p;
}
void speech() {
    auto api=std::make_shared<Fake>();
    {
        nba97::RecoveredAudioPlayer player(api);auto info=player.startCoolFact(fact(3),9);
        check(info.record==3&&player.isPlaying());
        auto* header=api->borrowed;const auto* pcm=reinterpret_cast<const std::int16_t*>(header->lpData);
        const auto gain=info.playback_volume;
        check(pcm[0]==std::int64_t(32767)*gain/127&&pcm[1]==std::int64_t(-32768)*gain/127);
        api->unprepare_error=WAVERR_STILLPLAYING;
        throws([&]{player.startCoolFact(fact(4),9);});
        check(player.info().record==3&&api->borrowed==header&&api->writes==1);
        check(!player.isPlaying()); // Reset returned it, but unprepare still owns storage.
    }
    check(nba97::RecoveredWaveOutput::retainedCount()==1&&api->borrowed&&api->opened);
    api->unprepare_error=0;check(nba97::RecoveredWaveOutput::collectRetained()==0&&!api->opened);
}
}
int main(){try{cursor();speech();std::cout<<checks<<" actual-player injected lifetime/accepted-cue checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

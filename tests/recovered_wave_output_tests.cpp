#include "recovered_wave_output.hpp"
#include "recovered_wave_test_api.hpp"
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
unsigned checks;
void check(bool ok){++checks;if(!ok)throw std::runtime_error("wave output check "+std::to_string(checks));}
using nba97_wave_test::Fake;
using nba97::RecoveredWaveOperation;
using nba97::RecoveredWaveOutput;
std::vector<std::int16_t> pcm(std::int16_t value){return {value,-1,32767,-32768,17};}
template<class F>void throws(F f){bool threw=false;try{f();}catch(const std::runtime_error&){threw=true;}check(threw);}
void reuse(){
    auto api=std::make_shared<Fake>();RecoveredWaveOutput out(api);
    auto first=out.play(pcm(11),22050);check(first==1&&api->opens==1&&out.isPlaying());
    auto* header=api->borrowed;auto* data=header->lpData;
    api->reset_error=MMSYSERR_ERROR;
    throws([&]{out.play(pcm(22),22050);});
    check(api->borrowed==header&&header->lpData==data&&reinterpret_cast<std::int16_t*>(data)[0]==11);
    check(api->writes==1&&api->unprepares==0&&out.isCurrentGeneration(first)&&out.isPlaying());
    check(out.failure().operation==RecoveredWaveOperation::Reset);
    api->reset_error=0;auto second=out.play(pcm(22),22050);
    check(second==2&&api->opens==1&&api->closes==0&&api->unprepares==1);
    check(!out.isCurrentGeneration(first)&&out.isCurrentGeneration(second));
    auto old=out.progress();api->finish();check(out.progress().natural&&out.progress().returned);
    auto third=out.play(pcm(33),44100);
    check(third==3&&api->opens==2&&api->closes==1&&!out.isCurrentGeneration(old.generation));
    check(!out.progress().returned&&out.isPlaying());
    out.stop();check(!out.isPlaying()&&!api->opened&&!api->borrowed);
    check(out.progress().interrupted&&!out.progress().natural&&out.progress().storage_released);
}
void failures(){
    auto api=std::make_shared<Fake>();RecoveredWaveOutput out(api);auto generation=out.play(pcm(4),22050);
    auto* header=api->borrowed;auto* data=header->lpData;
    api->reset_returns_buffer=false;out.stop();
    check(out.failure().operation==RecoveredWaveOperation::AwaitReturn&&out.failure().code==WAVERR_STILLPLAYING);
    check(api->borrowed==header&&api->unprepares==0&&api->closes==0&&out.isCurrentGeneration(generation));
    check(header->lpData==data&&out.isPlaying()&&!out.progress().returned);
    api->reset_returns_buffer=true;api->unprepare_error=MMSYSERR_ERROR;out.stop();
    check(out.failure().operation==RecoveredWaveOperation::Unprepare&&api->borrowed==header&&header->lpData==data);
    check(out.progress().returned&&!out.progress().natural&&!out.progress().storage_released);
    api->unprepare_error=0;api->close_error=MMSYSERR_ERROR;out.stop();
    check(out.failure().operation==RecoveredWaveOperation::Close&&api->borrowed==nullptr&&api->opened);
    check(out.progress().storage_released&&out.progress().generation==generation);
    api->close_error=0;out.stop();check(!api->opened&&out.failure().code==0);
}
void submission(){
    auto api=std::make_shared<Fake>();RecoveredWaveOutput out(api);
    api->open_error=MMSYSERR_NODRIVER;throws([&]{out.play(pcm(7),22050);});
    check(!api->borrowed&&!api->opened&&out.failure().operation==RecoveredWaveOperation::Open);
    api->open_error=0;api->prepare_error=MMSYSERR_NOMEM;throws([&]{out.play(pcm(7),22050);});
    check(!api->borrowed&&!api->opened&&api->writes==0&&api->unprepares==0);
    api->prepare_error=0;api->write_error=MMSYSERR_ERROR;api->unprepare_error=WAVERR_STILLPLAYING;
    throws([&]{out.play(pcm(7),22050);});
    check(api->borrowed&&api->opened&&out.failure().operation==RecoveredWaveOperation::Unprepare);
    check(!out.isPlaying()&&!out.progress().submitted&&api->resets==0);
    api->unprepare_error=0;api->write_error=0;out.stop();check(!api->borrowed&&!api->opened);
    auto g=out.play(pcm(7),22050);check(g==3); // Failed preparations have native IDs, never source RNG.
    api->finish();api->clear_flags_on_unprepare=true;out.stop();
    check(out.progress().natural&&!out.progress().interrupted&&out.progress().returned);
}
void destruction(){
    check(RecoveredWaveOutput::retainedCount()==0);
    auto api=std::make_shared<Fake>();WAVEHDR* header=nullptr;LPSTR data=nullptr;
    {
        RecoveredWaveOutput out(api);out.play(pcm(19),22050);header=api->borrowed;data=header->lpData;
        api->reset_error=MMSYSERR_ERROR;
    }
    check(RecoveredWaveOutput::retainedCount()==1&&api->borrowed==header&&header->lpData==data);
    check(reinterpret_cast<std::int16_t*>(data)[0]==19&&api->opened);
    check(RecoveredWaveOutput::collectRetained()==1&&api->borrowed==header);
    api->reset_error=0;api->unprepare_error=WAVERR_STILLPLAYING;
    check(RecoveredWaveOutput::collectRetained()==1&&api->borrowed==header);
    api->unprepare_error=0;api->close_error=MMSYSERR_ERROR;
    check(RecoveredWaveOutput::collectRetained()==1&&!api->borrowed&&api->opened);
    api->close_error=0;check(RecoveredWaveOutput::collectRetained()==0&&!api->opened);
    std::weak_ptr<Fake> weak;
    {
        auto held=std::make_shared<Fake>();weak=held;RecoveredWaveOutput out(held);
        out.play(pcm(23),22050);held->reset_error=MMSYSERR_ERROR;
    }
    check(!weak.expired()&&RecoveredWaveOutput::retainedCount()==1);
    weak.lock()->reset_error=0;check(RecoveredWaveOutput::collectRetained()==0&&weak.expired());
}
void delayedAndRetained(){
    auto old=std::make_shared<Fake>();
    {
        RecoveredWaveOutput out(old);out.play(pcm(29),22050);
        old->reset_returns_buffer=false;
    }
    check(RecoveredWaveOutput::retainedCount()==1&&old->borrowed&&old->unprepares==0);
    auto api=std::make_shared<Fake>();RecoveredWaveOutput replacement(api);
    auto generation=replacement.play(pcm(37),22050);
    auto* header=api->borrowed;auto* data=header->lpData;
    check(RecoveredWaveOutput::retainedCount()==1&&replacement.isCurrentGeneration(generation));
    // An old device returning its old stable header cannot mark the new clip
    // done; CALLBACK_NULL means no queued callback can reference its owner.
    old->finish();check(replacement.isPlaying()&&!replacement.progress().returned);
    old->clear_flags_on_unprepare=true;replacement.stop();
    check(RecoveredWaveOutput::retainedCount()==0&&!old->opened&&!api->opened);
    check(old->unprepares==1&&api->unprepares==1&&api->last_unprepared.size()==10);
    (void)header;(void)data; // Both released only after their own unprepare.
    auto g2=replacement.play(pcm(41),22050);check(g2>generation);
    api->finish();api->close_error=MMSYSERR_ERROR;
    throws([&]{replacement.play(pcm(43),44100);});
    check(api->opens==2&&api->writes==2&&api->opened&&!api->borrowed);
    check(replacement.failure().operation==RecoveredWaveOperation::Close&&!replacement.isPlaying());
    api->close_error=0;replacement.play(pcm(43),44100);
    check(api->opens==3&&api->writes==3&&replacement.isPlaying());
}
}
int main(){try{reuse();failures();submission();destruction();delayedAndRetained();std::cout<<checks<<" injected WinMM generation/retirement checks passed\n";}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

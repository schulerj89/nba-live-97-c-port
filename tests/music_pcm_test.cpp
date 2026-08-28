#include "music_pcm.hpp"
#include <array>
#include <cstdint>
#include <iostream>

void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(){try {
    std::vector<std::int16_t> source(65536);
    for(int i=0;i<65536;++i)source[i]=static_cast<std::int16_t>(i-32768);
    const auto original=source;
    std::vector<std::int16_t> output(source.size());
    for(unsigned gain=0;gain<256;++gain){
        std::size_t position=0;
        nba97::fillMusicPcm(source,position,output.data(),output.size(),gain);
        require(position==0,"loop cursor did not wrap");
        for(std::size_t i=0;i<source.size();++i)
            require(output[i]==static_cast<int>(source[i])*static_cast<int>((std::min)(gain,127u))/127,"gain sample mismatch");
    }
    require(source==original,"music gain changed immutable source");
    const std::vector<std::int16_t> loop{127,-127,254,-254,381,-381};
    std::size_t position=4;
    std::array<std::int16_t,10> chunk{};
    nba97::fillMusicPcm(loop,position,chunk.data(),chunk.size(),127);
    require(chunk==std::array<std::int16_t,10>{381,-381,127,-127,254,-254,381,-381,127,-127} && position==2,"chunk wrap/order");
    nba97::fillMusicPcm(loop,position,chunk.data(),chunk.size(),0);
    require(chunk==std::array<std::int16_t,10>{} && position==0,"muting must advance playback without changing source");
    nba97::fillMusicPcm(loop,position,chunk.data(),2,64);
    require(chunk[0]==64 && chunk[1]==-64 && position==2,"unmute cursor/gain");
    for(int invalid=0;invalid<4;++invalid){
        bool refused=false;std::size_t at=invalid==0?1:invalid==1?6:0;
        try{nba97::fillMusicPcm(invalid==2?std::vector<std::int16_t>{}:loop,at,chunk.data(),invalid==3?3:2,127);}
        catch(const std::invalid_argument&){refused=true;}
        require(refused,"invalid stereo extent accepted");
    }
    std::cout<<"MUSIC PCM PASS 256 gains x 65536 samples; immutable stereo source; wrap/mute/unmute cursor; invalid extents\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"MUSIC PCM FAIL "<<e.what()<<'\n';return 1;}}

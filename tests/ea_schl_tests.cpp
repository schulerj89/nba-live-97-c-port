#include "ea_schl.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using Bytes=std::vector<std::uint8_t>;
void require(bool value,const char* why) {if(!value)throw std::runtime_error(why);}
void word(Bytes& data,std::size_t at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)data.at(at+i)=std::uint8_t(value>>(i*8));
}
void tag(Bytes& data,std::size_t at,const char* value) {std::copy_n(value,4,data.begin()+at);}
Bytes fixture() {
    Bytes data(128+48*2+8,0);
    tag(data,0,"SCHl");word(data,4,128);tag(data,12,"PATl");tag(data,68,"TMxl");
    data[73]=16;data[74]=2;data[75]=6;word(data,78,44100);word(data,84,56);
    for(unsigned i=0;i<2;++i) {tag(data,128+i*48,"SCDl");word(data,132+i*48,48);}
    // First28 frames are exact left-4096/right4096; second block uses predictor1
    // with zero residual. Its nonzero start proves history crosses SCDl chunks.
    std::fill_n(data.begin()+146,14,std::uint8_t(0xff));
    std::fill_n(data.begin()+162,14,std::uint8_t(0x11));
    data[192]=data[208]=0x10;
    tag(data,224,"SCEl");word(data,228,8);return data;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--inspect") {
            const auto pcm=nba97::loadEaSchl(argv[2]);
            std::cout<<"{\"sample_rate\":"<<pcm.info.sample_rate<<",\"sample_frames\":"<<
                pcm.info.sample_count<<",\"channels\":"<<unsigned(pcm.info.channels)<<
                ",\"data_blocks\":"<<pcm.info.data_blocks<<",\"decoded_samples\":"<<pcm.samples.size()<<"}\n";
            return 0;
        }
        const auto data=fixture();const auto pcm=nba97::decodeEaSchl(data);
        require(pcm.info.sample_rate==44100 && pcm.info.sample_count==56 &&
                pcm.info.data_blocks==2 && pcm.samples.size()==112,"stereo metadata/count");
        for(unsigned i=0;i<28;++i)
            require(pcm.samples[i*2]==-4096 && pcm.samples[i*2+1]==4096,"channel separation/nibble sign");
        const std::int16_t expected[]={3840,3600,3375,3164,2966,2781,2607,2444};
        for(unsigned i=0;i<8;++i)require(pcm.samples[56+i*2+1]==expected[i],"inter-block predictor history");
        // Negative rounding intentionally follows arithmetic shift, not truncation.
        require(pcm.samples[56]==-3840 && pcm.samples[70]==-2444,"negative predictor rounding");
        auto trimmed=data;word(trimmed,84,29);
        const auto short_pcm=nba97::decodeEaSchl(trimmed);
        require(short_pcm.samples.size()==58 && short_pcm.samples.back()==3840,"declared frame tail trimming");
        for(std::size_t i=0;i<224;++i) {
            auto truncated=data;truncated.resize(i);
            bool refused=false;try {nba97::decodeEaSchl(truncated);}catch(const std::runtime_error&) {refused=true;}
            require(refused,"truncation before complete declared samples accepted");
        }
        for(const auto bad: {std::pair<std::size_t,unsigned>{73,8},{74,1},{75,7},{144,0x50},{144,13}}) {
            auto invalid=data;invalid[bad.first]=std::uint8_t(bad.second);
            bool refused=false;try {nba97::decodeEaSchl(invalid);}catch(const std::runtime_error&) {refused=true;}
            require(refused,"unsupported stream/frame encoding accepted");
        }
        require(nba97::decodeEaSchl(data).samples==pcm.samples,"decoder history leaked across files");
        std::cout<<"EA SCHL PASS: stereo signs, cross-chunk history, arithmetic rounding, trimmed tails, malformed inputs and independent files\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

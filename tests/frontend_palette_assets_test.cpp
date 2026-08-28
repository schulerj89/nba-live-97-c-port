#include "frontend_palette_assets.hpp"
#include <iostream>
#include <stdexcept>

static void check(bool b,const char* why) { if(!b) throw std::runtime_error(why); }
int main() {
    try {
        const char* tags[]={"atlP","bosP","chaP","chiP","cleP","dalP","denP","detP","golP",
            "houP","indP","lacP","lalP","miaP","milP","minP","nwjP","nwyP","orlP","phiP",
            "phoP","porP","sacP","sanP","seaP","torP","utaP","vanP","wasP","xeaP","xweP","zc1P","zc2P"};
        std::vector<std::uint8_t> bytes{'N','9','7','P',1,0,4,0,33,0,128,0,240,0,0,0};
        auto word=[&](unsigned w){bytes.push_back(w&255);bytes.push_back(w>>8);};
        for(unsigned t=0;t<33;++t) {
            bytes.insert(bytes.end(),tags[t],tags[t]+4);
            for(unsigned i=0;i<160;++i) word((t*917+i*43)&65535);
        }
        for(unsigned strip=0;strip<4;++strip) {
            for(unsigned i=0;i<128*240;++i) bytes.push_back(i&255);
            for(unsigned i=0;i<96;++i) word((0x8000+strip*512+i*27)&65535);
        }
        nba97::FrontendPaletteAssets assets(bytes);
        Nba97FrontendPalette state{};
        check(nba97_frontend_palette_begin(&state,assets.bank(),33,0,29),"begin source bank");
        PshImage image; image.width=512;image.height=240;image.rgba.resize(512*240*4);
        auto verify=[&]() {
            assets.draw(image,state);
            for(unsigned strip=0;strip<4;++strip) for(unsigned i=0;i<128*240;++i) {
                const unsigned index=i&255;
                const auto w=index<160?state.half[strip/2].current[index]:((0x8000+strip*512+(index-160)*27)&65535);
                const auto at=((i/128)*512+strip*128+i%128)*4;
                for(unsigned c=0;c<3;++c) { const auto v=(w>>(c*5))&31;
                    check(image.rgba[at+c]==((v<<3)|(v>>2)),"index/CLUT pixel"); }
                check(image.rgba[at+3]==255,"background alpha");
            }
        };
        verify();
        check(nba97_frontend_palette_request(&state,0,3,33),"request left");
        for(unsigned i=0;i<17;++i) { check(nba97_frontend_palette_tick(&state,assets.bank(),33)==1,"left tick");verify(); }
        auto bad=[&](std::vector<std::uint8_t> b) {
            bool rejected=false;try{nba97::FrontendPaletteAssets invalid(b);}catch(const std::runtime_error&){rejected=true;}
            check(rejected,"malformed pack accepted");
        };
        for(unsigned at:{0u,4u,6u,8u,10u,12u,14u,16u,16u+29*324}) {auto b=bytes;b[at]^=1;bad(b);}
        auto short_pack=bytes;short_pack.pop_back();bad(short_pack);bytes.push_back(0);bad(bytes);
        image.width=511;bool rejected=false;try{assets.draw(image,state);}catch(const std::runtime_error&){rejected=true;}
        check(rejected,"wrong target geometry accepted");
        std::cout<<"PALETTE ASSET PASS 18 full frames; all 256 indices, fixed local colors, split teams, 17 updates; malformed guards\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}

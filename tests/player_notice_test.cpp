#include "player_notice.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool b,const char* message) {if(!b) throw std::runtime_error(message);}
std::vector<std::uint8_t> fixture() {
    return {136,0,90,0,240,0,64,1,2,0,1,'A',0,1,'B',0,1,'C',0};
}
}
int main(int argc,char** argv) {
    try {
        const auto b=fixture();const auto d=nba97::parsePlayerNotice(b);
        check(d.style==1 && d.lines.size()==3 && d.lines[2].extra_before==6,"descriptor/footer route");
        for(std::size_t n=0;n<b.size();++n) {
            bool rejected=false;
            try {nba97::parsePlayerNotice({b.begin(),b.begin()+n});}catch(const std::runtime_error&){rejected=true;}
            check(rejected,"truncated notice accepted");
        }
        for(auto offset:{0,6,7,8,9,10,13,16}) {
            auto bad=b;bad[offset]^=0x80;bool rejected=false;
            try{nba97::parsePlayerNotice(bad);}catch(const std::runtime_error&){rejected=true;}
            check(rejected,"malformed notice accepted");
        }
        std::cout<<"NOTICE PASS descriptor_footer_and_bounds\n";
        for(std::uint16_t close:{1,2,4,8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000}) {
            Nba97HelpModal m{};nba97_help_open(&m,d.rect,0x800);
            for(int i=0;i<12;++i) {nba97_help_tick(&m,0x800);check(!nba97_help_text_visible(&m),"early text");}
            nba97_help_tick(&m,0x800);check(m.phase==NBA97_HELP_WAIT_CHANGE,"13-tick growth");
            for(int i=0;i<20;++i) nba97_help_tick(&m,0x800);
            check(m.phase==NBA97_HELP_WAIT_CHANGE,"held opener closed notice");
            nba97_help_input(&m,0);
            check(nba97_help_input(&m,close)==NBA97_HELP_CLOSE_SOUND,"fresh input not accepted");
            for(int i=0;i<13;++i) nba97_help_tick(&m,close);
            check(m.phase==NBA97_HELP_RETURN_BARRIER && !nba97_help_visible(&m),"13-tick shrink");
            nba97_help_tick(&m,close);check(m.phase==NBA97_HELP_RETURN_BARRIER,"held closer leaked");
            check(nba97_help_tick(&m,0)==NBA97_HELP_RETURNED,"return barrier");
        }
        std::cout<<"NOTICE PASS fourteen_dismiss_controls_and_13_tick_barriers\n";
        Nba97HelpModal m{};nba97_help_open(&m,d.rect,0x800);
        PshImage image;image.width=512;image.height=240;image.rgba.assign(512*240*4,0);
        nba97::FrontendHelpPack::draw(image,nba97::PshFont{},d,m);
        const auto at=(110*512+246)*4;
        check(image.rgba[at]==20 && image.rgba[at+1]==10 && image.rgba[at+2]==10,"red modal corner");
        unsigned covered=0;for(std::size_t i=3;i<image.rgba.size();i+=4) covered+=image.rgba[i]!=0;
        check(covered==200,"initial panel not20x10");
        std::cout<<"NOTICE PASS red_style_not_fullscreen\n";
        std::vector<std::uint8_t> index(4+11*8,0);index[0]=10;
        check(!nba97::playerHasCoolFacts(index,0),"empty player has facts");
        for(unsigned v=0;v<5;++v) {
            index[4+(6+v)*8]=16; // Player1 logical5..9 lives in physical6..10.
            check(nba97::playerHasCoolFacts(index,1) && !nba97::playerHasCoolFacts(index,0),"five-slot grouping");
            index[4+(6+v)*8]=0;
        }
        check(!nba97::playerHasCoolFacts(index,65535),"out of index ID has facts");
        for(unsigned n:{0u,3u,5u,83u}) {
            auto bad=index;bad.resize(n);bool rejected=false;
            try{nba97::playerHasCoolFacts(bad,0);}catch(const std::runtime_error&){rejected=true;}
            check(rejected,"bad index reported absence");
        }
        std::cout<<"NOTICE PASS index_absence_vs_corruption\n";
        if(argc==2) {
            const std::filesystem::path root=argv[1];
            const auto original=nba97::loadPlayerNotice(root/"player/no-facts.n97ui");
            const auto font=nba97::load_psh_font(root/"fonts/ZFONT1.PSH",10,1);
            m={};nba97_help_open(&m,original.rect,0x800);
            for(int i=0;i<13;++i) nba97_help_tick(&m,0x800);
            image.rgba.assign(512*240*4,0);
            nba97::FrontendHelpPack::draw(image,font,original,m);
            auto background=original;background.lines.clear();
            PshImage expected=image;expected.rgba.assign(512*240*4,0);
            nba97::FrontendHelpPack::draw(expected,font,background,m);
            const int y[]{105,117,135};
            for(unsigned i=0;i<3;++i) {
                check(font.textWidth(original.lines[i].encoded)<=240,"text exceeds original panel");
                nba97::draw_psh_text_centered(expected,font,original.lines[i].encoded,256,y[i]);
            }
            check(image.rgba==expected.rgba,"source row/footer coordinates differ");
            std::cout<<"NOTICE PASS private_font_rows_105_117_135\n";
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<"NOTICE FAIL "<<e.what()<<'\n';return 1;}
}

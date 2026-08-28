#include "roster_reset_assets.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
static void check(bool good,const char* why) {if(!good) throw std::runtime_error(why);}
int main(int argc,char** argv) {
    try {
        std::array<std::uint16_t,535> base{},work{};
        for(unsigned i=0;i<base.size();++i) base[i]=std::uint16_t(i);
        work=base;
        check(!nba97_reset_enabled(work.data(),base.data(),0,0),"defaults enabled");
        for(unsigned i=0;i<base.size();++i) {
            work[i]^=1;
            check(nba97_reset_enabled(work.data(),base.data(),0,1),"missed slot difference");
            check(!nba97_reset_enabled(work.data(),base.data(),1,1),"special override ignored");
            check(nba97_reset_enabled(work.data(),base.data(),1,0),"wrong special predicate");
            check(nba97_reset_enabled(work.data(),base.data(),1,-1),"signed context ignored");
            work[i]^=1;
        }
        check(!nba97_reset_enabled(nullptr,base.data(),0,0),"null table enabled");
        std::cout<<"RESET PASS all535 slot differences; exact two-byte special gate; restored defaults locked\n";
        for(int pref=0;pref<2;++pref) {
            Nba97ResetPrompt p{};
            check(nba97_reset_open(&p,{121,75,270,110},0x800,pref)==NBA97_RESET_OPEN,"open");
            check(p.choice==(pref ? 0:1),"preference default");
            check(!nba97_reset_open(&p,{121,75,270,110},0x800,pref),"double open");
            for(int i=0;i<32;++i) nba97_reset_tick(&p,0x800);
            check(p.modal.phase==NBA97_HELP_WAIT_CHANGE,"held opener accepted");
            check(p.tint[p.choice].rgb[0]==120 && p.tint[p.choice].rgb[2]==0,"initial gold hold missing");
            check(p.tint[1-p.choice].rgb[2]==128,"unselected text tinted");
            nba97_reset_tick(&p,0);
            for(auto mask:{0x80,0x100,4,8,0x801}) {
                nba97_reset_input(&p,std::uint16_t(mask));
                check(p.modal.phase==NBA97_HELP_READY,"unrelated input dismissed");
                for(int i=0;i<8;++i) nba97_reset_tick(&p,0);
            }
            nba97_reset_input(&p,1); // focus restore; endpoint produces no sound
            check(p.choice==0,"up focus");
            check(p.initial_choice==(pref ? 0:1),"focus changed fixed text layout");
            check(!nba97_reset_input(&p,0x800),"eight-update throttle bypassed");
            for(int i=0;i<8;++i) nba97_reset_tick(&p,0);
            check(nba97_reset_input(&p,2)==NBA97_RESET_DOWN,"down sound event");
            for(int i=0;i<8;++i) nba97_reset_tick(&p,0);
            check(nba97_reset_input(&p,2)==0,"endpoint made sound");
            for(int i=0;i<8;++i) nba97_reset_tick(&p,0);
            if(pref) {
                check(nba97_reset_input(&p,1)==NBA97_RESET_UP,"up sound event");
                for(int i=0;i<8;++i) nba97_reset_tick(&p,0);
            }
            check(nba97_reset_input(&p,0x800)==NBA97_RESET_CHOSEN,"choice");
            check(!nba97_help_text_visible(&p.modal),"text retained during close");
            for(int i=0;i<32;++i) check(!(nba97_reset_tick(&p,0x800)&NBA97_RESET_RETURN),"held confirm leaked");
            check(p.modal.phase==NBA97_HELP_RETURN_BARRIER,"no return barrier");
            check(nba97_reset_tick(&p,0)==NBA97_RESET_RETURN,"return");
            check(p.choice==(pref ? 0:1),"result changed");
        }
        std::cout<<"RESET PASS Cancel/restore; open/return barriers; ignored inputs; throttled focus; close events\n";
        std::vector<std::uint8_t> b{121,0,75,0,14,1,110,1,5,2};
        for(int i=0;i<7;++i) {b.push_back(1);b.push_back('A'+i);b.push_back(0);}
        nba97::RosterResetAssets valid(b);
        check(valid.rect().width==270,"descriptor geometry");
        unsigned rejected=0;
        for(std::size_t i=0;i<b.size();++i) {
            try {nba97::RosterResetAssets invalid(std::vector<std::uint8_t>(b.begin(),b.begin()+i));}
            catch(const std::exception&) {++rejected;}
        }
        check(rejected==b.size(),"accepted truncation");
        b.push_back(0);
        bool trailing=false;try {nba97::RosterResetAssets invalid(b);} catch(const std::exception&) {trailing=true;}
        check(trailing,"accepted trailing data");
        std::cout<<"RESET PASS bounded private descriptor parser; every truncation rejected\n";
        if(argc==2) {
            nba97::RosterResetAssets private_assets{std::filesystem::path(argv[1])};
            Nba97ResetPrompt p{};nba97_reset_open(&p,private_assets.rect(),0x800,0);
            for(int i=0;i<32;++i) nba97_reset_tick(&p,0);
            PshImage im;im.width=512;im.height=240;im.rgba.resize(512*240*4,0);
            private_assets.draw(im,p,20);
            check(im.rgba[(76*512+122)*4]>im.rgba[(76*512+122)*4+1],"warning is not red");
            std::cout<<"RESET PASS private descriptor + original ZFONT1 render\n";
        }
        return 0;
    } catch(const std::exception& e) {std::cerr<<"RESET FAIL "<<e.what()<<'\n';return 1;}
}

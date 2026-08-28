#include "recovered/frontend_palette.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

static void check(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
static unsigned reference(unsigned a,unsigned b,unsigned mask,unsigned k) {
    // Independent instruction-shaped oracle: negative-product correction,
    // floor division simulates SRA without implementation-defined signed shifts.
    const int old=static_cast<int>(a&mask);
    int product=(static_cast<int>(b&mask)-old)*static_cast<int>(k);
    if(product<0) product+=15;
    const int shifted=product>=0 ? product/16 : -((-product+15)/16);
    return static_cast<unsigned>(old+shifted)&mask;
}
int main() {
    try {
        unsigned cases=0;
        for(unsigned mask:{0x001fu,0x03e0u,0x3c00u}) {
            const unsigned shift=mask==0x001f?0:mask==0x03e0?5:10;
            // Include both values of the discarded blue high bit and STP.
            for(unsigned a=0;a<32;++a) for(unsigned b=0;b<32;++b)
                for(unsigned k=0;k<=16;++k) for(unsigned stp=0;stp<4;++stp) {
                    const auto from=static_cast<uint16_t>((a<<shift)|((stp&1)?0x8000:0));
                    const auto to=static_cast<uint16_t>((b<<shift)|((stp&2)?0x8000:0));
                    const auto expected=reference(from,to,mask,k)|(to&0x8000);
                    check(nba97_frontend_palette_blend(from,to,k)==expected,"masked interpolation");
                    ++cases;
                }
        }
        check(nba97_frontend_palette_blend(1,0,1)==1,"red negative rounding");
        check(nba97_frontend_palette_blend(32,0,1)==0,"green masked-word rounding");
        check(nba97_frontend_palette_blend(0x7fff,0xffff,16)==0xbfff,"blue mask and target STP");
        check(nba97_frontend_palette_blend(0,0xffff,999)==0xbfff,"native factor bound");
        std::cout<<"PALETTE PASS arithmetic cases="<<cases<<" source=8002FF40 masks=001F/03E0/3C00\n";

        std::array<uint16_t,4*160> bank{};
        for(unsigned t=0;t<4;++t) for(unsigned i=0;i<160;++i)
            bank[t*160+i]=static_cast<uint16_t>(((t*773+i*211)&0x7fff)|((i&1)?0x8000:0));
        Nba97FrontendPalette s{};
        check(nba97_frontend_palette_begin(&s,bank.data(),4,0,1),"begin");
        const auto initial=s;
        check(nba97_frontend_palette_tick(&s,bank.data(),4)==0,"settled initial state");
        check(nba97_frontend_palette_request(&s,0,2,4),"request");
        for(unsigned k=0;k<=7;++k) {
            check(nba97_frontend_palette_tick(&s,bank.data(),4)==1,"left-only tick");
            for(unsigned i=0;i<160;++i) check(s.half[0].current[i]==
                nba97_frontend_palette_blend(bank[i],bank[320+i],k),"logical factor");
            check(!std::memcmp(&s.half[1],&initial.half[1],sizeof(s.half[1])),"inactive half changed");
        }
        auto interrupted=s;
        check(nba97_frontend_palette_request(&s,0,2,4) && !std::memcmp(&s,&interrupted,sizeof(s)),"same target must not restart");
        check(nba97_frontend_palette_request(&s,0,3,4),"interrupt");
        check(!std::memcmp(s.half[0].from,interrupted.half[0].current,sizeof(s.half[0].from)),"interrupt from live palette");
        check(nba97_frontend_palette_request(&s,1,2,4),"right request");
        for(unsigned k=0;k<=16;++k) {
            check(nba97_frontend_palette_tick(&s,bank.data(),4)==3,"both tick");
            for(unsigned i=0;i<160;++i) {
                check(s.half[0].current[i]==nba97_frontend_palette_blend(interrupted.half[0].current[i],bank[480+i],k),"interrupt factor");
                check(s.half[1].current[i]==nba97_frontend_palette_blend(bank[160+i],bank[320+i],k),"right factor");
            }
        }
        auto settled=s;
        check(nba97_frontend_palette_tick(&s,bank.data(),4)==0 && !std::memcmp(&s,&settled,sizeof(s)),"17 updates only");
        check(!nba97_frontend_palette_request(&s,2,1,4) && !nba97_frontend_palette_request(&s,0,4,4) &&
            !nba97_frontend_palette_begin(&s,bank.data(),4,4,0) &&
            nba97_frontend_palette_tick(&s,nullptr,4)==-1 && !std::memcmp(&s,&settled,sizeof(s)),"atomic guards");
        std::cout<<"PALETTE PASS independent_halves_interruption_17_factors_guards state_bytes="<<sizeof(s)
                 <<"; arithmetic/controller suite, not an original recording comparison\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

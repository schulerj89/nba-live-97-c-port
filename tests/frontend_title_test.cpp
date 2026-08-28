#include "recovered/frontend_title.h"
#include "frontend_title.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok, const char* why) { if(!ok) throw std::runtime_error(why); }
// Independent transcription of the original branch/delay-slot arithmetic.
std::uint16_t originalRandom(std::uint16_t old) {
    std::uint32_t r2=old;
    if(!r2) r2=0xa5a5;
    const auto r4=r2<<1;
    r2<<=17;
    const auto r3=(r2&0x80000000u) ? (r4^0x1d87u) : r4;
    return static_cast<std::uint16_t>(r3&0xffffu);
}
}
int main() {
    try {
        for(unsigned n=0;n<65536;++n) {
            auto state=static_cast<std::uint16_t>(n);
            check(nba97_frontend_random(&state)==originalRandom(n) && state==originalRandom(n), "RNG branch/return mismatch");
        }
        std::cout<<"TITLE PASS rng_all_65536_seeds_bit14_and_zero_fallback\n";
        const int16_t bases[2][8]={{156,10,360,10,156,48,360,48},{40,18,261,18,40,51,261,51}};
        for(unsigned count=1;count<=2;++count) for(unsigned seed=0;seed<65536;++seed) {
            Nba97TitleMotion s{};check(nba97_title_init(&s,bases,count),"init");
            auto rng=static_cast<uint16_t>(seed), expected=rng;
            for(unsigned tick=0;tick<4;++tick) {
                const auto before=s;const unsigned object=tick&1;
                const auto changed=nba97_title_step(&s,&rng);
                check(s.next==((tick+1)&1),"alternating object phase");
                if(object>=count) {
                    check(changed==-1 && rng==expected && !std::memcmp(before.current,s.current,sizeof(s.current)),"absent slot changed state or RNG");
                } else {
                    check(changed==int(object),"wrong object");
                    for(unsigned i=0;i<8;++i) {
                        expected=originalRandom(expected);
                        check(s.current[object][i]==bases[object][i]+(expected&3),"corner draw/order/base mismatch");
                    }
                    check(!std::memcmp(before.current[object^1],s.current[object^1],sizeof(s.current[0])),"other object mutated");
                    check(rng==expected,"wrong RNG consumption");
                }
            }
        }
        std::cout<<"TITLE PASS both_slot_counts_all_seeds_alternation_draw_order_and_no_drift\n";
        for(int coordinate=-32768;coordinate<=32767;++coordinate) {
            int16_t base[2][8]{};for(auto& v:base[0])v=static_cast<int16_t>(coordinate);
            for(unsigned seed=1;seed<=4;++seed) {
                Nba97TitleMotion s{};nba97_title_init(&s,base,1);
                auto rng=static_cast<uint16_t>(seed);nba97_title_step(&s,&rng);
                auto expected=static_cast<uint16_t>(seed);
                for(unsigned i=0;i<8;++i) {
                    expected=originalRandom(expected);
                    const auto bits=static_cast<uint16_t>(coordinate+(expected&3));
                    const int value=bits<32768 ? bits : int(bits)-65536;
                    check(s.current[0][i]==value,"signed halfword wrap");
                }
            }
        }
        std::cout<<"TITLE PASS all_halfword_coordinates_and_wrap\n";
        Nba97TitleMotion s{};nba97_title_init(&s,bases,1);uint16_t rng=0;
        auto before=s;auto expected=originalRandom(rng);
        check(nba97_title_selector_step(&s,&rng,1)==-1 && rng==expected && !std::memcmp(&s,&before,sizeof(s)),"suppression must still consume one random value");
        expected=originalRandom(expected);
        for(int i=0;i<8;++i) expected=originalRandom(expected);
        check(nba97_title_selector_step(&s,&rng,0)==0 && rng==expected,"normal selector must consume nine draws");
        expected=originalRandom(expected);
        check(nba97_title_selector_step(&s,&rng,0)==-1 && rng==expected,"empty alternate must consume only selector pre-draw");
        before=s;
        check(!nba97_title_init(&s,bases,0) && !nba97_title_init(&s,bases,3) && !std::memcmp(&s,&before,sizeof(s)),"invalid init mutation");
        std::cout<<"TITLE PASS selector_predraw_suppression_and_init_guards\n";
        nba97::FrontendTitlePresentation host;
        check(host.select("ba22",156,10,202,54),"select initial title");
        rng=0;host.present(rng);before=host.state();
        check(!host.select("ba22",156,10,202,54) && !std::memcmp(&host.state(),&before,sizeof(before)),"repaint must not reset title");
        check(host.select("ba41",40,18,221,42) && host.state().next==1,"child must preserve global alternating phase");
        before=host.state();const auto prior=rng;
        check(host.present(rng)==-1 && rng==originalRandom(prior) && !std::memcmp(before.current,host.state().current,sizeof(before.current)),"child absent-slot hold");
        host.leave();check(!host.active(),"leave");
        check(host.select("ba22",156,10,202,54) && host.state().next==0,"return phase");
        std::cout<<"TITLE PASS host_repaint_child_return_and_global_phase\n";
        for(const auto width:{1,127,128,129,167,202,221,255}) {
            PshImage texture;texture.width=width;texture.height=42;
            texture.rgba.resize(width*42*4);
            for(int y=0;y<42;++y) for(int x=0;x<width;++x) {
                const auto at=(y*width+x)*4;
                texture.rgba[at]=x;texture.rgba[at+1]=y;texture.rgba[at+2]=17;
                texture.rgba[at+3]=(x+y)%7 ? 255 : 0;
            }
            PshImage frame;frame.width=512;frame.height=240;frame.rgba.assign(512*240*4,33);
            const int16_t corners[8]={40,18,static_cast<int16_t>(40+width),18,40,60,static_cast<int16_t>(40+width),60};
            nba97::drawFrontendTitle(frame,texture,corners);
            for(int y=0;y<240;++y) for(int x=0;x<512;++x) {
                const bool inside=x>=40 && x<40+width && y>=18 && y<60;
                const auto dst=(y*512+x)*4;
                if(inside && texture.rgba[((y-18)*width+x-40)*4+3]) {
                    check(frame.rgba[dst]==x-40 && frame.rgba[dst+1]==y-18 && frame.rgba[dst+2]==17,"quad identity/seam/last texel");
                } else check(frame.rgba[dst]==33 && frame.rgba[dst+1]==33 && frame.rgba[dst+2]==33,"quad outside/transparent write");
            }
        }
        std::cout<<"TITLE PASS textured_quad_identity_transparency_and_128_boundary\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"TITLE FAIL "<<e.what()<<'\n';return 1;}
}

#include "recovered/cool_fact_selection.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
int count(unsigned mask) {int n=0;for(;mask;mask>>=1)n+=mask&1;return n;}
}
int main() {
    try {
        for(unsigned mask=0;mask<32;++mask) for(int prior=-1;prior<5;++prior) {
            Nba97CoolFacts s{};s.selected=static_cast<int8_t>(prior);
            const auto result=nba97_fact_refresh(&s,static_cast<uint8_t>(mask));
            check(result==(mask ? NBA97_FACT_DRAW : NBA97_FACT_NONE),"refresh result");
            for(int i=0;i<5;++i) check(s.flags[i]==((mask&(1u<<i)) ? 1 : -1),"refresh flags");
            if(!mask) {check(s.selected==-1 && !s.draw_mode,"empty group");continue;}
            for(unsigned random=0;random<256;++random) {
                auto trial=s;const auto candidate=random&7;
                const auto excluded=count(mask)==1 ? -1 : prior;
                const bool accepted=candidate<unsigned(count(mask)) && int(candidate)!=excluded;
                check(nba97_fact_offer_random(&trial,random)==(accepted ? NBA97_FACT_READY : NBA97_FACT_DRAW),"refresh draw predicate");
                check(accepted ? trial.selected==candidate : !std::memcmp(&s,&trial,sizeof(s)),"draw mutation");
            }
        }
        std::cout<<"FACT PASS all_masks_previous_values_and_256_draws\n";
        // Every -1/0/1 flag combination and every selected slot, including holes.
        for(unsigned code=0;code<243;++code) for(int selected=0;selected<5;++selected) {
            Nba97CoolFacts s{};s.selected=static_cast<int8_t>(selected);unsigned n=code;
            bool unused=false;
            for(int i=0;i<5;++i) {
                s.flags[i]=static_cast<int8_t>(int(n%3)-1);n/=3;
                if(s.flags[i]!=-1)s.available_mask|=1u<<i;
                unused|=s.flags[i]==1;
            }
            const auto before=s;const auto result=nba97_fact_prepare(&s);
            if(before.flags[selected]!=0) check(result==NBA97_FACT_READY,"ready slot changed");
            else if(unused) {
                check(result==NBA97_FACT_DRAW && s.draw_mode==2,"unused selection route");
                for(unsigned draw=0;draw<8;++draw) {
                    auto trial=s;const bool accept=draw<5 && before.flags[draw]==1;
                    check(nba97_fact_offer_random(&trial,draw)==(accept ? NBA97_FACT_READY : NBA97_FACT_DRAW),"unused predicate");
                }
            } else check(result==NBA97_FACT_DRAW && s.draw_mode==1,"depleted refill route");
        }
        std::cout<<"FACT PASS all_243_flag_states_and_unused_routes\n";
        Nba97CoolFacts s{};s.selected=-1;nba97_fact_refresh(&s,31);
        for(int cycle=0;cycle<3;++cycle) for(int wanted=4;wanted>=0;--wanted) {
            nba97_fact_prepare(&s);
            for(int draw=7;draw>=wanted;--draw) {
                const auto result=nba97_fact_offer_random(&s,draw);
                check(result==(draw==wanted ? NBA97_FACT_READY : NBA97_FACT_DRAW),"no-repeat cycle");
            }
            check(s.selected==wanted,"wrong cycle variant");nba97_fact_consume(&s);
        }
        std::cout<<"FACT PASS depletion_refill_and_no_repeat_cycle\n";
        nba97_fact_refresh(&s,1);check(s.selected==-1,"single reset");
        for(int i=0;i<3;++i) {nba97_fact_prepare(&s);nba97_fact_offer_random(&s,0);check(s.selected==0,"single repeat");nba97_fact_consume(&s);}
        // Original player320 mask=0b10011. Refresh can select slot2 even though
        // absent; later unused selection accepts slot4. Preserve both rules.
        s.selected=-1;nba97_fact_refresh(&s,19);nba97_fact_offer_random(&s,2);
        check(s.selected==2 && s.flags[2]==-1 && nba97_fact_prepare(&s)==NBA97_FACT_READY,"sparse original refresh changed");
        nba97_fact_consume(&s);nba97_fact_prepare(&s);
        check(nba97_fact_offer_random(&s,4)==NBA97_FACT_READY,"sparse unused slot lost");
        std::cout<<"FACT PASS single_variant_and_sparse_original_edge\n";
        static_assert(sizeof(Nba97CoolFacts)==9 && sizeof(Nba97FactFlash)==1,
                      "selection/flash state must remain compact");
        for(int slot=0;slot<5;++slot) for(int initial : {-1,1}) {
            s={};s.selected=static_cast<int8_t>(slot);s.flags[slot]=static_cast<int8_t>(initial);
            Nba97FactFlash flash{};
            check(nba97_fact_flash_begin(&flash)==NBA97_FACT_READY,"flash begin");
            check(nba97_fact_flash_begin(&flash)==NBA97_FACT_INVALID && flash.remaining==8,"flash restarted");
            for(int frame=0;frame<8;++frame) {
                check(nba97_fact_flash_visible(&flash)==(frame%2==0),"overlay phase");
                check(s.flags[slot]==initial,"consumed before eighth present");
                const auto result=nba97_fact_flash_presented(&flash,&s);
                check(result==(frame==7 ? NBA97_FACT_READY : NBA97_FACT_NONE),"flash completion event");
            }
            check(!flash.remaining && !nba97_fact_flash_visible(&flash) && s.flags[slot]==0,"flash tail");
            const auto after=s;
            check(nba97_fact_flash_presented(&flash,&s)==NBA97_FACT_INVALID &&
                  !std::memcmp(&after,&s,sizeof(s)),"idle tick mutated selection");
        }
        std::cout<<"FACT PASS eight_present_overlay_and_deferred_consumption\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FACT FAIL "<<e.what()<<'\n';return 1;}
}

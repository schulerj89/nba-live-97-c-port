#include "recovered/game_controllers.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

static void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}

static void check_effects(const Nba97GameControllersInput& input,
                          const Nba97GameControllersEffects& out) {
    unsigned counts[2]{};
    for(unsigned i=0;i<8;++i) {
        const auto side=input.assignment[i];
        const bool joined=side==1 || side==2;
        if(joined) ++counts[side-1];
        check(out.team_base[i]==(side==1 ? 0:side==2 ? 5:-1),"side mapping");
        check(out.selected_written[i]==!joined,"selected write ownership");
        check(out.selected[i].known==(joined ? input.previous_selected[i].known:1),"selection provenance");
        check(out.selected[i].word==(joined ? input.previous_selected[i].word:0xffff),"raw selection retention");
        check(out.controller_binding[i]==i,"controller pointer binding");
    }
    check(out.human_count[0]==counts[0] && out.human_count[1]==counts[1],"human counts");
    for(auto claim:out.player_claim) check(claim==-1,"player claim clear");
    check(out.marker==-1,"marker clear");
}

static void tests() {
    unsigned cases=0;
    Nba97GameControllersInput input{};
    Nba97GameControllersEffects output{};
    const uint16_t edge_words[8]={0,1,4,5,9,10,0x8000,0xffff};
    for(unsigned combination=0;combination<6561;++combination) {
        unsigned digits=combination;
        for(unsigned i=0;i<8;++i) {
            input.assignment[i]=uint8_t(digits%3);digits/=3;
            input.previous_selected[i].known=uint8_t((combination>>i)&1);
            input.previous_selected[i].word=input.previous_selected[i].known ? edge_words[i]:0;
        }
        const auto before=input;
        check(nba97_game_controllers_initialize(&output,&input)==1,"valid input refused");
        check_effects(input,output);
        check(!std::memcmp(&before,&input,sizeof(input)),"input modified");
        ++cases;
    }
    // Every raw halfword is retained even when it is not a valid entity ID.
    for(unsigned word=0;word<65536;++word) {
        for(unsigned i=0;i<8;++i) {
            input.assignment[i]=uint8_t(1+(i&1));
            input.previous_selected[i].known=1;
            input.previous_selected[i].word=uint16_t(word+i);
        }
        check(nba97_game_controllers_initialize(&output,&input)==1,"raw word refused");
        check_effects(input,output);
        ++cases;
    }
    for(unsigned raw=0;raw<256;++raw) {
        for(auto& assignment:input.assignment) assignment=uint8_t(raw);
        check(nba97_game_controllers_initialize(&output,&input)==1,"raw assignment refused");
        check_effects(input,output);
        ++cases;
    }
    // Joining must not resolve an unknown word; leaving then joining retains
    // the source-written FFFF, until a different owner actually chooses one.
    input={};input.assignment[3]=1;
    check(nba97_game_controllers_initialize(&output,&input)==1,"unknown lifetime refused");
    check(!output.selected[3].known && !output.selected_written[3],"join fabricated selection");
    input.assignment[3]=0;
    check(nba97_game_controllers_initialize(&output,&input)==1,"leave refused");
    input.previous_selected[3]=output.selected[3];input.assignment[3]=2;
    check(nba97_game_controllers_initialize(&output,&input)==1,"rejoin refused");
    check(output.selected[3].known && output.selected[3].word==0xffff &&
          !output.selected_written[3],"rejoin replaced retained word");

    // Reject invalid provenance atomically, including a late neutral slot.
    for(unsigned i=0;i<8;++i) for(unsigned bad=2;bad<256;++bad) {
        auto invalid=input;invalid.previous_selected[i].known=uint8_t(bad);
        const auto before=output;
        check(!nba97_game_controllers_initialize(&output,&invalid),"invalid provenance accepted");
        check(!std::memcmp(&before,&output,sizeof(output)),"invalid input changed output");
    }
    auto invalid=input;invalid.previous_selected[7].word=1;
    const auto before=output;
    check(!nba97_game_controllers_initialize(&output,&invalid),"unknown payload accepted");
    check(!nba97_game_controllers_initialize(&output,nullptr),"null input accepted");
    check(!nba97_game_controllers_initialize(nullptr,&input),"null output accepted");
    check(!std::memcmp(&before,&output,sizeof(output)),"guard changed output");

    // Byte overlap is deliberately part of this effect API, in both directions.
    alignas(Nba97GameControllersEffects) unsigned char overlap[512]{};
    for(unsigned input_offset:{0u,8u,128u}) for(unsigned output_offset:{0u,8u,128u}) {
        std::memcpy(overlap+input_offset,&input,sizeof(input));
        auto* in=reinterpret_cast<Nba97GameControllersInput*>(overlap+input_offset);
        auto* out=reinterpret_cast<Nba97GameControllersEffects*>(overlap+output_offset);
        check(nba97_game_controllers_initialize(out,in)==1,"overlap refused");
        Nba97GameControllersEffects copy;std::memcpy(&copy,out,sizeof(copy));
        check_effects(input,copy);
    }
    std::printf("GAME CONTROLLERS PASS: %u assignment/raw-word cases; provenance, lifecycle, guards and overlap\n",cases);
}

int main() {try {tests();return 0;}catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}}
